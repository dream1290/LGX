/**
 * @file test_threading_fiber.c
 * @brief LGX Threading Module - Fiber System Tests
 *
 * Validates:
 * - Fiber creation and destruction (with guard-page stacks)
 * - Fiber yield/resume round-trip
 * - Fiber wait-on-job (suspend until job completes)
 * - Multiple fibers interleaving
 * - Fiber state transitions
 */

#include "lgx_threading.h"
#include <stdio.h>
#include <stdatomic.h>

/* ──────────────────── Test Helpers ──────────────────── */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [FIBER] %-55s ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ──────────────────── Test: Create and Destroy ──────────────────── */

static void simple_fiber_func(void* data) {
    int* counter = (int*)data;
    (*counter)++;
}

static void test_fiber_create_destroy(void) {
    TEST(fiber_create_destroy);

    lgx_fiber_t fiber = NULL;
    int counter = 0;
    lgx_threading_error_e err = lgx_fiber_create(simple_fiber_func, &counter, 0, &fiber);

    if (err != LGX_THREADING_SUCCESS || !fiber) {
        FAIL("create failed");
        return;
    }

    if (lgx_fiber_get_state(fiber) != LGX_FIBER_STATE_IDLE) {
        lgx_fiber_destroy(fiber);
        FAIL("initial state not IDLE");
        return;
    }

    lgx_fiber_destroy(fiber);
    PASS();
}

/* ──────────────────── Test: Resume and Complete ──────────────────── */

static void test_fiber_resume_complete(void) {
    TEST(fiber_resume_and_complete);

    int counter = 0;
    lgx_fiber_t fiber = NULL;
    lgx_fiber_create(simple_fiber_func, &counter, 0, &fiber);

    /* Resume runs the fiber function to completion */
    lgx_fiber_resume(fiber);

    if (counter != 1) {
        char buf[64];
        snprintf(buf, sizeof(buf), "counter=%d expected=1", counter);
        lgx_fiber_destroy(fiber);
        FAIL(buf);
        return;
    }

    if (lgx_fiber_get_state(fiber) != LGX_FIBER_STATE_COMPLETED) {
        lgx_fiber_destroy(fiber);
        FAIL("state not COMPLETED");
        return;
    }

    lgx_fiber_destroy(fiber);
    PASS();
}

/* ──────────────────── Test: Yield and Resume ──────────────────── */

static int g_yield_steps = 0;

static void yielding_fiber_func(void* data) {
    (void)data;
    g_yield_steps = 1;
    lgx_fiber_yield();
    g_yield_steps = 2;
    lgx_fiber_yield();
    g_yield_steps = 3;
}

static void test_fiber_yield_resume(void) {
    TEST(fiber_yield_resume_3_steps);

    g_yield_steps = 0;
    lgx_fiber_t fiber = NULL;
    lgx_fiber_create(yielding_fiber_func, NULL, 0, &fiber);

    /* Step 1: resume → runs until first yield */
    lgx_fiber_resume(fiber);
    if (g_yield_steps != 1 || lgx_fiber_get_state(fiber) != LGX_FIBER_STATE_SUSPENDED) {
        FAIL("step 1 failed");
        lgx_fiber_destroy(fiber);
        return;
    }

    /* Step 2: resume → runs until second yield */
    lgx_fiber_resume(fiber);
    if (g_yield_steps != 2 || lgx_fiber_get_state(fiber) != LGX_FIBER_STATE_SUSPENDED) {
        FAIL("step 2 failed");
        lgx_fiber_destroy(fiber);
        return;
    }

    /* Step 3: resume → runs to completion */
    lgx_fiber_resume(fiber);
    if (g_yield_steps != 3 || lgx_fiber_get_state(fiber) != LGX_FIBER_STATE_COMPLETED) {
        FAIL("step 3 failed");
        lgx_fiber_destroy(fiber);
        return;
    }

    lgx_fiber_destroy(fiber);
    PASS();
}

/* ──────────────────── Test: Multiple Fibers Interleave ──────────────────── */

static int g_interleave_log[10];
static int g_interleave_idx = 0;

static void interleave_fiber_a(void* data) {
    (void)data;
    g_interleave_log[g_interleave_idx++] = 1;  /* A step 1 */
    lgx_fiber_yield();
    g_interleave_log[g_interleave_idx++] = 3;  /* A step 2 */
}

static void interleave_fiber_b(void* data) {
    (void)data;
    g_interleave_log[g_interleave_idx++] = 2;  /* B step 1 */
    lgx_fiber_yield();
    g_interleave_log[g_interleave_idx++] = 4;  /* B step 2 */
}

static void test_fiber_interleave(void) {
    TEST(fiber_interleave_A_B_A_B);

    g_interleave_idx = 0;

    lgx_fiber_t fa = NULL, fb = NULL;
    lgx_fiber_create(interleave_fiber_a, NULL, 0, &fa);
    lgx_fiber_create(interleave_fiber_b, NULL, 0, &fb);

    /* Round-robin: A, B, A, B */
    lgx_fiber_resume(fa);  /* A runs, yields, log=[1] */
    lgx_fiber_resume(fb);  /* B runs, yields, log=[1,2] */
    lgx_fiber_resume(fa);  /* A resumes, completes, log=[1,2,3] */
    lgx_fiber_resume(fb);  /* B resumes, completes, log=[1,2,3,4] */

    int expected[] = {1, 2, 3, 4};
    int ok = 1;
    for (int i = 0; i < 4; i++) {
        if (g_interleave_log[i] != expected[i]) ok = 0;
    }

    if (ok && g_interleave_idx == 4) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "log=[%d,%d,%d,%d] idx=%d",
                 g_interleave_log[0], g_interleave_log[1],
                 g_interleave_log[2], g_interleave_log[3], g_interleave_idx);
        FAIL(buf);
    }

    lgx_fiber_destroy(fa);
    lgx_fiber_destroy(fb);
}

/* ──────────────────── Test: Fiber Wait on Job ──────────────────── */

static atomic_int g_wait_job_result = 0;

static void wait_job_fiber_func(void* data) {
    lgx_job_handle_t handle = (lgx_job_handle_t)data;
    /* Suspend until connected job completes */
    lgx_fiber_wait_job(handle);
    atomic_store(&g_wait_job_result, 42);
}

static void dummy_job(void* data, uint32_t tid) {
    (void)data; (void)tid;
    /* Just completes */
}

static void test_fiber_wait_job(void) {
    lgx_threading_config_t cfg = LGX_THREADING_CONFIG_INIT;
    cfg.thread_count = 2;
    lgx_threading_init(&cfg);

    TEST(fiber_wait_on_job);
    atomic_store(&g_wait_job_result, 0);

    /* Submit a job */
    lgx_job_desc_t d = LGX_JOB_DESC_INIT;
    d.func = dummy_job;
    lgx_job_handle_t jh = NULL;
    lgx_job_submit(&d, &jh);

    /* Wait for the job to complete on pool */
    lgx_job_wait(jh);

    /* Now create a fiber that "waits" on the already-complete job */
    lgx_fiber_t fiber = NULL;
    lgx_fiber_create(wait_job_fiber_func, jh, 0, &fiber);
    lgx_fiber_resume(fiber);

    /* Since the job was already complete, fiber should run straight through */
    if (atomic_load(&g_wait_job_result) == 42 &&
        lgx_fiber_get_state(fiber) == LGX_FIBER_STATE_COMPLETED) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "result=%d state=%d",
                 atomic_load(&g_wait_job_result), lgx_fiber_get_state(fiber));
        FAIL(buf);
    }

    lgx_fiber_destroy(fiber);
    lgx_threading_shutdown();
}

/* ──────────────────── Test: Yield Outside Fiber Fails ──────────────────── */

static void test_fiber_yield_outside(void) {
    TEST(yield_outside_fiber_returns_error);

    lgx_threading_error_e err = lgx_fiber_yield();
    if (err == LGX_THREADING_ERROR_OPERATION_FAILED) {
        PASS();
    } else {
        FAIL("expected OPERATION_FAILED");
    }
}

/* ──────────────────── Test: Custom Stack Size ──────────────────── */

static void large_stack_func(void* data) {
    /* Use a large stack allocation to verify custom stack works */
    volatile char big[32 * 1024];  /* 32KB on stack */
    big[0] = 1;
    big[32 * 1024 - 1] = 2;
    int* result = (int*)data;
    *result = big[0] + big[32 * 1024 - 1];
}

static void test_fiber_custom_stack(void) {
    TEST(fiber_custom_128KB_stack);

    int result = 0;
    lgx_fiber_t fiber = NULL;
    lgx_fiber_create(large_stack_func, &result, 128 * 1024, &fiber);
    lgx_fiber_resume(fiber);

    if (result == 3 && lgx_fiber_get_state(fiber) == LGX_FIBER_STATE_COMPLETED) {
        PASS();
    } else {
        FAIL("large stack test failed");
    }

    lgx_fiber_destroy(fiber);
}

/* ──────────────────── Main ──────────────────── */

int main(void) {
    printf("LGX Threading Module — Fiber Tests\n");
    printf("===================================\n\n");

    test_fiber_create_destroy();
    test_fiber_resume_complete();
    test_fiber_yield_resume();
    test_fiber_interleave();
    test_fiber_yield_outside();
    test_fiber_custom_stack();
    test_fiber_wait_job();

    printf("\n===================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
