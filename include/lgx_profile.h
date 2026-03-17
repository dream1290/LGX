/**
 * LGX Profiling Module v1.5 — Public API
 *
 * Frame-level profiling: hierarchical zones, performance counters,
 * frame history, FPS tracking. Zero overhead when disabled.
 *
 * Requires: lgx_runtime (v1.0)
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under the Apache License, Version 2.0
 */

#ifndef LGX_PROFILE_H
#define LGX_PROFILE_H

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

#define LGX_PROFILE_VERSION_MAJOR  1
#define LGX_PROFILE_VERSION_MINOR  5
#define LGX_PROFILE_VERSION_PATCH  0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque Handle
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Profiling context — frame state, zone stack, counters, history */
typedef struct lgx_prof_context lgx_prof_context_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct lgx_prof_config {
    size_t   struct_size;       /**< Must be sizeof(lgx_prof_config_t) */
    bool     enabled;           /**< Enable profiling (default true) */
    uint32_t max_zones;         /**< Max zones per frame (default 256) */
    uint32_t max_counters;      /**< Max named counters (default 64) */
    uint32_t max_frames;        /**< Frame history depth (default 120) */
} lgx_prof_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Zone Record (read-only, returned by query functions)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct lgx_prof_zone_info {
    const char* name;           /**< Zone name (pointer to static string) */
    double      time_ms;        /**< Duration in milliseconds */
    uint32_t    depth;          /**< Nesting depth (0 = top-level) */
} lgx_prof_zone_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Convenience Macros (zero-cost when ctx is NULL or disabled)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LGX_PROFILE_ZONE_BEGIN(ctx, name)  lgx_prof_zone_begin((ctx), (name))
#define LGX_PROFILE_ZONE_END(ctx)          lgx_prof_zone_end((ctx))
#define LGX_PROFILE_COUNTER(ctx, name, v)  lgx_prof_counter_set((ctx), (name), (v))

/* ═══════════════════════════════════════════════════════════════════════════
 * Context Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a profiling context. NULL config = defaults (enabled, 256 zones, 64 counters, 120 frames).
 */
lgx_prof_context_t* lgx_prof_create(const lgx_prof_config_t* config);

/** Destroy profiling context. NULL-safe. */
void lgx_prof_destroy(lgx_prof_context_t* ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Frame Boundary
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Mark the beginning of a frame. Resets zone stack, records timestamp. */
lgx_result_t lgx_prof_frame_begin(lgx_prof_context_t* ctx);

/** Mark the end of a frame. Records frame time, stores in history. */
lgx_result_t lgx_prof_frame_end(lgx_prof_context_t* ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Zones (Hierarchical Timing)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Begin a named zone. Zones can be nested.
 * @param name  Static string — NOT copied, must outlive the frame.
 */
lgx_result_t lgx_prof_zone_begin(lgx_prof_context_t* ctx, const char* name);

/** End the current zone. Must match a previous zone_begin. */
lgx_result_t lgx_prof_zone_end(lgx_prof_context_t* ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Counters
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Set a named counter to a value. Creates counter if it doesn't exist. */
lgx_result_t lgx_prof_counter_set(lgx_prof_context_t* ctx, const char* name, double value);

/** Add delta to a named counter. Creates counter (initial 0) if it doesn't exist. */
lgx_result_t lgx_prof_counter_add(lgx_prof_context_t* ctx, const char* name, double delta);

/** Get current value of a named counter. Returns 0.0 if not found. */
double lgx_prof_get_counter(lgx_prof_context_t* ctx, const char* name);

/* ═══════════════════════════════════════════════════════════════════════════
 * Query
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Get the last completed frame time in milliseconds. */
double lgx_prof_get_frame_time_ms(lgx_prof_context_t* ctx);

/** Get the last frame's zone time by name (ms). Returns 0.0 if not found. */
double lgx_prof_get_zone_time_ms(lgx_prof_context_t* ctx, const char* name);

/** Get rolling FPS based on frame history. */
double lgx_prof_get_fps(lgx_prof_context_t* ctx);

/** Get total frames profiled. */
uint64_t lgx_prof_get_frame_count(lgx_prof_context_t* ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Export
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Dump the last frame's profiling data as a human-readable text summary.
 *
 * @param buf   Output buffer
 * @param size  Buffer size in bytes
 * @return Number of bytes written (excluding null terminator)
 */
int lgx_prof_dump_last_frame(lgx_prof_context_t* ctx, char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* LGX_PROFILE_H */
