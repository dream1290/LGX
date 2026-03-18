/**
 * LGX Tooling Module v2.2 — Core Implementation
 *
 * Performance analyzer (frame stats, hot zones) and memory tracker
 * (leak detection, peak usage). Platform-level developer diagnostics.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#define _GNU_SOURCE
#include "lgx_tools.h"
#include "lgx_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DEFAULT_MAX_FRAMES       1000
#define DEFAULT_MAX_ZONES        256
#define DEFAULT_MAX_ALLOCATIONS  4096

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Zone Accumulator
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char     name[64];
    double   total_ms;
    uint32_t call_count;
} zone_entry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Performance Analyzer
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_perf_analyzer {
    double*       frames;
    uint32_t      frame_count;
    uint32_t      max_frames;
    uint32_t      frame_head;

    zone_entry_t* zones;
    uint32_t      zone_count;
    uint32_t      max_zones;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Allocation Record
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void*    ptr;
    size_t   size;
    char     tag[32];
    uint64_t alloc_id;
    bool     active;
} alloc_record_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Memory Tracker
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_mem_tracker {
    alloc_record_t* records;
    uint32_t        max_records;
    uint32_t        record_count;
    size_t          active_bytes;
    size_t          peak_bytes;
    uint64_t        total_allocs;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Performance Analyzer — Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_perf_analyzer_t* lgx_perf_create(const lgx_perf_config_t* config) {
    lgx_perf_analyzer_t* a = calloc(1, sizeof(*a));
    if (!a) return NULL;

    a->max_frames = (config && config->max_frames > 0)
                    ? config->max_frames : DEFAULT_MAX_FRAMES;
    a->max_zones  = (config && config->max_zones > 0)
                    ? config->max_zones : DEFAULT_MAX_ZONES;

    a->frames = calloc(a->max_frames, sizeof(double));
    a->zones  = calloc(a->max_zones, sizeof(zone_entry_t));
    if (!a->frames || !a->zones) {
        free(a->frames); free(a->zones); free(a);
        return NULL;
    }

    printf("[LGX TOOLS] Performance analyzer created: max_frames=%u, max_zones=%u\n",
           a->max_frames, a->max_zones);
    return a;
}

void lgx_perf_destroy(lgx_perf_analyzer_t* a) {
    if (!a) return;
    free(a->frames);
    free(a->zones);
    free(a);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Performance Analyzer — Recording
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_perf_record_frame(lgx_perf_analyzer_t* a, double time_ms) {
    if (!a) return LGX_ERROR_INVALID_PARAM;

    a->frames[a->frame_head] = time_ms;
    a->frame_head = (a->frame_head + 1) % a->max_frames;
    if (a->frame_count < a->max_frames) a->frame_count++;

    return LGX_SUCCESS;
}

lgx_result_t lgx_perf_record_zone(lgx_perf_analyzer_t* a,
                                  const char* name, double time_ms) {
    if (!a || !name) return LGX_ERROR_INVALID_PARAM;

    /* Find existing zone */
    for (uint32_t i = 0; i < a->zone_count; i++) {
        if (strcmp(a->zones[i].name, name) == 0) {
            a->zones[i].total_ms += time_ms;
            a->zones[i].call_count++;
            return LGX_SUCCESS;
        }
    }

    /* New zone */
    if (a->zone_count >= a->max_zones) return LGX_ERROR_INVALID_PARAM;
    strncpy(a->zones[a->zone_count].name, name, 63);
    a->zones[a->zone_count].name[63] = '\0';
    a->zones[a->zone_count].total_ms = time_ms;
    a->zones[a->zone_count].call_count = 1;
    a->zone_count++;
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Performance Analyzer — Queries
 * ═══════════════════════════════════════════════════════════════════════════ */

double lgx_perf_get_avg_fps(lgx_perf_analyzer_t* a) {
    if (!a || a->frame_count == 0) return 0.0;

    double sum = 0.0;
    for (uint32_t i = 0; i < a->frame_count; i++) {
        sum += a->frames[i];
    }
    double avg_ms = sum / a->frame_count;
    return (avg_ms > 0.0) ? (1000.0 / avg_ms) : 0.0;
}

/* Compare for qsort */
static int cmp_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

lgx_result_t lgx_perf_get_frame_stats(lgx_perf_analyzer_t* a,
                                      lgx_frame_stats_t* stats) {
    if (!a || !stats) return LGX_ERROR_INVALID_PARAM;
    if (a->frame_count == 0) {
        memset(stats, 0, sizeof(*stats));
        return LGX_SUCCESS;
    }

    /* Copy and sort for percentile calculation */
    double* sorted = malloc(a->frame_count * sizeof(double));
    if (!sorted) return LGX_ERROR_INVALID_PARAM;
    memcpy(sorted, a->frames, a->frame_count * sizeof(double));
    qsort(sorted, a->frame_count, sizeof(double), cmp_double);

    stats->min_ms = sorted[0];
    stats->max_ms = sorted[a->frame_count - 1];
    stats->frame_count = a->frame_count;

    /* Average */
    double sum = 0.0;
    for (uint32_t i = 0; i < a->frame_count; i++) sum += sorted[i];
    stats->avg_ms = sum / a->frame_count;

    /* Percentiles */
    uint32_t p95_idx = (uint32_t)(a->frame_count * 0.95);
    uint32_t p99_idx = (uint32_t)(a->frame_count * 0.99);
    if (p95_idx >= a->frame_count) p95_idx = a->frame_count - 1;
    if (p99_idx >= a->frame_count) p99_idx = a->frame_count - 1;
    stats->p95_ms = sorted[p95_idx];
    stats->p99_ms = sorted[p99_idx];

    /* Standard deviation */
    double variance = 0.0;
    for (uint32_t i = 0; i < a->frame_count; i++) {
        double diff = sorted[i] - stats->avg_ms;
        variance += diff * diff;
    }
    stats->stddev_ms = sqrt(variance / a->frame_count);

    free(sorted);
    return LGX_SUCCESS;
}

/* Compare zones for sorting by total_ms descending */
static int cmp_zone_desc(const void* a, const void* b) {
    const lgx_hot_zone_t* za = (const lgx_hot_zone_t*)a;
    const lgx_hot_zone_t* zb = (const lgx_hot_zone_t*)b;
    if (za->total_ms > zb->total_ms) return -1;
    if (za->total_ms < zb->total_ms) return 1;
    return 0;
}

int lgx_perf_get_hot_zones(lgx_perf_analyzer_t* a,
                           lgx_hot_zone_t* out, int max_count) {
    if (!a || !out || max_count <= 0) return 0;

    int count = (int)a->zone_count;
    if (count > max_count) count = max_count;

    /* Copy zone data to output */
    for (int i = 0; i < count; i++) {
        strncpy(out[i].name, a->zones[i].name, 63);
        out[i].name[63] = '\0';
        out[i].total_ms = a->zones[i].total_ms;
        out[i].call_count = a->zones[i].call_count;
        out[i].avg_ms = (a->zones[i].call_count > 0)
                        ? a->zones[i].total_ms / a->zones[i].call_count : 0.0;
    }

    /* Sort by total time descending */
    qsort(out, (size_t)count, sizeof(lgx_hot_zone_t), cmp_zone_desc);
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Performance Analyzer — Export
 * ═══════════════════════════════════════════════════════════════════════════ */

int lgx_perf_dump_report(lgx_perf_analyzer_t* a, char* buf, size_t max) {
    if (!a || !buf || max == 0) return 0;

    lgx_frame_stats_t stats;
    lgx_perf_get_frame_stats(a, &stats);

    int len = snprintf(buf, max,
        "╔══════════════════════════════════════════════════╗\n"
        "║  LGX Performance Report                         ║\n"
        "╠══════════════════════════════════════════════════╣\n"
        "  Frames analyzed: %u\n"
        "  Avg FPS:         %.1f\n"
        "  Frame time:      %.2f ms avg, %.2f ms min, %.2f ms max\n"
        "  P95:             %.2f ms\n"
        "  P99:             %.2f ms\n"
        "  Stddev:          %.2f ms\n",
        stats.frame_count, lgx_perf_get_avg_fps(a),
        stats.avg_ms, stats.min_ms, stats.max_ms,
        stats.p95_ms, stats.p99_ms, stats.stddev_ms);

    if (a->zone_count > 0 && (size_t)len < max - 1) {
        len += snprintf(buf + len, max - (size_t)len,
            "\n  Hot Zones (by total time):\n");

        lgx_hot_zone_t zones[32];
        int zcount = lgx_perf_get_hot_zones(a, zones, 32);
        for (int i = 0; i < zcount && (size_t)len < max - 1; i++) {
            len += snprintf(buf + len, max - (size_t)len,
                "    %-30s %8.2f ms total, %8.2f ms avg (%u calls)\n",
                zones[i].name, zones[i].total_ms, zones[i].avg_ms, zones[i].call_count);
        }
    }

    if ((size_t)len < max - 1) {
        len += snprintf(buf + len, max - (size_t)len,
            "╚══════════════════════════════════════════════════╝\n");
    }

    return len;
}

lgx_result_t lgx_perf_export_csv(lgx_perf_analyzer_t* a, const char* path) {
    if (!a || !path) return LGX_ERROR_INVALID_PARAM;

    FILE* f = fopen(path, "w");
    if (!f) return LGX_ERROR_INVALID_PARAM;

    fprintf(f, "frame,time_ms\n");
    for (uint32_t i = 0; i < a->frame_count; i++) {
        uint32_t idx = (a->frame_count < a->max_frames)
                       ? i : (a->frame_head + i) % a->max_frames;
        fprintf(f, "%u,%.4f\n", i + 1, a->frames[idx]);
    }

    fclose(f);
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Memory Tracker — Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_mem_tracker_t* lgx_memtrack_create(const lgx_memtrack_config_t* config) {
    lgx_mem_tracker_t* t = calloc(1, sizeof(*t));
    if (!t) return NULL;

    t->max_records = (config && config->max_allocations > 0)
                     ? config->max_allocations : DEFAULT_MAX_ALLOCATIONS;

    t->records = calloc(t->max_records, sizeof(alloc_record_t));
    if (!t->records) { free(t); return NULL; }

    printf("[LGX TOOLS] Memory tracker created: max_allocs=%u\n", t->max_records);
    return t;
}

void lgx_memtrack_destroy(lgx_mem_tracker_t* t) {
    if (!t) return;
    free(t->records);
    free(t);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Memory Tracker — Recording
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_memtrack_record_alloc(lgx_mem_tracker_t* t,
                                       void* ptr, size_t size, const char* tag) {
    if (!t || !ptr) return LGX_ERROR_INVALID_PARAM;

    /* Find empty slot */
    for (uint32_t i = 0; i < t->max_records; i++) {
        if (!t->records[i].active) {
            t->records[i].ptr = ptr;
            t->records[i].size = size;
            t->records[i].alloc_id = t->total_allocs + 1;
            t->records[i].active = true;
            if (tag) {
                strncpy(t->records[i].tag, tag, 31);
                t->records[i].tag[31] = '\0';
            } else {
                t->records[i].tag[0] = '\0';
            }

            t->active_bytes += size;
            t->total_allocs++;
            t->record_count++;

            if (t->active_bytes > t->peak_bytes)
                t->peak_bytes = t->active_bytes;

            return LGX_SUCCESS;
        }
    }

    return LGX_ERROR_INVALID_PARAM;  /* Table full */
}

lgx_result_t lgx_memtrack_record_free(lgx_mem_tracker_t* t, void* ptr) {
    if (!t || !ptr) return LGX_ERROR_INVALID_PARAM;

    for (uint32_t i = 0; i < t->max_records; i++) {
        if (t->records[i].active && t->records[i].ptr == ptr) {
            t->active_bytes -= t->records[i].size;
            t->records[i].active = false;
            return LGX_SUCCESS;
        }
    }

    return LGX_ERROR_INVALID_PARAM;  /* Not found (double-free?) */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Memory Tracker — Queries
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t lgx_memtrack_get_active_bytes(lgx_mem_tracker_t* t) {
    if (!t) return 0;
    return t->active_bytes;
}

size_t lgx_memtrack_get_peak_bytes(lgx_mem_tracker_t* t) {
    if (!t) return 0;
    return t->peak_bytes;
}

uint64_t lgx_memtrack_get_alloc_count(lgx_mem_tracker_t* t) {
    if (!t) return 0;
    return t->total_allocs;
}

int lgx_memtrack_find_leaks(lgx_mem_tracker_t* t,
                            lgx_leak_info_t* out, int max_count) {
    if (!t || !out || max_count <= 0) return 0;

    int found = 0;
    for (uint32_t i = 0; i < t->max_records && found < max_count; i++) {
        if (t->records[i].active) {
            out[found].ptr = t->records[i].ptr;
            out[found].size = t->records[i].size;
            out[found].alloc_id = t->records[i].alloc_id;
            strncpy(out[found].tag, t->records[i].tag, 31);
            out[found].tag[31] = '\0';
            found++;
        }
    }

    return found;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Memory Tracker — Export
 * ═══════════════════════════════════════════════════════════════════════════ */

int lgx_memtrack_dump_report(lgx_mem_tracker_t* t, char* buf, size_t max) {
    if (!t || !buf || max == 0) return 0;

    /* Count leaks */
    int leak_count = 0;
    size_t leak_bytes = 0;
    for (uint32_t i = 0; i < t->max_records; i++) {
        if (t->records[i].active) {
            leak_count++;
            leak_bytes += t->records[i].size;
        }
    }

    int len = snprintf(buf, max,
        "╔══════════════════════════════════════════════════╗\n"
        "║  LGX Memory Report                              ║\n"
        "╠══════════════════════════════════════════════════╣\n"
        "  Total allocations:  %lu\n"
        "  Active bytes:       %zu\n"
        "  Peak bytes:         %zu\n"
        "  Leaks detected:     %d (%zu bytes)\n",
        (unsigned long)t->total_allocs, t->active_bytes,
        t->peak_bytes, leak_count, leak_bytes);

    if (leak_count > 0 && (size_t)len < max - 1) {
        len += snprintf(buf + len, max - (size_t)len, "\n  Leaked allocations:\n");
        for (uint32_t i = 0; i < t->max_records && (size_t)len < max - 1; i++) {
            if (t->records[i].active) {
                len += snprintf(buf + len, max - (size_t)len,
                    "    [#%lu] %p  %zu bytes  tag=\"%s\"\n",
                    (unsigned long)t->records[i].alloc_id,
                    t->records[i].ptr,
                    t->records[i].size,
                    t->records[i].tag);
            }
        }
    }

    if ((size_t)len < max - 1) {
        len += snprintf(buf + len, max - (size_t)len,
            "╚══════════════════════════════════════════════════╝\n");
    }

    return len;
}

lgx_result_t lgx_memtrack_export_csv(lgx_mem_tracker_t* t, const char* path) {
    if (!t || !path) return LGX_ERROR_INVALID_PARAM;

    FILE* f = fopen(path, "w");
    if (!f) return LGX_ERROR_INVALID_PARAM;

    fprintf(f, "alloc_id,ptr,size,tag,active\n");
    for (uint32_t i = 0; i < t->max_records; i++) {
        if (t->records[i].alloc_id > 0) {
            fprintf(f, "%lu,%p,%zu,%s,%s\n",
                    (unsigned long)t->records[i].alloc_id,
                    t->records[i].ptr,
                    t->records[i].size,
                    t->records[i].tag,
                    t->records[i].active ? "yes" : "no");
        }
    }

    fclose(f);
    return LGX_SUCCESS;
}
