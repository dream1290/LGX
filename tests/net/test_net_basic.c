/**
 * LGX Networking Module v2.0 — Basic Tests
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#include "lgx_net.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [%02d] %-50s", tests_run, name); \
    } while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ═══════════════════════════════════════════════════════════════════════════ */

static void test_socket_create_destroy(void) {
    TEST("Socket create/destroy (NULL config)");
    lgx_net_socket_t* sock = lgx_net_socket_create(NULL);
    if (!sock) { FAIL("create returned NULL"); return; }
    lgx_net_socket_destroy(sock);
    PASS();
}

static void test_socket_create_config(void) {
    TEST("Socket create with explicit config");
    lgx_net_config_t cfg = {
        .struct_size = sizeof(lgx_net_config_t),
        .max_connections = 16,
        .timeout_ms = 3000,
        .tick_rate = 30,
    };
    lgx_net_socket_t* sock = lgx_net_socket_create(&cfg);
    if (!sock) { FAIL("create returned NULL"); return; }
    lgx_net_socket_destroy(sock);
    PASS();
}

static void test_destroy_null(void) {
    TEST("Destroy NULL is safe");
    lgx_net_socket_destroy(NULL);
    lgx_net_stream_destroy(NULL);
    PASS();
}

static void test_null_params(void) {
    TEST("Functions with NULL return error/0");
    bool ok = true;
    ok = ok && (lgx_net_socket_bind(NULL, 0) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_net_socket_send(NULL, NULL, NULL, 0) == -1);
    ok = ok && (lgx_net_socket_recv(NULL, NULL, NULL, 0) == -1);
    ok = ok && (lgx_net_connect(NULL, NULL, 0) == -1);
    ok = ok && (lgx_net_disconnect(NULL, 0) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_net_update(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_net_get_connection_count(NULL) == 0);
    ok = ok && (lgx_net_get_rtt_ms(NULL, 0) == 0.0);
    ok = ok && (lgx_net_stream_bytes_written(NULL) == 0);
    if (!ok) { FAIL("unexpected result"); return; }
    PASS();
}

static void test_socket_bind(void) {
    TEST("Socket bind to ephemeral port");
    lgx_net_socket_t* sock = lgx_net_socket_create(NULL);
    /* Use port 0 for ephemeral (OS-assigned) */
    lgx_result_t r = lgx_net_socket_bind(sock, 0);
    lgx_net_socket_destroy(sock);
    if (r != LGX_SUCCESS) { FAIL("bind should succeed for port 0"); return; }
    PASS();
}

static void test_loopback_send_recv(void) {
    TEST("Loopback send/recv (localhost UDP)");
    lgx_net_socket_t* sender = lgx_net_socket_create(NULL);
    lgx_net_socket_t* receiver = lgx_net_socket_create(NULL);

    /* Bind receiver to a known port */
    lgx_result_t r = lgx_net_socket_bind(receiver, 19876);
    if (r != LGX_SUCCESS) {
        lgx_net_socket_destroy(sender);
        lgx_net_socket_destroy(receiver);
        FAIL("bind failed"); return;
    }

    /* Send from sender to receiver */
    lgx_net_addr_t dest = { .ip = htonl(INADDR_LOOPBACK), .port = 19876 };
    const char* msg = "Hello LGX";
    int sent = lgx_net_socket_send(sender, &dest, msg, strlen(msg));

    /* Small delay for the packet to arrive */
    usleep(10000);

    char buf[256] = {0};
    lgx_net_addr_t from = {0};
    int received = lgx_net_socket_recv(receiver, &from, buf, sizeof(buf));

    lgx_net_socket_destroy(sender);
    lgx_net_socket_destroy(receiver);

    if (sent != (int)strlen(msg)) { FAIL("send byte count wrong"); return; }
    if (received != (int)strlen(msg)) { FAIL("recv byte count wrong"); return; }
    if (memcmp(buf, msg, strlen(msg)) != 0) { FAIL("data mismatch"); return; }
    PASS();
}

static void test_recv_no_data(void) {
    TEST("Recv returns 0 when no data (non-blocking)");
    lgx_net_socket_t* sock = lgx_net_socket_create(NULL);
    lgx_net_socket_bind(sock, 0);

    char buf[64];
    lgx_net_addr_t from = {0};
    int r = lgx_net_socket_recv(sock, &from, buf, sizeof(buf));
    lgx_net_socket_destroy(sock);

    if (r != 0) { FAIL("expected 0 (no data)"); return; }
    PASS();
}

static void test_stream_integers(void) {
    TEST("Stream write/read u8, u16, u32, f32");
    uint8_t buf[64];
    lgx_net_stream_t* ws = lgx_net_stream_create(buf, sizeof(buf));

    lgx_net_stream_write_u8(ws, 42);
    lgx_net_stream_write_u16(ws, 1234);
    lgx_net_stream_write_u32(ws, 0xDEADBEEF);
    lgx_net_stream_write_f32(ws, 3.14f);

    size_t written = lgx_net_stream_bytes_written(ws);
    lgx_net_stream_destroy(ws);

    lgx_net_stream_t* rs = lgx_net_stream_create_read(buf, written);
    uint8_t  v8  = lgx_net_stream_read_u8(rs);
    uint16_t v16 = lgx_net_stream_read_u16(rs);
    uint32_t v32 = lgx_net_stream_read_u32(rs);
    float    vf  = lgx_net_stream_read_f32(rs);
    lgx_net_stream_destroy(rs);

    if (v8 != 42) { FAIL("u8 mismatch"); return; }
    if (v16 != 1234) { FAIL("u16 mismatch"); return; }
    if (v32 != 0xDEADBEEF) { FAIL("u32 mismatch"); return; }
    if (fabsf(vf - 3.14f) > 0.001f) { FAIL("f32 mismatch"); return; }
    PASS();
}

static void test_stream_string(void) {
    TEST("Stream write/read string");
    uint8_t buf[128];
    lgx_net_stream_t* ws = lgx_net_stream_create(buf, sizeof(buf));
    lgx_net_stream_write_string(ws, "LGX Networking", 64);
    size_t written = lgx_net_stream_bytes_written(ws);
    lgx_net_stream_destroy(ws);

    lgx_net_stream_t* rs = lgx_net_stream_create_read(buf, written);
    char str[64] = {0};
    int len = lgx_net_stream_read_string(rs, str, sizeof(str));
    lgx_net_stream_destroy(rs);

    if (len <= 0) { FAIL("read_string returned 0"); return; }
    if (strcmp(str, "LGX Networking") != 0) { FAIL("string mismatch"); return; }
    PASS();
}

static void test_stream_overflow(void) {
    TEST("Stream overflow detection");
    uint8_t buf[4];
    lgx_net_stream_t* ws = lgx_net_stream_create(buf, sizeof(buf));
    lgx_result_t r1 = lgx_net_stream_write_u32(ws, 0x12345678);  /* Fits exactly */
    lgx_result_t r2 = lgx_net_stream_write_u8(ws, 0xFF);         /* Should fail */
    lgx_net_stream_destroy(ws);

    if (r1 != LGX_SUCCESS) { FAIL("first write should fit"); return; }
    if (r2 != LGX_ERROR_INVALID_PARAM) { FAIL("overflow should fail"); return; }
    PASS();
}

static void test_delta_roundtrip(void) {
    TEST("Delta encode/decode round-trip");
    uint8_t prev[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint8_t curr[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    /* Change a few bytes */
    curr[3] = 99;
    curr[7] = 200;
    curr[15] = 42;

    uint8_t delta[64] = {0};
    int delta_len = lgx_net_delta_encode(prev, curr, 16, delta, sizeof(delta));
    if (delta_len <= 0) { FAIL("encode failed"); return; }

    uint8_t decoded[16] = {0};
    int decoded_len = lgx_net_delta_decode(prev, delta, (size_t)delta_len, decoded, sizeof(decoded));
    if (decoded_len != 16) { FAIL("decode size wrong"); return; }

    if (memcmp(decoded, curr, 16) != 0) { FAIL("decoded != curr"); return; }
    PASS();
}

static void test_delta_identical(void) {
    TEST("Delta encode identical states (compressed)");
    uint8_t state[32];
    memset(state, 0xAA, sizeof(state));

    uint8_t delta[128] = {0};
    int delta_len = lgx_net_delta_encode(state, state, sizeof(state), delta, sizeof(delta));

    if (delta_len <= 0) { FAIL("encode should succeed"); return; }
    /* Identical states should compress well (all zero XOR → RLE) */
    if (delta_len >= 32) { FAIL("identical should compress"); return; }
    PASS();
}

static void test_delta_all_different(void) {
    TEST("Delta encode all-different states");
    uint8_t prev[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t curr[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    uint8_t delta[64] = {0};
    int delta_len = lgx_net_delta_encode(prev, curr, 8, delta, sizeof(delta));
    if (delta_len <= 0) { FAIL("encode failed"); return; }

    uint8_t decoded[8] = {0};
    int decoded_len = lgx_net_delta_decode(prev, delta, (size_t)delta_len, decoded, sizeof(decoded));
    if (decoded_len != 8) { FAIL("decode size wrong"); return; }
    if (memcmp(decoded, curr, 8) != 0) { FAIL("decoded != curr"); return; }
    PASS();
}

static void test_connection_tracking(void) {
    TEST("Connection count and disconnect");
    lgx_net_socket_t* sock = lgx_net_socket_create(NULL);

    int c1 = lgx_net_connect(sock, "127.0.0.1", 9999);
    int c2 = lgx_net_connect(sock, "127.0.0.1", 9998);
    uint32_t count = lgx_net_get_connection_count(sock);

    lgx_net_disconnect(sock, c1);
    uint32_t after = lgx_net_get_connection_count(sock);

    lgx_net_socket_destroy(sock);

    if (c1 < 0 || c2 < 0) { FAIL("connect failed"); return; }
    if (count != 2) { FAIL("expected 2 connections"); return; }
    if (after != 1) { FAIL("expected 1 after disconnect"); return; }
    PASS();
}

static void test_rtt_no_connection(void) {
    TEST("RTT returns 0 for invalid connection");
    lgx_net_socket_t* sock = lgx_net_socket_create(NULL);
    double rtt = lgx_net_get_rtt_ms(sock, 99);
    lgx_net_socket_destroy(sock);

    if (rtt != 0.0) { FAIL("expected 0.0"); return; }
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  LGX Networking Module v2.0 — Basic Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    lgx_runtime_init(NULL);

    test_socket_create_destroy();
    test_socket_create_config();
    test_destroy_null();
    test_null_params();
    test_socket_bind();
    test_loopback_send_recv();
    test_recv_no_data();
    test_stream_integers();
    test_stream_string();
    test_stream_overflow();
    test_delta_roundtrip();
    test_delta_identical();
    test_delta_all_different();
    test_connection_tracking();
    test_rtt_no_connection();

    lgx_runtime_shutdown();

    printf("\n───────────────────────────────────────────────────────────\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
