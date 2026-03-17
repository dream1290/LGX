/**
 * LGX Profiling Module v1.5 — Basic Tests
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#include "lgx_profile.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [%02d] %-50s", tests_run, name); \
    } while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* Busy-wait to simulate work (more reliable than usleep for timing) */
static void busy_wait_us(int us) {
    volatile int x = 0;
    for (int i = 0; i < us * 100; i++) x += i;
    (void)x;
}

/* ═══════════════════════════════════════════════════════════════════════════ */

static void test_create_destroy_default(void) {
    TEST("Create/destroy with NULL config (defaults)");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);
    if (!ctx) { FAIL("create returned NULL"); return; }
    lgx_prof_destroy(ctx);
    PASS();
}

static void test_create_destroy_config(void) {
    TEST("Create/destroy with explicit config");
    lgx_prof_config_t cfg = {
        .struct_size = sizeof(lgx_prof_config_t),
        .enabled = true,
        .max_zones = 128,
        .max_counters = 32,
        .max_frames = 60,
    };
    lgx_prof_context_t* ctx = lgx_prof_create(&cfg);
    if (!ctx) { FAIL("create returned NULL"); return; }
    lgx_prof_destroy(ctx);
    PASS();
}

static void test_destroy_null(void) {
    TEST("Destroy NULL is safe (no-op)");
    lgx_prof_destroy(NULL);
    PASS();
}

static void test_null_params(void) {
    TEST("Functions with NULL return error/0");
    bool ok = true;
    ok = ok && (lgx_prof_frame_begin(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_prof_frame_end(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_prof_zone_begin(NULL, "x") == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_prof_zone_end(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_prof_counter_set(NULL, "x", 1.0) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_prof_get_frame_time_ms(NULL) == 0.0);
    ok = ok && (lgx_prof_get_fps(NULL) == 0.0);
    ok = ok && (lgx_prof_get_frame_count(NULL) == 0);
    if (!ok) { FAIL("unexpected result"); return; }
    PASS();
}

static void test_frame_timing(void) {
    TEST("Frame begin/end records frame time > 0");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);
    if (!ctx) { FAIL("create failed"); return; }

    lgx_prof_frame_begin(ctx);
    busy_wait_us(500);
    lgx_prof_frame_end(ctx);

    double t = lgx_prof_get_frame_time_ms(ctx);
    lgx_prof_destroy(ctx);

    if (t <= 0.0) { FAIL("frame time should be > 0"); return; }
    PASS();
}

static void test_zone_timing(void) {
    TEST("Zone begin/end records zone time > 0");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);

    lgx_prof_frame_begin(ctx);
    lgx_prof_zone_begin(ctx, "Physics");
    busy_wait_us(200);
    lgx_prof_zone_end(ctx);
    lgx_prof_frame_end(ctx);

    double t = lgx_prof_get_zone_time_ms(ctx, "Physics");
    lgx_prof_destroy(ctx);

    if (t <= 0.0) { FAIL("zone time should be > 0"); return; }
    PASS();
}

static void test_nested_zones(void) {
    TEST("Nested zones (hierarchical)");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);

    lgx_prof_frame_begin(ctx);
    lgx_prof_zone_begin(ctx, "Update");
      lgx_prof_zone_begin(ctx, "Physics");
      busy_wait_us(100);
      lgx_prof_zone_end(ctx);

      lgx_prof_zone_begin(ctx, "AI");
      busy_wait_us(100);
      lgx_prof_zone_end(ctx);
    lgx_prof_zone_end(ctx);
    lgx_prof_frame_end(ctx);

    double update = lgx_prof_get_zone_time_ms(ctx, "Update");
    double physics = lgx_prof_get_zone_time_ms(ctx, "Physics");
    double ai = lgx_prof_get_zone_time_ms(ctx, "AI");
    lgx_prof_destroy(ctx);

    if (update <= 0.0 || physics <= 0.0 || ai <= 0.0) {
        FAIL("all zones should have > 0 time"); return;
    }
    if (update < physics || update < ai) {
        FAIL("parent should be >= children"); return;
    }
    PASS();
}

static void test_counters(void) {
    TEST("Counter set/add/get");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);

    lgx_prof_counter_set(ctx, "draw_calls", 42.0);
    lgx_prof_counter_add(ctx, "draw_calls", 8.0);
    double v = lgx_prof_get_counter(ctx, "draw_calls");

    double unknown = lgx_prof_get_counter(ctx, "nonexistent");
    lgx_prof_destroy(ctx);

    if (v < 49.9 || v > 50.1) { FAIL("expected 50.0"); return; }
    if (unknown != 0.0) { FAIL("unknown counter should be 0.0"); return; }
    PASS();
}

static void test_fps(void) {
    TEST("FPS calculation after multiple frames");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);

    /* Run 10 frames */
    for (int i = 0; i < 10; i++) {
        lgx_prof_frame_begin(ctx);
        busy_wait_us(100);
        lgx_prof_frame_end(ctx);
    }

    double fps = lgx_prof_get_fps(ctx);
    uint64_t count = lgx_prof_get_frame_count(ctx);
    lgx_prof_destroy(ctx);

    if (count != 10) { FAIL("expected 10 frames"); return; }
    if (fps <= 0.0) { FAIL("FPS should be > 0"); return; }
    PASS();
}

static void test_frame_dump(void) {
    TEST("Frame dump output is non-empty");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);

    lgx_prof_frame_begin(ctx);
    lgx_prof_zone_begin(ctx, "Render");
    busy_wait_us(100);
    lgx_prof_zone_end(ctx);
    lgx_prof_counter_set(ctx, "triangles", 50000.0);
    lgx_prof_frame_end(ctx);

    char buf[1024] = {0};
    int written = lgx_prof_dump_last_frame(ctx, buf, sizeof(buf));
    lgx_prof_destroy(ctx);

    if (written <= 0) { FAIL("dump should write something"); return; }
    if (strstr(buf, "Render") == NULL) { FAIL("dump should contain zone name"); return; }
    if (strstr(buf, "triangles") == NULL) { FAIL("dump should contain counter"); return; }
    PASS();
}

static void test_disabled_profiler(void) {
    TEST("Disabled profiler (all no-ops)");
    lgx_prof_config_t cfg = {
        .struct_size = sizeof(lgx_prof_config_t),
        .enabled = false,
    };
    lgx_prof_context_t* ctx = lgx_prof_create(&cfg);
    if (!ctx) { FAIL("create failed"); return; }

    lgx_prof_frame_begin(ctx);
    lgx_prof_zone_begin(ctx, "Test");
    lgx_prof_zone_end(ctx);
    lgx_prof_counter_set(ctx, "x", 1.0);
    lgx_prof_frame_end(ctx);

    double t = lgx_prof_get_frame_time_ms(ctx);
    uint64_t count = lgx_prof_get_frame_count(ctx);
    lgx_prof_destroy(ctx);

    /* When disabled, nothing should be recorded */
    if (t != 0.0) { FAIL("disabled should have 0 frame time"); return; }
    if (count != 0) { FAIL("disabled should have 0 frames"); return; }
    PASS();
}

static void test_zone_overflow(void) {
    TEST("Zone overflow returns error");
    lgx_prof_config_t cfg = {
        .struct_size = sizeof(lgx_prof_config_t),
        .enabled = true,
        .max_zones = 4,
    };
    lgx_prof_context_t* ctx = lgx_prof_create(&cfg);

    lgx_prof_frame_begin(ctx);
    lgx_result_t r1 = lgx_prof_zone_begin(ctx, "z1");
    lgx_prof_zone_end(ctx);
    lgx_result_t r2 = lgx_prof_zone_begin(ctx, "z2");
    lgx_prof_zone_end(ctx);
    lgx_result_t r3 = lgx_prof_zone_begin(ctx, "z3");
    lgx_prof_zone_end(ctx);
    lgx_result_t r4 = lgx_prof_zone_begin(ctx, "z4");
    lgx_prof_zone_end(ctx);
    lgx_result_t r5 = lgx_prof_zone_begin(ctx, "z5");  /* Should fail */
    lgx_prof_frame_end(ctx);

    lgx_prof_destroy(ctx);

    if (r1 != LGX_SUCCESS || r2 != LGX_SUCCESS || r3 != LGX_SUCCESS || r4 != LGX_SUCCESS) {
        FAIL("first 4 should succeed"); return;
    }
    if (r5 != LGX_ERROR_INVALID_PARAM) {
        FAIL("5th zone should overflow"); return;
    }
    PASS();
}

static void test_zone_underflow(void) {
    TEST("Zone end without begin returns error");
    lgx_prof_context_t* ctx = lgx_prof_create(NULL);

    lgx_prof_frame_begin(ctx);
    lgx_result_t r = lgx_prof_zone_end(ctx);  /* No matching begin */
    lgx_prof_frame_end(ctx);
    lgx_prof_destroy(ctx);

    if (r != LGX_ERROR_INVALID_PARAM) {
        FAIL("should return error on underflow"); return;
    }
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  LGX Profiling Module v1.5 — Basic Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    lgx_runtime_init(NULL);

    test_create_destroy_default();
    test_create_destroy_config();
    test_destroy_null();
    test_null_params();
    test_frame_timing();
    test_zone_timing();
    test_nested_zones();
    test_counters();
    test_fps();
    test_frame_dump();
    test_disabled_profiler();
    test_zone_overflow();
    test_zone_underflow();

    lgx_runtime_shutdown();

    printf("\n───────────────────────────────────────────────────────────\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
