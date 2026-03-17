/**
 * LGX Profiling Module v1.5 — Core Implementation
 * Frame profiler with hierarchical zones, counters, and frame history.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#define _GNU_SOURCE
#include "lgx_profile.h"
#include "lgx_runtime.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants & Defaults
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DEFAULT_MAX_ZONES    256
#define DEFAULT_MAX_COUNTERS 64
#define DEFAULT_MAX_FRAMES   120

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Zone Entry
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char* name;
    uint64_t    start_ns;
    uint64_t    duration_ns;
    uint32_t    depth;
    bool        closed;
} zone_entry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Counter Entry
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char    name[64];
    double  value;
    bool    used;
} counter_entry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Frame Record
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    double       frame_time_ms;
    zone_entry_t* zones;
    uint32_t     zone_count;
    uint32_t     max_zones;
} frame_record_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal: Context
 * ═══════════════════════════════════════════════════════════════════════════ */

struct lgx_prof_context {
    bool     enabled;

    /* Current frame */
    uint64_t     frame_start_ns;
    zone_entry_t* zones;         /* Current frame zones */
    uint32_t     zone_count;
    uint32_t     max_zones;
    uint32_t     zone_stack[64]; /* Stack of open zone indices */
    uint32_t     zone_depth;

    /* Counters */
    counter_entry_t* counters;
    uint32_t         max_counters;

    /* Frame history (ring buffer) */
    frame_record_t*  frames;
    uint32_t         max_frames;
    uint32_t         frame_head;
    uint32_t         frame_stored;
    uint64_t         total_frames;

    /* Last completed frame */
    double           last_frame_ms;
    zone_entry_t*    last_zones;
    uint32_t         last_zone_count;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Time Helper
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint64_t time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static double ns_to_ms(uint64_t ns) {
    return (double)ns / 1000000.0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Counter Lookup (simple linear search — counters are few)
 * ═══════════════════════════════════════════════════════════════════════════ */

static counter_entry_t* find_counter(lgx_prof_context_t* ctx, const char* name, bool create) {
    for (uint32_t i = 0; i < ctx->max_counters; i++) {
        if (ctx->counters[i].used && strcmp(ctx->counters[i].name, name) == 0)
            return &ctx->counters[i];
    }
    if (!create) return NULL;

    /* Find empty slot */
    for (uint32_t i = 0; i < ctx->max_counters; i++) {
        if (!ctx->counters[i].used) {
            ctx->counters[i].used = true;
            strncpy(ctx->counters[i].name, name, sizeof(ctx->counters[i].name) - 1);
            ctx->counters[i].value = 0.0;
            return &ctx->counters[i];
        }
    }
    return NULL;  /* Full */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Context Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_prof_context_t* lgx_prof_create(const lgx_prof_config_t* config) {
    lgx_prof_context_t* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->enabled      = config ? config->enabled : true;
    ctx->max_zones    = (config && config->max_zones > 0) ? config->max_zones : DEFAULT_MAX_ZONES;
    ctx->max_counters = (config && config->max_counters > 0) ? config->max_counters : DEFAULT_MAX_COUNTERS;
    ctx->max_frames   = (config && config->max_frames > 0) ? config->max_frames : DEFAULT_MAX_FRAMES;

    /* Current frame zones */
    ctx->zones = calloc(ctx->max_zones, sizeof(zone_entry_t));
    if (!ctx->zones) { free(ctx); return NULL; }

    /* Last frame zones (copy) */
    ctx->last_zones = calloc(ctx->max_zones, sizeof(zone_entry_t));
    if (!ctx->last_zones) { free(ctx->zones); free(ctx); return NULL; }

    /* Counters */
    ctx->counters = calloc(ctx->max_counters, sizeof(counter_entry_t));
    if (!ctx->counters) { free(ctx->last_zones); free(ctx->zones); free(ctx); return NULL; }

    /* Frame history */
    ctx->frames = calloc(ctx->max_frames, sizeof(frame_record_t));
    if (!ctx->frames) { free(ctx->counters); free(ctx->last_zones); free(ctx->zones); free(ctx); return NULL; }

    /* Pre-allocate zone storage for each frame record */
    for (uint32_t i = 0; i < ctx->max_frames; i++) {
        ctx->frames[i].zones = calloc(ctx->max_zones, sizeof(zone_entry_t));
        ctx->frames[i].max_zones = ctx->max_zones;
    }

    printf("[LGX PROFILE] Context created: %s, %u zones, %u counters, %u frame history\n",
           ctx->enabled ? "enabled" : "disabled", ctx->max_zones, ctx->max_counters, ctx->max_frames);
    return ctx;
}

void lgx_prof_destroy(lgx_prof_context_t* ctx) {
    if (!ctx) return;

    for (uint32_t i = 0; i < ctx->max_frames; i++) {
        free(ctx->frames[i].zones);
    }
    free(ctx->frames);
    free(ctx->counters);
    free(ctx->last_zones);
    free(ctx->zones);
    free(ctx);
    printf("[LGX PROFILE] Context destroyed\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Frame Boundary
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_prof_frame_begin(lgx_prof_context_t* ctx) {
    if (!ctx) return LGX_ERROR_INVALID_PARAM;
    if (!ctx->enabled) return LGX_SUCCESS;

    ctx->frame_start_ns = time_now_ns();
    ctx->zone_count = 0;
    ctx->zone_depth = 0;

    return LGX_SUCCESS;
}

lgx_result_t lgx_prof_frame_end(lgx_prof_context_t* ctx) {
    if (!ctx) return LGX_ERROR_INVALID_PARAM;
    if (!ctx->enabled) return LGX_SUCCESS;

    uint64_t end_ns = time_now_ns();
    uint64_t frame_ns = end_ns - ctx->frame_start_ns;
    ctx->last_frame_ms = ns_to_ms(frame_ns);

    /* Copy zones to last_zones */
    ctx->last_zone_count = ctx->zone_count;
    memcpy(ctx->last_zones, ctx->zones, ctx->zone_count * sizeof(zone_entry_t));

    /* Store in frame history ring buffer */
    frame_record_t* rec = &ctx->frames[ctx->frame_head];
    rec->frame_time_ms = ctx->last_frame_ms;
    rec->zone_count = ctx->zone_count;
    memcpy(rec->zones, ctx->zones, ctx->zone_count * sizeof(zone_entry_t));

    ctx->frame_head = (ctx->frame_head + 1) % ctx->max_frames;
    if (ctx->frame_stored < ctx->max_frames) ctx->frame_stored++;
    ctx->total_frames++;

    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Zones
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_prof_zone_begin(lgx_prof_context_t* ctx, const char* name) {
    if (!ctx || !name) return LGX_ERROR_INVALID_PARAM;
    if (!ctx->enabled) return LGX_SUCCESS;
    if (ctx->zone_count >= ctx->max_zones) return LGX_ERROR_INVALID_PARAM;
    if (ctx->zone_depth >= 64) return LGX_ERROR_INVALID_PARAM;  /* Stack overflow */

    uint32_t idx = ctx->zone_count++;
    zone_entry_t* z = &ctx->zones[idx];
    z->name = name;
    z->start_ns = time_now_ns();
    z->duration_ns = 0;
    z->depth = ctx->zone_depth;
    z->closed = false;

    ctx->zone_stack[ctx->zone_depth++] = idx;

    return LGX_SUCCESS;
}

lgx_result_t lgx_prof_zone_end(lgx_prof_context_t* ctx) {
    if (!ctx) return LGX_ERROR_INVALID_PARAM;
    if (!ctx->enabled) return LGX_SUCCESS;
    if (ctx->zone_depth == 0) return LGX_ERROR_INVALID_PARAM;  /* Underflow */

    uint32_t idx = ctx->zone_stack[--ctx->zone_depth];
    zone_entry_t* z = &ctx->zones[idx];
    z->duration_ns = time_now_ns() - z->start_ns;
    z->closed = true;

    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Counters
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_prof_counter_set(lgx_prof_context_t* ctx, const char* name, double value) {
    if (!ctx || !name) return LGX_ERROR_INVALID_PARAM;
    if (!ctx->enabled) return LGX_SUCCESS;

    counter_entry_t* c = find_counter(ctx, name, true);
    if (!c) return LGX_ERROR_INVALID_PARAM;
    c->value = value;
    return LGX_SUCCESS;
}

lgx_result_t lgx_prof_counter_add(lgx_prof_context_t* ctx, const char* name, double delta) {
    if (!ctx || !name) return LGX_ERROR_INVALID_PARAM;
    if (!ctx->enabled) return LGX_SUCCESS;

    counter_entry_t* c = find_counter(ctx, name, true);
    if (!c) return LGX_ERROR_INVALID_PARAM;
    c->value += delta;
    return LGX_SUCCESS;
}

double lgx_prof_get_counter(lgx_prof_context_t* ctx, const char* name) {
    if (!ctx || !name) return 0.0;
    counter_entry_t* c = find_counter(ctx, name, false);
    return c ? c->value : 0.0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Query
 * ═══════════════════════════════════════════════════════════════════════════ */

double lgx_prof_get_frame_time_ms(lgx_prof_context_t* ctx) {
    if (!ctx) return 0.0;
    return ctx->last_frame_ms;
}

double lgx_prof_get_zone_time_ms(lgx_prof_context_t* ctx, const char* name) {
    if (!ctx || !name) return 0.0;

    for (uint32_t i = 0; i < ctx->last_zone_count; i++) {
        if (ctx->last_zones[i].name && strcmp(ctx->last_zones[i].name, name) == 0) {
            return ns_to_ms(ctx->last_zones[i].duration_ns);
        }
    }
    return 0.0;
}

double lgx_prof_get_fps(lgx_prof_context_t* ctx) {
    if (!ctx || ctx->frame_stored == 0) return 0.0;

    double total_ms = 0.0;
    uint32_t count = ctx->frame_stored;
    uint32_t start = (ctx->frame_head + ctx->max_frames - count) % ctx->max_frames;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start + i) % ctx->max_frames;
        total_ms += ctx->frames[idx].frame_time_ms;
    }

    if (total_ms < 0.001) return 0.0;
    return (double)count / (total_ms / 1000.0);
}

uint64_t lgx_prof_get_frame_count(lgx_prof_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->total_frames;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Export
 * ═══════════════════════════════════════════════════════════════════════════ */

int lgx_prof_dump_last_frame(lgx_prof_context_t* ctx, char* buf, size_t size) {
    if (!ctx || !buf || size == 0) return 0;

    int written = 0;
    int n;

    n = snprintf(buf + written, size - (size_t)written,
                 "Frame #%lu  %.3f ms  (%.0f FPS)\n",
                 (unsigned long)ctx->total_frames,
                 ctx->last_frame_ms,
                 lgx_prof_get_fps(ctx));
    if (n > 0) written += n;

    /* Zones */
    for (uint32_t i = 0; i < ctx->last_zone_count && (size_t)written < size - 1; i++) {
        zone_entry_t* z = &ctx->last_zones[i];
        /* Indent by depth */
        for (uint32_t d = 0; d < z->depth && (size_t)written < size - 1; d++) {
            n = snprintf(buf + written, size - (size_t)written, "  ");
            if (n > 0) written += n;
        }
        n = snprintf(buf + written, size - (size_t)written,
                     "%-30s %8.3f ms\n",
                     z->name ? z->name : "?",
                     ns_to_ms(z->duration_ns));
        if (n > 0) written += n;
    }

    /* Counters */
    bool has_counters = false;
    for (uint32_t i = 0; i < ctx->max_counters; i++) {
        if (ctx->counters[i].used) {
            if (!has_counters) {
                n = snprintf(buf + written, size - (size_t)written, "Counters:\n");
                if (n > 0) written += n;
                has_counters = true;
            }
            n = snprintf(buf + written, size - (size_t)written,
                         "  %-28s %10.2f\n",
                         ctx->counters[i].name, ctx->counters[i].value);
            if (n > 0) written += n;
        }
    }

    return written;
}
