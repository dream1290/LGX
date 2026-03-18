/**
 * LGX Tooling Module v2.2 — Basic Tests
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#include "lgx_tools.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [%02d] %-50s", tests_run, name); \
    } while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ═══════════════════════════════════════════════════════════════════════════ */

static void test_perf_create_destroy(void) {
    TEST("Perf analyzer create/destroy (NULL config)");
    lgx_perf_analyzer_t* a = lgx_perf_create(NULL);
    if (!a) { FAIL("create returned NULL"); return; }
    lgx_perf_destroy(a);
    PASS();
}

static void test_destroy_null(void) {
    TEST("Destroy NULL is safe");
    lgx_perf_destroy(NULL);
    lgx_memtrack_destroy(NULL);
    PASS();
}

static void test_null_params(void) {
    TEST("Functions with NULL return error/0");
    bool ok = true;
    ok = ok && (lgx_perf_record_frame(NULL, 0) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_perf_record_zone(NULL, NULL, 0) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_perf_get_avg_fps(NULL) == 0.0);
    ok = ok && (lgx_perf_get_frame_stats(NULL, NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_perf_get_hot_zones(NULL, NULL, 0) == 0);
    ok = ok && (lgx_perf_dump_report(NULL, NULL, 0) == 0);
    ok = ok && (lgx_memtrack_get_active_bytes(NULL) == 0);
    ok = ok && (lgx_memtrack_get_peak_bytes(NULL) == 0);
    ok = ok && (lgx_memtrack_get_alloc_count(NULL) == 0);
    ok = ok && (lgx_memtrack_find_leaks(NULL, NULL, 0) == 0);
    if (!ok) { FAIL("unexpected result"); return; }
    PASS();
}

static void test_perf_avg_fps(void) {
    TEST("Record frames, get avg FPS");
    lgx_perf_analyzer_t* a = lgx_perf_create(NULL);

    /* 60 FPS = 16.67ms per frame */
    for (int i = 0; i < 100; i++) {
        lgx_perf_record_frame(a, 16.67);
    }

    double fps = lgx_perf_get_avg_fps(a);
    lgx_perf_destroy(a);

    if (fps < 59.0 || fps > 61.0) { FAIL("expected ~60 FPS"); return; }
    PASS();
}

static void test_perf_frame_stats(void) {
    TEST("Frame stats (min/max/avg/p95/p99)");
    lgx_perf_analyzer_t* a = lgx_perf_create(NULL);

    /* Record varying frame times */
    for (int i = 0; i < 100; i++) {
        lgx_perf_record_frame(a, 10.0 + (double)i * 0.1);
    }

    lgx_frame_stats_t stats;
    lgx_result_t r = lgx_perf_get_frame_stats(a, &stats);
    lgx_perf_destroy(a);

    if (r != LGX_SUCCESS) { FAIL("get_frame_stats failed"); return; }
    if (stats.min_ms < 9.9 || stats.min_ms > 10.1) { FAIL("min wrong"); return; }
    if (stats.max_ms < 19.8 || stats.max_ms > 20.0) { FAIL("max wrong"); return; }
    if (stats.frame_count != 100) { FAIL("count wrong"); return; }
    if (stats.p95_ms <= stats.avg_ms) { FAIL("p95 should > avg"); return; }
    if (stats.p99_ms <= stats.p95_ms) { FAIL("p99 should > p95"); return; }
    PASS();
}

static void test_perf_hot_zones(void) {
    TEST("Record zones, get hot zones (sorted)");
    lgx_perf_analyzer_t* a = lgx_perf_create(NULL);

    lgx_perf_record_zone(a, "Physics", 5.0);
    lgx_perf_record_zone(a, "Render", 10.0);
    lgx_perf_record_zone(a, "Audio", 2.0);
    lgx_perf_record_zone(a, "Render", 8.0);  /* Accumulates */

    lgx_hot_zone_t zones[4];
    int count = lgx_perf_get_hot_zones(a, zones, 4);
    lgx_perf_destroy(a);

    if (count != 3) { FAIL("expected 3 zones"); return; }
    /* Should be sorted: Render (18.0) > Physics (5.0) > Audio (2.0) */
    if (strcmp(zones[0].name, "Render") != 0) { FAIL("Render should be #1"); return; }
    if (zones[0].total_ms < 17.9) { FAIL("Render total wrong"); return; }
    if (zones[0].call_count != 2) { FAIL("Render call_count wrong"); return; }
    PASS();
}

static void test_perf_dump_report(void) {
    TEST("Dump text performance report");
    lgx_perf_analyzer_t* a = lgx_perf_create(NULL);
    for (int i = 0; i < 10; i++) lgx_perf_record_frame(a, 16.67);
    lgx_perf_record_zone(a, "TestZone", 5.0);

    char buf[2048] = {0};
    int len = lgx_perf_dump_report(a, buf, sizeof(buf));
    lgx_perf_destroy(a);

    if (len <= 0) { FAIL("empty report"); return; }
    if (strstr(buf, "Performance Report") == NULL) { FAIL("missing header"); return; }
    if (strstr(buf, "TestZone") == NULL) { FAIL("missing zone"); return; }
    PASS();
}

static void test_perf_export_csv(void) {
    TEST("Export frame data to CSV");
    lgx_perf_analyzer_t* a = lgx_perf_create(NULL);
    for (int i = 0; i < 5; i++) lgx_perf_record_frame(a, 16.0 + i);

    const char* path = "/tmp/lgx_perf_test.csv";
    lgx_result_t r = lgx_perf_export_csv(a, path);
    lgx_perf_destroy(a);

    if (r != LGX_SUCCESS) { FAIL("export failed"); return; }
    if (access(path, R_OK) != 0) { FAIL("CSV file not created"); return; }
    unlink(path);
    PASS();
}

static void test_memtrack_create_destroy(void) {
    TEST("Memory tracker create/destroy");
    lgx_mem_tracker_t* t = lgx_memtrack_create(NULL);
    if (!t) { FAIL("create returned NULL"); return; }
    lgx_memtrack_destroy(t);
    PASS();
}

static void test_memtrack_alloc_free(void) {
    TEST("Record alloc/free, verify active bytes");
    lgx_mem_tracker_t* t = lgx_memtrack_create(NULL);

    int dummy1, dummy2;
    lgx_memtrack_record_alloc(t, &dummy1, 1024, "texture");
    lgx_memtrack_record_alloc(t, &dummy2, 2048, "mesh");

    size_t active = lgx_memtrack_get_active_bytes(t);
    if (active != 3072) { lgx_memtrack_destroy(t); FAIL("expected 3072"); return; }

    lgx_memtrack_record_free(t, &dummy1);
    active = lgx_memtrack_get_active_bytes(t);
    lgx_memtrack_destroy(t);

    if (active != 2048) { FAIL("expected 2048 after free"); return; }
    PASS();
}

static void test_memtrack_peak(void) {
    TEST("Peak memory tracking");
    lgx_mem_tracker_t* t = lgx_memtrack_create(NULL);

    int d1, d2, d3;
    lgx_memtrack_record_alloc(t, &d1, 1000, "a");
    lgx_memtrack_record_alloc(t, &d2, 2000, "b");
    lgx_memtrack_record_alloc(t, &d3, 3000, "c");  /* Peak = 6000 */
    lgx_memtrack_record_free(t, &d2);               /* Now 4000 */

    size_t peak = lgx_memtrack_get_peak_bytes(t);
    size_t active = lgx_memtrack_get_active_bytes(t);
    lgx_memtrack_destroy(t);

    if (peak != 6000) { FAIL("peak should be 6000"); return; }
    if (active != 4000) { FAIL("active should be 4000"); return; }
    PASS();
}

static void test_memtrack_leaks(void) {
    TEST("Leak detection (alloc without free)");
    lgx_mem_tracker_t* t = lgx_memtrack_create(NULL);

    int d1, d2, d3;
    lgx_memtrack_record_alloc(t, &d1, 100, "keep1");
    lgx_memtrack_record_alloc(t, &d2, 200, "freed");
    lgx_memtrack_record_alloc(t, &d3, 300, "keep2");
    lgx_memtrack_record_free(t, &d2);  /* Only free d2 */

    lgx_leak_info_t leaks[4];
    int leak_count = lgx_memtrack_find_leaks(t, leaks, 4);
    lgx_memtrack_destroy(t);

    if (leak_count != 2) { FAIL("expected 2 leaks"); return; }
    PASS();
}

static void test_memtrack_report(void) {
    TEST("Memory dump report");
    lgx_mem_tracker_t* t = lgx_memtrack_create(NULL);
    int d1;
    lgx_memtrack_record_alloc(t, &d1, 512, "leaked_buf");

    char buf[2048] = {0};
    int len = lgx_memtrack_dump_report(t, buf, sizeof(buf));
    lgx_memtrack_destroy(t);

    if (len <= 0) { FAIL("empty report"); return; }
    if (strstr(buf, "Memory Report") == NULL) { FAIL("missing header"); return; }
    if (strstr(buf, "leaked_buf") == NULL) { FAIL("missing leak tag"); return; }
    PASS();
}

static void test_memtrack_export_csv(void) {
    TEST("Export alloc log to CSV");
    lgx_mem_tracker_t* t = lgx_memtrack_create(NULL);
    int d1;
    lgx_memtrack_record_alloc(t, &d1, 256, "test_alloc");

    const char* path = "/tmp/lgx_memtrack_test.csv";
    lgx_result_t r = lgx_memtrack_export_csv(t, path);
    lgx_memtrack_destroy(t);

    if (r != LGX_SUCCESS) { FAIL("export failed"); return; }
    if (access(path, R_OK) != 0) { FAIL("CSV not created"); return; }
    unlink(path);
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  LGX Tooling Module v2.2 — Basic Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    lgx_runtime_init(NULL);

    test_perf_create_destroy();
    test_destroy_null();
    test_null_params();
    test_perf_avg_fps();
    test_perf_frame_stats();
    test_perf_hot_zones();
    test_perf_dump_report();
    test_perf_export_csv();
    test_memtrack_create_destroy();
    test_memtrack_alloc_free();
    test_memtrack_peak();
    test_memtrack_leaks();
    test_memtrack_report();
    test_memtrack_export_csv();

    lgx_runtime_shutdown();

    printf("\n───────────────────────────────────────────────────────────\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
