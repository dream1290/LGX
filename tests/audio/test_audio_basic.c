/**
 * LGX Audio Module v1.4 — Basic Tests
 * Headless tests: ALSA may or may not be available.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#include "lgx_audio.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [%02d] %-50s", tests_run, name); \
    } while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* Generate a sine wave buffer (mono, float) */
static float* gen_sine(uint32_t frames, uint32_t sample_rate, float freq) {
    float* buf = malloc(frames * sizeof(float));
    if (!buf) return NULL;
    for (uint32_t i = 0; i < frames; i++) {
        buf[i] = sinf(2.0f * 3.14159265f * freq * (float)i / (float)sample_rate);
    }
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════ */

static void test_create_destroy_default(void) {
    TEST("Create/destroy with NULL config (defaults)");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create returned NULL"); return; }
    lgx_aud_destroy(sys);
    PASS();
}

static void test_create_destroy_config(void) {
    TEST("Create/destroy with explicit config");
    lgx_aud_config_t cfg = {
        .struct_size = sizeof(lgx_aud_config_t),
        .sample_rate = 44100,
        .channels = 2,
        .buffer_frames = 512,
        .max_sources = 16,
        .master_volume = 0.8f,
    };
    lgx_aud_system_t* sys = lgx_aud_create(&cfg);
    if (!sys) { FAIL("create returned NULL"); return; }
    lgx_aud_destroy(sys);
    PASS();
}

static void test_destroy_null(void) {
    TEST("Destroy NULL is safe (no-op)");
    lgx_aud_destroy(NULL);
    PASS();
}

static void test_update_null(void) {
    TEST("Update NULL returns error");
    lgx_result_t r = lgx_aud_update(NULL);
    if (r != LGX_ERROR_INVALID_PARAM) { FAIL("expected error"); return; }
    PASS();
}

static void test_update_no_sources(void) {
    TEST("Update with no sources succeeds");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_result_t r = lgx_aud_update(sys);
    lgx_aud_destroy(sys);
    if (r != LGX_SUCCESS) { FAIL("update should succeed"); return; }
    PASS();
}

static void test_buffer_create_raw(void) {
    TEST("Buffer create from raw PCM (sine wave)");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    float* sine = gen_sine(4800, 48000, 440.0f);
    if (!sine) { FAIL("gen_sine failed"); lgx_aud_destroy(sys); return; }
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, sine, 4800,
                                                    LGX_AUD_FORMAT_F32, 48000, 1);
    free(sine);
    if (!buf) { FAIL("buffer_create returned NULL"); lgx_aud_destroy(sys); return; }
    lgx_aud_buffer_destroy(buf);
    lgx_aud_destroy(sys);
    PASS();
}

static void test_buffer_create_s16(void) {
    TEST("Buffer create from S16 PCM");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    int16_t samples[100];
    for (int i = 0; i < 100; i++) samples[i] = (int16_t)(sinf(i * 0.1f) * 16000.0f);
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, samples, 100,
                                                    LGX_AUD_FORMAT_S16, 48000, 1);
    if (!buf) { FAIL("buffer_create returned NULL"); lgx_aud_destroy(sys); return; }
    lgx_aud_buffer_destroy(buf);
    lgx_aud_destroy(sys);
    PASS();
}

static void test_buffer_null_params(void) {
    TEST("Buffer create with NULL params returns NULL");
    lgx_aud_buffer_t* b1 = lgx_aud_buffer_create(NULL, NULL, 0, LGX_AUD_FORMAT_F32, 48000, 1);
    if (b1 != NULL) { FAIL("expected NULL"); return; }
    lgx_aud_buffer_destroy(NULL);  /* no crash */
    PASS();
}

static void test_source_lifecycle(void) {
    TEST("Source create/play/pause/stop/destroy");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    float* sine = gen_sine(480, 48000, 440.0f);
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, sine, 480,
                                                    LGX_AUD_FORMAT_F32, 48000, 1);
    free(sine);
    lgx_aud_source_t* src = lgx_aud_source_create(sys, buf);
    if (!src) { FAIL("source_create returned NULL"); lgx_aud_buffer_destroy(buf); lgx_aud_destroy(sys); return; }

    if (lgx_aud_source_get_state(src) != LGX_AUD_STOPPED) { FAIL("initial state"); }

    lgx_aud_source_play(src);
    if (lgx_aud_source_get_state(src) != LGX_AUD_PLAYING) { FAIL("play state"); }

    lgx_aud_source_pause(src);
    if (lgx_aud_source_get_state(src) != LGX_AUD_PAUSED) { FAIL("pause state"); }

    lgx_aud_source_play(src);
    if (lgx_aud_source_get_state(src) != LGX_AUD_PLAYING) { FAIL("resume state"); }

    lgx_aud_source_stop(src);
    if (lgx_aud_source_get_state(src) != LGX_AUD_STOPPED) { FAIL("stop state"); }

    lgx_aud_source_destroy(src);
    lgx_aud_buffer_destroy(buf);
    lgx_aud_destroy(sys);
    PASS();
}

static void test_source_volume_pitch(void) {
    TEST("Source volume/pitch/looping setters");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    float* sine = gen_sine(480, 48000, 440.0f);
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, sine, 480, LGX_AUD_FORMAT_F32, 48000, 1);
    free(sine);
    lgx_aud_source_t* src = lgx_aud_source_create(sys, buf);

    lgx_result_t r1 = lgx_aud_source_set_volume(src, 0.5f);
    lgx_result_t r2 = lgx_aud_source_set_pitch(src, 2.0f);
    lgx_result_t r3 = lgx_aud_source_set_looping(src, true);

    lgx_aud_source_destroy(src);
    lgx_aud_buffer_destroy(buf);
    lgx_aud_destroy(sys);

    if (r1 != LGX_SUCCESS || r2 != LGX_SUCCESS || r3 != LGX_SUCCESS) {
        FAIL("setter failed"); return;
    }
    PASS();
}

static void test_source_null_params(void) {
    TEST("Source functions with NULL return error");
    bool ok = true;
    ok = ok && (lgx_aud_source_create(NULL, NULL) == NULL);
    ok = ok && (lgx_aud_source_play(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_aud_source_stop(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_aud_source_pause(NULL) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_aud_source_set_volume(NULL, 0.5f) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_aud_source_set_pitch(NULL, 1.0f) == LGX_ERROR_INVALID_PARAM);
    ok = ok && (lgx_aud_source_get_state(NULL) == LGX_AUD_STOPPED);
    lgx_aud_source_destroy(NULL);  /* no crash */
    if (!ok) { FAIL("unexpected result"); return; }
    PASS();
}

static void test_3d_positions(void) {
    TEST("3D position setting (listener + source)");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    float* sine = gen_sine(480, 48000, 440.0f);
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, sine, 480, LGX_AUD_FORMAT_F32, 48000, 1);
    free(sine);
    lgx_aud_source_t* src = lgx_aud_source_create(sys, buf);

    lgx_result_t r1 = lgx_aud_source_set_position(src, 5.0f, 0.0f, -3.0f);
    lgx_result_t r2 = lgx_aud_listener_set_position(sys, 0.0f, 0.0f, 0.0f);
    lgx_result_t r3 = lgx_aud_listener_set_orientation(sys, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);

    lgx_aud_source_destroy(src);
    lgx_aud_buffer_destroy(buf);
    lgx_aud_destroy(sys);

    if (r1 != LGX_SUCCESS || r2 != LGX_SUCCESS || r3 != LGX_SUCCESS) {
        FAIL("setter failed"); return;
    }
    PASS();
}

static void test_distance_model(void) {
    TEST("Distance model parameters");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    float* sine = gen_sine(480, 48000, 440.0f);
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, sine, 480, LGX_AUD_FORMAT_F32, 48000, 1);
    free(sine);
    lgx_aud_source_t* src = lgx_aud_source_create(sys, buf);

    lgx_result_t r1 = lgx_aud_source_set_distance_model(src, 1.0f, 50.0f, 2.0f);
    lgx_result_t r2 = lgx_aud_source_set_distance_model(src, -1.0f, 50.0f, 1.0f);  /* invalid */

    lgx_aud_source_destroy(src);
    lgx_aud_buffer_destroy(buf);
    lgx_aud_destroy(sys);

    if (r1 != LGX_SUCCESS || r2 != LGX_ERROR_INVALID_PARAM) {
        FAIL("unexpected result"); return;
    }
    PASS();
}

static void test_master_volume(void) {
    TEST("Master volume get/set");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create failed"); return; }

    float initial = lgx_aud_get_master_volume(sys);
    lgx_aud_set_master_volume(sys, 0.5f);
    float after = lgx_aud_get_master_volume(sys);

    lgx_aud_destroy(sys);

    if (initial < 0.99f || initial > 1.01f) { FAIL("default not 1.0"); return; }
    if (after < 0.49f || after > 0.51f) { FAIL("set to 0.5 failed"); return; }
    PASS();
}

static void test_mix_with_source(void) {
    TEST("Mix one source through update (no crash)");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    float* sine = gen_sine(4800, 48000, 440.0f);
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create(sys, sine, 4800,
                                                    LGX_AUD_FORMAT_F32, 48000, 1);
    free(sine);
    lgx_aud_source_t* src = lgx_aud_source_create(sys, buf);
    lgx_aud_source_play(src);

    /* Run a few update cycles */
    for (int i = 0; i < 3; i++) {
        lgx_aud_update(sys);
    }

    lgx_aud_source_destroy(src);
    lgx_aud_buffer_destroy(buf);
    lgx_aud_destroy(sys);
    PASS();
}

static void test_wav_load_invalid(void) {
    TEST("WAV load nonexistent file returns NULL");
    lgx_aud_system_t* sys = lgx_aud_create(NULL);
    if (!sys) { FAIL("create failed"); return; }
    lgx_aud_buffer_t* buf = lgx_aud_buffer_create_from_wav(sys, "/tmp/nonexistent.wav");
    lgx_aud_destroy(sys);
    if (buf != NULL) { FAIL("expected NULL"); return; }
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  LGX Audio Module v1.4 — Basic Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    lgx_runtime_init(NULL);

    test_create_destroy_default();
    test_create_destroy_config();
    test_destroy_null();
    test_update_null();
    test_update_no_sources();
    test_buffer_create_raw();
    test_buffer_create_s16();
    test_buffer_null_params();
    test_source_lifecycle();
    test_source_volume_pitch();
    test_source_null_params();
    test_3d_positions();
    test_distance_model();
    test_master_volume();
    test_mix_with_source();
    test_wav_load_invalid();

    lgx_runtime_shutdown();

    printf("\n───────────────────────────────────────────────────────────\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
