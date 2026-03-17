/**
 * LGX Input Module v1.3 — Basic Tests
 * Headless tests: no real input devices needed.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#include "lgx_input.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [%02d] %-50s", tests_run, name); \
    } while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Cases
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_create_destroy_default(void) {
    TEST("Create/destroy with NULL config (defaults)");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create returned NULL"); return; }
    lgx_in_destroy(sys);
    PASS();
}

static void test_create_destroy_config(void) {
    TEST("Create/destroy with explicit config");
    lgx_in_config_t cfg = {
        .struct_size = sizeof(lgx_in_config_t),
        .enable_gamepad = true,
        .enable_keyboard = true,
        .enable_mouse = true,
        .event_queue_size = 128,
    };
    lgx_in_system_t* sys = lgx_in_create(&cfg);
    if (!sys) { FAIL("create returned NULL"); return; }
    lgx_in_destroy(sys);
    PASS();
}

static void test_destroy_null(void) {
    TEST("Destroy NULL is safe (no-op)");
    lgx_in_destroy(NULL);  /* Should not crash */
    PASS();
}

static void test_poll_null(void) {
    TEST("Poll NULL returns error");
    lgx_result_t r = lgx_in_poll(NULL);
    if (r != LGX_ERROR_INVALID_PARAM) { FAIL("expected error"); return; }
    PASS();
}

static void test_poll_empty(void) {
    TEST("Poll with no devices succeeds");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_result_t r = lgx_in_poll(sys);
    if (r != LGX_SUCCESS) { FAIL("poll should succeed even with no devices"); lgx_in_destroy(sys); return; }
    lgx_in_destroy(sys);
    PASS();
}

static void test_key_down_no_devices(void) {
    TEST("Key queries return false with no devices");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_in_poll(sys);
    bool ok = true;
    ok = ok && !lgx_in_key_down(sys, LGX_KEY_A);
    ok = ok && !lgx_in_key_pressed(sys, LGX_KEY_SPACE);
    ok = ok && !lgx_in_key_released(sys, LGX_KEY_ESCAPE);
    ok = ok && !lgx_in_key_down(NULL, LGX_KEY_A);
    ok = ok && !lgx_in_key_down(sys, LGX_KEY_COUNT);  /* Out of range */
    lgx_in_destroy(sys);
    if (!ok) { FAIL("unexpected true"); return; }
    PASS();
}

static void test_mouse_state_no_devices(void) {
    TEST("Mouse state zeroed with no devices");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_in_poll(sys);
    lgx_in_mouse_state_t state;
    memset(&state, 0xFF, sizeof(state));
    lgx_result_t r = lgx_in_mouse_get_state(sys, &state);
    if (r != LGX_SUCCESS) { FAIL("get_state failed"); lgx_in_destroy(sys); return; }
    if (state.x != 0 || state.y != 0 || state.buttons != 0) {
        FAIL("non-zero initial state"); lgx_in_destroy(sys); return;
    }
    lgx_in_destroy(sys);
    PASS();
}

static void test_mouse_state_null(void) {
    TEST("Mouse state NULL params return error");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_result_t r1 = lgx_in_mouse_get_state(NULL, NULL);
    lgx_result_t r2 = lgx_in_mouse_get_state(sys, NULL);
    lgx_in_destroy(sys);
    if (r1 != LGX_ERROR_INVALID_PARAM || r2 != LGX_ERROR_INVALID_PARAM) {
        FAIL("expected error"); return;
    }
    PASS();
}

static void test_gamepad_count_no_devices(void) {
    TEST("Gamepad count is 0 with no devices");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    uint32_t count = lgx_in_gamepad_count(sys);
    lgx_in_destroy(sys);
    if (count != 0) { FAIL("expected 0 gamepads"); return; }
    PASS();
}

static void test_gamepad_connected_invalid(void) {
    TEST("Gamepad connected returns false for invalid IDs");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    bool ok = true;
    ok = ok && !lgx_in_gamepad_connected(sys, 0);
    ok = ok && !lgx_in_gamepad_connected(sys, 99);
    ok = ok && !lgx_in_gamepad_connected(NULL, 0);
    lgx_in_destroy(sys);
    if (!ok) { FAIL("unexpected true"); return; }
    PASS();
}

static void test_gamepad_get_state_invalid(void) {
    TEST("Gamepad get_state fails for invalid params");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_in_gamepad_state_t state;
    lgx_result_t r = lgx_in_gamepad_get_state(sys, 0, &state);
    lgx_in_destroy(sys);
    if (r != LGX_ERROR_INVALID_PARAM) { FAIL("expected error"); return; }
    PASS();
}

static void test_event_queue_empty(void) {
    TEST("Event queue empty after poll with no devices");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_in_poll(sys);
    lgx_in_event_t evt;
    bool has_event = lgx_in_next_event(sys, &evt);
    lgx_in_destroy(sys);
    if (has_event) { FAIL("expected empty queue"); return; }
    PASS();
}

static void test_mouse_relative_mode(void) {
    TEST("Mouse relative mode toggle");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_result_t r1 = lgx_in_mouse_set_relative(sys, true);
    lgx_result_t r2 = lgx_in_mouse_set_relative(sys, false);
    lgx_result_t r3 = lgx_in_mouse_set_relative(NULL, true);
    lgx_in_destroy(sys);
    if (r1 != LGX_SUCCESS || r2 != LGX_SUCCESS || r3 != LGX_ERROR_INVALID_PARAM) {
        FAIL("unexpected result"); return;
    }
    PASS();
}

static void test_gamepad_deadzone(void) {
    TEST("Gamepad deadzone set returns error for no pad");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_result_t r = lgx_in_gamepad_set_deadzone(sys, 0, 0.2f);
    lgx_in_destroy(sys);
    if (r != LGX_ERROR_INVALID_PARAM) { FAIL("expected error (no gamepad)"); return; }
    PASS();
}

static void test_gamepad_rumble_invalid(void) {
    TEST("Gamepad rumble fails without connected pad");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_result_t r = lgx_in_gamepad_rumble(sys, 0, 0.5f, 0.5f, 200);
    lgx_in_destroy(sys);
    if (r != LGX_ERROR_INVALID_PARAM) { FAIL("expected error"); return; }
    PASS();
}

static void test_gamepad_name_invalid(void) {
    TEST("Gamepad name returns NULL for invalid pad");
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    const char* name = lgx_in_gamepad_get_name(sys, 0);
    lgx_in_destroy(sys);
    if (name != NULL) { FAIL("expected NULL"); return; }
    PASS();
}

static void test_next_event_null(void) {
    TEST("next_event with NULL params returns false");
    bool ok = true;
    ok = ok && !lgx_in_next_event(NULL, NULL);
    lgx_in_system_t* sys = lgx_in_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    ok = ok && !lgx_in_next_event(sys, NULL);
    lgx_in_destroy(sys);
    if (!ok) { FAIL("expected false"); return; }
    PASS();
}

static void test_disabled_inputs(void) {
    TEST("Disabled inputs do not open devices");
    lgx_in_config_t cfg = {
        .struct_size = sizeof(lgx_in_config_t),
        .enable_gamepad = false,
        .enable_keyboard = false,
        .enable_mouse = false,
        .event_queue_size = 64,
    };
    lgx_in_system_t* sys = lgx_in_create(&cfg);
    if (!sys) { FAIL("create failed"); return; }
    lgx_in_poll(sys);
    lgx_in_destroy(sys);
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  LGX Input Module v1.3 — Basic Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    lgx_runtime_init(NULL);

    test_create_destroy_default();
    test_create_destroy_config();
    test_destroy_null();
    test_poll_null();
    test_poll_empty();
    test_key_down_no_devices();
    test_mouse_state_no_devices();
    test_mouse_state_null();
    test_gamepad_count_no_devices();
    test_gamepad_connected_invalid();
    test_gamepad_get_state_invalid();
    test_event_queue_empty();
    test_mouse_relative_mode();
    test_gamepad_deadzone();
    test_gamepad_rumble_invalid();
    test_gamepad_name_invalid();
    test_next_event_null();
    test_disabled_inputs();

    lgx_runtime_shutdown();

    printf("\n───────────────────────────────────────────────────────────\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
