/**
 * LGX Tooling Module v2.2 — Public API
 *
 * Platform-level developer tools: performance analyzer, memory tracker.
 * The LGX equivalent of DirectX PIX tooling.
 *
 * Requires: lgx_runtime (v1.0)
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under the Apache License, Version 2.0
 */

#ifndef LGX_TOOLS_H
#define LGX_TOOLS_H

#include "lgx_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Version
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LGX_TOOLS_VERSION_MAJOR  2
#define LGX_TOOLS_VERSION_MINOR  2
#define LGX_TOOLS_VERSION_PATCH  0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque Handles
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Performance analyzer — frame stats, hot zones, bottleneck detection */
typedef struct lgx_perf_analyzer  lgx_perf_analyzer_t;

/** Memory tracker — allocation ledger, leak detection */
typedef struct lgx_mem_tracker    lgx_mem_tracker_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Frame time statistics */
typedef struct lgx_frame_stats {
    double min_ms;
    double max_ms;
    double avg_ms;
    double p95_ms;      /**< 95th percentile */
    double p99_ms;      /**< 99th percentile */
    double stddev_ms;
    uint32_t frame_count;
} lgx_frame_stats_t;

/** Hot zone entry — a named zone sorted by total time */
typedef struct lgx_hot_zone {
    char     name[64];
    double   total_ms;      /**< Cumulative time across all frames */
    double   avg_ms;        /**< Average per-call time */
    uint32_t call_count;
} lgx_hot_zone_t;

/** Leak entry — an unfreed allocation */
typedef struct lgx_leak_info {
    void*       ptr;
    size_t      size;
    char        tag[32];    /**< Developer-assigned label */
    uint64_t    alloc_id;   /**< Sequence number */
} lgx_leak_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct lgx_perf_config {
    size_t   struct_size;       /**< Must be sizeof(lgx_perf_config_t) */
    uint32_t max_frames;        /**< Frame history size (default 1000) */
    uint32_t max_zones;         /**< Max tracked zones (default 256) */
} lgx_perf_config_t;

typedef struct lgx_memtrack_config {
    size_t   struct_size;       /**< Must be sizeof(lgx_memtrack_config_t) */
    uint32_t max_allocations;   /**< Max tracked allocations (default 4096) */
} lgx_memtrack_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Performance Analyzer
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Create performance analyzer. NULL config = defaults. */
lgx_perf_analyzer_t* lgx_perf_create(const lgx_perf_config_t* config);

/** Destroy analyzer. NULL-safe. */
void lgx_perf_destroy(lgx_perf_analyzer_t* analyzer);

/** Record a frame time (milliseconds). */
lgx_result_t lgx_perf_record_frame(lgx_perf_analyzer_t* analyzer, double time_ms);

/** Record a named zone time (milliseconds). Accumulates across calls. */
lgx_result_t lgx_perf_record_zone(lgx_perf_analyzer_t* analyzer,
                                  const char* name, double time_ms);

/** Get average FPS over recorded frame history. */
double lgx_perf_get_avg_fps(lgx_perf_analyzer_t* analyzer);

/** Get frame time statistics (min/max/avg/p95/p99/stddev). */
lgx_result_t lgx_perf_get_frame_stats(lgx_perf_analyzer_t* analyzer,
                                      lgx_frame_stats_t* stats);

/** Get top N slowest zones, sorted by total time descending. Returns count. */
int lgx_perf_get_hot_zones(lgx_perf_analyzer_t* analyzer,
                           lgx_hot_zone_t* out, int max_count);

/** Dump human-readable performance report to buffer. Returns bytes written. */
int lgx_perf_dump_report(lgx_perf_analyzer_t* analyzer, char* buf, size_t max);

/** Export frame data to CSV file. */
lgx_result_t lgx_perf_export_csv(lgx_perf_analyzer_t* analyzer, const char* path);

/* ═══════════════════════════════════════════════════════════════════════════
 * Memory Tracker
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Create memory tracker. NULL config = defaults. */
lgx_mem_tracker_t* lgx_memtrack_create(const lgx_memtrack_config_t* config);

/** Destroy tracker. NULL-safe. */
void lgx_memtrack_destroy(lgx_mem_tracker_t* tracker);

/** Record an allocation (ptr, size, developer tag). */
lgx_result_t lgx_memtrack_record_alloc(lgx_mem_tracker_t* tracker,
                                       void* ptr, size_t size, const char* tag);

/** Record a free. */
lgx_result_t lgx_memtrack_record_free(lgx_mem_tracker_t* tracker, void* ptr);

/** Get current live (unfreed) bytes. */
size_t lgx_memtrack_get_active_bytes(lgx_mem_tracker_t* tracker);

/** Get peak memory usage. */
size_t lgx_memtrack_get_peak_bytes(lgx_mem_tracker_t* tracker);

/** Get total allocation count (lifetime). */
uint64_t lgx_memtrack_get_alloc_count(lgx_mem_tracker_t* tracker);

/** Find unfreed allocations (leaks). Returns count found. */
int lgx_memtrack_find_leaks(lgx_mem_tracker_t* tracker,
                            lgx_leak_info_t* out, int max_count);

/** Dump human-readable memory report to buffer. Returns bytes written. */
int lgx_memtrack_dump_report(lgx_mem_tracker_t* tracker, char* buf, size_t max);

/** Export allocation log to CSV file. */
lgx_result_t lgx_memtrack_export_csv(lgx_mem_tracker_t* tracker, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* LGX_TOOLS_H */
