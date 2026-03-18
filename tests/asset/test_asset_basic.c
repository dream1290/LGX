/**
 * LGX Asset Pipeline v2.1 — Basic Tests
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#include "lgx_asset.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [%02d] %-50s", tests_run, name); \
    } while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* Helper: create a temp file with content */
static const char* create_temp_file(const char* content) {
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/lgx_test_asset_XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) return NULL;
    size_t len = strlen(content);
    ssize_t w = write(fd, content, len);
    close(fd);
    if (w != (ssize_t)len) return NULL;
    return path;
}

/* ═══════════════════════════════════════════════════════════════════════════ */

static void test_manager_create_destroy(void) {
    TEST("Manager create/destroy (NULL config)");
    lgx_asset_manager_t* mgr = lgx_asset_manager_create(NULL);
    if (!mgr) { FAIL("create returned NULL"); return; }
    lgx_asset_manager_destroy(mgr);
    PASS();
}

static void test_manager_config(void) {
    TEST("Manager create with explicit config");
    lgx_asset_config_t cfg = {
        .struct_size = sizeof(lgx_asset_config_t),
        .max_assets = 256,
        .max_search_paths = 8,
    };
    lgx_asset_manager_t* mgr = lgx_asset_manager_create(&cfg);
    if (!mgr) { FAIL("create returned NULL"); return; }
    lgx_asset_manager_destroy(mgr);
    PASS();
}

static void test_destroy_null(void) {
    TEST("Destroy NULL is safe");
    lgx_asset_manager_destroy(NULL);
    lgx_asset_unload(NULL);
    PASS();
}

static void test_null_params(void) {
    TEST("Functions with NULL return error");
    bool ok = true;
    ok = ok && (lgx_asset_manager_update(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_asset_manager_add_path(NULL, NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_asset_load(NULL, NULL, LGX_ASSET_BINARY) == NULL);
    ok = ok && (lgx_asset_reload(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_asset_get_data(NULL) == NULL);
    ok = ok && (lgx_asset_get_size(NULL) == 0);
    ok = ok && (lgx_asset_get_path(NULL) == NULL);
    ok = ok && (lgx_asset_get_state(NULL) == LGX_ASSET_ERROR);
    if (!ok) { FAIL("unexpected result"); return; }
    PASS();
}

static void test_add_search_path(void) {
    TEST("Add search path");
    lgx_asset_manager_t* mgr = lgx_asset_manager_create(NULL);
    lgx_result_t r = lgx_asset_manager_add_path(mgr, "/tmp");
    lgx_asset_manager_destroy(mgr);
    if (r != LGX_SUCCESS) { FAIL("add path should succeed"); return; }
    PASS();
}

static void test_load_file(void) {
    TEST("Load file from disk");
    const char* content = "Hello LGX Asset Pipeline!";
    const char* path = create_temp_file(content);
    if (!path) { FAIL("temp file creation failed"); return; }

    lgx_asset_manager_t* mgr = lgx_asset_manager_create(NULL);
    lgx_asset_t* asset = lgx_asset_load(mgr, path, LGX_ASSET_TEXT);

    if (!asset) { lgx_asset_manager_destroy(mgr); unlink(path); FAIL("load returned NULL"); return; }

    bool ok = true;
    ok = ok && (lgx_asset_get_state(asset) == LGX_ASSET_READY);
    ok = ok && (lgx_asset_get_size(asset) == strlen(content));
    ok = ok && (lgx_asset_get_data(asset) != NULL);
    ok = ok && (memcmp(lgx_asset_get_data(asset), content, strlen(content)) == 0);
    ok = ok && (lgx_asset_get_type(asset) == LGX_ASSET_TEXT);
    ok = ok && (lgx_asset_get_path(asset) != NULL);

    lgx_asset_manager_destroy(mgr);
    unlink(path);

    if (!ok) { FAIL("asset data mismatch"); return; }
    PASS();
}

static void test_load_nonexistent(void) {
    TEST("Load nonexistent file returns error state");
    lgx_asset_manager_t* mgr = lgx_asset_manager_create(NULL);
    lgx_asset_t* asset = lgx_asset_load(mgr, "/no/such/file.bin", LGX_ASSET_BINARY);

    lgx_asset_state_t state = lgx_asset_get_state(asset);
    lgx_asset_manager_destroy(mgr);

    if (state != LGX_ASSET_ERROR) { FAIL("expected ERROR state"); return; }
    PASS();
}

static void test_unload(void) {
    TEST("Unload frees data");
    const char* content = "test data";
    const char* path = create_temp_file(content);
    if (!path) { FAIL("temp file"); return; }

    lgx_asset_manager_t* mgr = lgx_asset_manager_create(NULL);
    lgx_asset_t* asset = lgx_asset_load(mgr, path, LGX_ASSET_BINARY);

    lgx_asset_unload(asset);
    lgx_asset_state_t state = lgx_asset_get_state(asset);
    const void* data = lgx_asset_get_data(asset);

    lgx_asset_manager_destroy(mgr);
    unlink(path);

    if (state != LGX_ASSET_UNLOADED) { FAIL("expected UNLOADED"); return; }
    if (data != NULL) { FAIL("data should be NULL after unload"); return; }
    PASS();
}

static void test_reload(void) {
    TEST("Reload reloads from disk");
    const char* content1 = "version1";
    const char* path = create_temp_file(content1);
    if (!path) { FAIL("temp file"); return; }

    lgx_asset_manager_t* mgr = lgx_asset_manager_create(NULL);
    lgx_asset_t* asset = lgx_asset_load(mgr, path, LGX_ASSET_TEXT);

    /* Overwrite file */
    int fd = open(path, O_WRONLY | O_TRUNC);
    const char* content2 = "version2_longer";
    if (fd >= 0) { write(fd, content2, strlen(content2)); close(fd); }

    lgx_result_t r = lgx_asset_reload(asset);
    size_t new_size = lgx_asset_get_size(asset);

    lgx_asset_manager_destroy(mgr);
    unlink(path);

    if (r != LGX_SUCCESS) { FAIL("reload failed"); return; }
    if (new_size != strlen(content2)) { FAIL("size should reflect new content"); return; }
    PASS();
}

static void test_compress_roundtrip(void) {
    TEST("Compress/decompress round-trip");
    const char* data = "AAAAAAAAABBBBBBBBCCCCCCCCDDDDDDDDAAAAAAAABBBBBBBB";
    size_t data_len = strlen(data);

    uint8_t compressed[256] = {0};
    int clen = lgx_asset_compress(data, data_len, compressed, sizeof(compressed));
    if (clen <= 0) { FAIL("compress failed"); return; }

    uint8_t decompressed[256] = {0};
    int dlen = lgx_asset_decompress(compressed, (size_t)clen, decompressed, sizeof(decompressed));
    if (dlen != (int)data_len) { FAIL("decompress size wrong"); return; }
    if (memcmp(decompressed, data, data_len) != 0) { FAIL("data mismatch"); return; }
    PASS();
}

static void test_compress_zeros(void) {
    TEST("Compress all-zero data (high compression)");
    uint8_t zeros[256];
    memset(zeros, 0, sizeof(zeros));

    uint8_t compressed[256] = {0};
    int clen = lgx_asset_compress(zeros, sizeof(zeros), compressed, sizeof(compressed));
    if (clen <= 0) { FAIL("compress failed"); return; }
    if (clen >= 256) { FAIL("should compress well"); return; }

    uint8_t decompressed[256] = {0};
    int dlen = lgx_asset_decompress(compressed, (size_t)clen, decompressed, sizeof(decompressed));
    if (dlen != 256) { FAIL("decompress size wrong"); return; }
    if (memcmp(decompressed, zeros, 256) != 0) { FAIL("data mismatch"); return; }
    PASS();
}

static void test_compress_random(void) {
    TEST("Compress random-like data (still decompresses)");
    uint8_t data[64];
    for (int i = 0; i < 64; i++) data[i] = (uint8_t)(i * 37 + 13);

    uint8_t compressed[256] = {0};
    int clen = lgx_asset_compress(data, sizeof(data), compressed, sizeof(compressed));
    if (clen <= 0) { FAIL("compress failed"); return; }

    uint8_t decompressed[64] = {0};
    int dlen = lgx_asset_decompress(compressed, (size_t)clen, decompressed, sizeof(decompressed));
    if (dlen != 64) { FAIL("decompress size wrong"); return; }
    if (memcmp(decompressed, data, 64) != 0) { FAIL("data mismatch"); return; }
    PASS();
}

static void test_search_path_resolution(void) {
    TEST("Multiple search paths (priority order)");
    const char* content = "search_path_test";
    const char* path = create_temp_file(content);
    if (!path) { FAIL("temp file"); return; }

    /* Extract filename from path */
    const char* basename = strrchr(path, '/');
    if (!basename) { unlink(path); FAIL("bad path"); return; }
    basename++;  /* Skip '/' */

    lgx_asset_manager_t* mgr = lgx_asset_manager_create(NULL);
    lgx_asset_manager_add_path(mgr, "/nonexistent");
    lgx_asset_manager_add_path(mgr, "/tmp");

    lgx_asset_t* asset = lgx_asset_load(mgr, basename, LGX_ASSET_TEXT);
    lgx_asset_state_t state = lgx_asset_get_state(asset);

    lgx_asset_manager_destroy(mgr);
    unlink(path);

    if (state != LGX_ASSET_READY) { FAIL("should find via search path"); return; }
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  LGX Asset Pipeline v2.1 — Basic Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    lgx_runtime_init(NULL);

    test_manager_create_destroy();
    test_manager_config();
    test_destroy_null();
    test_null_params();
    test_add_search_path();
    test_load_file();
    test_load_nonexistent();
    test_unload();
    test_reload();
    test_compress_roundtrip();
    test_compress_zeros();
    test_compress_random();
    test_search_path_resolution();

    lgx_runtime_shutdown();

    printf("\n───────────────────────────────────────────────────────────\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
