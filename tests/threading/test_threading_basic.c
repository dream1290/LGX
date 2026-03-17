/**
 * @file test_threading_basic.c
 * @brief LGX Threading Module - Phase 0 Validation Test
 *
 * Tests: init → version check → job submit/wait → sync primitives → shutdown
 */

#include "lgx_threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>

/* ──────────────────── Test Helpers ──────────────────── */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [TEST] %-50s ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ──────────────────── Test: Version ──────────────────── */

static void test_version(void) {
    TEST(version_returns_1_1_0);
    uint32_t major = 0, minor = 0, patch = 0;
    lgx_threading_version(&major, &minor, &patch);
    if (major == 1 && minor == 1 && patch == 0) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "got %u.%u.%u", major, minor, patch);
        FAIL(buf);
    }
}

/* ──────────────────── Test: Error Strings ──────────────────── */

static void test_error_strings(void) {
    TEST(error_string_success);
    const char* s = lgx_threading_error_string(LGX_THREADING_SUCCESS);
    if (s && strlen(s) > 0) { PASS(); } else { FAIL("empty string"); }

    TEST(error_string_all_codes);
    int all_ok = 1;
    lgx_threading_error_e codes[] = {
        LGX_THREADING_ERROR_INVALID_PARAM,
        LGX_THREADING_ERROR_OUT_OF_MEMORY,
        LGX_THREADING_ERROR_NOT_INITIALIZED,
        LGX_THREADING_ERROR_QUEUE_FULL,
        LGX_THREADING_ERROR_QUEUE_EMPTY,
        LGX_THREADING_ERROR_TIMEOUT,
        LGX_THREADING_ERROR_DEADLOCK,
        LGX_THREADING_ERROR_FIBER_OVERFLOW,
        LGX_THREADING_ERROR_POOL_SHUTDOWN,
    };
    for (size_t i = 0; i < sizeof(codes)/sizeof(codes[0]); i++) {
        const char* str = lgx_threading_error_string(codes[i]);
        if (!str || strlen(str) == 0) { all_ok = 0; break; }
    }
    if (all_ok) { PASS(); } else { FAIL("missing error string"); }
}

/* ──────────────────── Test: Init / Shutdown ──────────────────── */

static void test_init_shutdown(void) {
    TEST(init_success);
    lgx_threading_config_t cfg = LGX_THREADING_CONFIG_INIT;
    cfg.thread_count = 2;  /* Small for testing */
    lgx_threading_error_e err = lgx_threading_init(&cfg);
    if (err == LGX_THREADING_SUCCESS) { PASS(); } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "err=%d detail=%s",
                 err, lgx_threading_get_last_error_detail());
        FAIL(buf);
    }

    TEST(double_init_fails);
    err = lgx_threading_init(&cfg);
    if (err == LGX_THREADING_ERROR_ALREADY_INIT) { PASS(); } else { FAIL("expected ALREADY_INIT"); }

    TEST(get_pool_not_null);
    lgx_thread_pool_t pool = lgx_threading_get_pool();
    if (pool != NULL) { PASS(); } else { FAIL("pool is NULL"); }

    TEST(pool_stats);
    lgx_thread_pool_stats_t stats;
    err = lgx_thread_pool_get_stats(pool, &stats);
    if (err == LGX_THREADING_SUCCESS && stats.total_threads == 2) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "threads=%u", stats.total_threads);
        FAIL(buf);
    }

    TEST(shutdown_success);
    err = lgx_threading_shutdown();
    if (err == LGX_THREADING_SUCCESS) { PASS(); } else { FAIL("shutdown failed"); }

    TEST(double_shutdown_fails);
    err = lgx_threading_shutdown();
    if (err == LGX_THREADING_ERROR_NOT_INITIALIZED) { PASS(); } else { FAIL("expected NOT_INITIALIZED"); }
}

/* ──────────────────── Test: Job System ──────────────────── */

static atomic_int g_counter = 0;

static void increment_job(void* data, uint32_t thread_index) {
    (void)thread_index;
    int* val = (int*)data;
    atomic_fetch_add(&g_counter, *val);
}

static void test_job_system(void) {
    lgx_threading_config_t cfg = LGX_THREADING_CONFIG_INIT;
    cfg.thread_count = 2;
    lgx_threading_init(&cfg);

    /* Single job */
    TEST(job_submit_and_wait);
    atomic_store(&g_counter, 0);
    int val = 42;
    lgx_job_desc_t desc = LGX_JOB_DESC_INIT;
    desc.func = increment_job;
    desc.data = &val;

    lgx_job_handle_t handle = NULL;
    lgx_threading_error_e err = lgx_job_submit(&desc, &handle);
    if (err != LGX_THREADING_SUCCESS) { FAIL("submit failed"); return; }

    err = lgx_job_wait(handle);
    if (err == LGX_THREADING_SUCCESS && atomic_load(&g_counter) == 42) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "counter=%d", atomic_load(&g_counter));
        FAIL(buf);
    }

    /* Batch submit */
    TEST(batch_submit_100_jobs);
    atomic_store(&g_counter, 0);
    int ones[100];
    lgx_job_desc_t descs[100];
    lgx_job_handle_t handles[100];

    for (int i = 0; i < 100; i++) {
        ones[i] = 1;
        lgx_job_desc_t d = LGX_JOB_DESC_INIT;
        d.func = increment_job;
        d.data = &ones[i];
        descs[i] = d;
    }

    err = lgx_job_submit_batch(descs, 100, handles);
    if (err != LGX_THREADING_SUCCESS) { FAIL("batch submit failed"); goto cleanup; }

    /* Wait for all */
    for (int i = 0; i < 100; i++) {
        lgx_job_wait(handles[i]);
    }

    if (atomic_load(&g_counter) == 100) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "counter=%d (expected 100)", atomic_load(&g_counter));
        FAIL(buf);
    }

    TEST(is_complete);
    if (lgx_job_is_complete(handles[0])) { PASS(); } else { FAIL("not complete"); }

cleanup:
    lgx_threading_shutdown();
}

/* ──────────────────── Test: MPMC Queue ──────────────────── */

static void test_mpmc_queue(void) {
    TEST(mpmc_create_destroy);
    lgx_mpmc_queue_t q = NULL;
    lgx_threading_error_e err = lgx_mpmc_queue_create(16, sizeof(int), &q);
    if (err == LGX_THREADING_SUCCESS && q != NULL) { PASS(); } else { FAIL("create failed"); return; }

    TEST(mpmc_push_pop);
    int in = 42, out = 0;
    err = lgx_mpmc_queue_push(q, &in);
    if (err != LGX_THREADING_SUCCESS) { FAIL("push failed"); lgx_mpmc_queue_destroy(q); return; }

    err = lgx_mpmc_queue_pop(q, &out);
    if (err == LGX_THREADING_SUCCESS && out == 42) { PASS(); } else { FAIL("pop mismatch"); }

    TEST(mpmc_empty_pop);
    err = lgx_mpmc_queue_pop(q, &out);
    if (err == LGX_THREADING_ERROR_QUEUE_EMPTY) { PASS(); } else { FAIL("expected QUEUE_EMPTY"); }

    TEST(mpmc_fill_and_full);
    for (int i = 0; i < 16; i++) {
        lgx_mpmc_queue_push(q, &i);
    }
    int extra = 99;
    err = lgx_mpmc_queue_push(q, &extra);
    if (err == LGX_THREADING_ERROR_QUEUE_FULL) { PASS(); } else { FAIL("expected QUEUE_FULL"); }

    TEST(mpmc_size);
    size_t sz = lgx_mpmc_queue_size(q);
    if (sz == 16) { PASS(); } else {
        char buf[32]; snprintf(buf, sizeof(buf), "size=%zu", sz); FAIL(buf);
    }

    lgx_mpmc_queue_destroy(q);
}

/* ──────────────────── Test: SPSC Queue ──────────────────── */

static void test_spsc_queue(void) {
    TEST(spsc_create_destroy);
    lgx_spsc_queue_t q = NULL;
    lgx_threading_error_e err = lgx_spsc_queue_create(8, sizeof(int), &q);
    if (err == LGX_THREADING_SUCCESS && q != NULL) { PASS(); } else { FAIL("create failed"); return; }

    TEST(spsc_push_pop);
    int in = 7, out = 0;
    lgx_spsc_queue_push(q, &in);
    err = lgx_spsc_queue_pop(q, &out);
    if (err == LGX_THREADING_SUCCESS && out == 7) { PASS(); } else { FAIL("mismatch"); }

    TEST(spsc_empty);
    err = lgx_spsc_queue_pop(q, &out);
    if (err == LGX_THREADING_ERROR_QUEUE_EMPTY) { PASS(); } else { FAIL("expected empty"); }

    lgx_spsc_queue_destroy(q);
}

/* ──────────────────── Test: Mutex ──────────────────── */

static void test_mutex(void) {
    TEST(mutex_lock_unlock);
    lgx_mutex_t mtx = LGX_MUTEX_INIT;
    lgx_threading_error_e err = lgx_mutex_lock(&mtx);
    if (err == LGX_THREADING_SUCCESS) {
        lgx_mutex_unlock(&mtx);
        PASS();
    } else {
        FAIL("lock failed");
    }

    TEST(mutex_try_lock);
    err = lgx_mutex_try_lock(&mtx);
    if (err == LGX_THREADING_SUCCESS) {
        /* Now try_lock should fail (we hold it) */
        lgx_threading_error_e err2 = lgx_mutex_try_lock(&mtx);
        lgx_mutex_unlock(&mtx);
        if (err2 == LGX_THREADING_ERROR_TIMEOUT) { PASS(); } else { FAIL("expected timeout"); }
    } else {
        FAIL("first try_lock failed");
    }
}

/* ──────────────────── Test: Spinlock ──────────────────── */

static void test_spinlock(void) {
    TEST(spinlock_lock_unlock);
    lgx_spinlock_t sl = LGX_SPINLOCK_INIT;
    lgx_spinlock_lock(&sl);
    lgx_spinlock_unlock(&sl);
    PASS();

    TEST(spinlock_try_lock);
    if (lgx_spinlock_try_lock(&sl)) {
        bool second = lgx_spinlock_try_lock(&sl);
        lgx_spinlock_unlock(&sl);
        if (!second) { PASS(); } else { FAIL("double lock succeeded"); }
    } else {
        FAIL("try_lock failed");
    }
}

/* ──────────────────── Test: Barrier ──────────────────── */

static void test_barrier(void) {
    TEST(barrier_single_thread);
    lgx_barrier_t bar = NULL;
    lgx_threading_error_e err = lgx_barrier_create(1, &bar);
    if (err != LGX_THREADING_SUCCESS) { FAIL("create failed"); return; }
    err = lgx_barrier_wait(bar);
    if (err == LGX_THREADING_SUCCESS) { PASS(); } else { FAIL("wait failed"); }
    lgx_barrier_destroy(bar);
}

/* ──────────────────── Test: Semaphore ──────────────────── */

static void test_semaphore(void) {
    TEST(semaphore_basic);
    lgx_semaphore_t sem;
    lgx_semaphore_init(&sem, 2);

    lgx_threading_error_e e1 = lgx_semaphore_wait(&sem);
    lgx_threading_error_e e2 = lgx_semaphore_wait(&sem);
    lgx_threading_error_e e3 = lgx_semaphore_try_wait(&sem);

    if (e1 == LGX_THREADING_SUCCESS &&
        e2 == LGX_THREADING_SUCCESS &&
        e3 == LGX_THREADING_ERROR_TIMEOUT) {
        lgx_semaphore_post(&sem);
        lgx_threading_error_e e4 = lgx_semaphore_try_wait(&sem);
        if (e4 == LGX_THREADING_SUCCESS) { PASS(); } else { FAIL("post+wait failed"); }
    } else {
        FAIL("basic semaphore ops failed");
    }
}

/* ──────────────────── Test: RWLock ──────────────────── */

static void test_rwlock(void) {
    TEST(rwlock_read_read);
    lgx_rwlock_t rw = LGX_RWLOCK_INIT;
    lgx_rwlock_read_lock(&rw);
    lgx_rwlock_read_lock(&rw);  /* Second reader should succeed */
    lgx_rwlock_read_unlock(&rw);
    lgx_rwlock_read_unlock(&rw);
    PASS();

    TEST(rwlock_write);
    lgx_rwlock_write_lock(&rw);
    lgx_rwlock_write_unlock(&rw);
    PASS();
}

/* ──────────────────── Main ──────────────────── */

int main(void) {
    printf("LGX Threading Module — Phase 0 Tests\n");
    printf("=====================================\n\n");

    test_version();
    test_error_strings();
    test_init_shutdown();
    test_job_system();
    test_mpmc_queue();
    test_spsc_queue();
    test_mutex();
    test_spinlock();
    test_barrier();
    test_semaphore();
    test_rwlock();

    printf("\n=====================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
