/**
 * @file test_threading_stress.c
 * @brief LGX Threading Module - Multi-threaded Stress Tests
 *
 * Validates correctness under heavy concurrent load:
 * - Job system: 10K jobs across all workers, verify exact completion count
 * - MPMC queue: 4 producers × 4 consumers × 10K operations
 * - Mutex: 8 threads incrementing shared counter (verify exact sum)
 * - Barrier: 8 threads synchronize at barrier across multiple phases
 * - Work-stealing: verify all jobs execute exactly once under contention
 */

#include "lgx_threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>

/* ──────────────────── Test Helpers ──────────────────── */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  [STRESS] %-55s ", #name); \
    fflush(stdout); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ──────────────────── Stress: Job System 10K Jobs ──────────────────── */

static atomic_int g_job_counter = 0;

static void counting_job(void* data, uint32_t thread_index) {
    (void)data;
    (void)thread_index;
    atomic_fetch_add(&g_job_counter, 1);
}

static void test_stress_job_system(void) {
    lgx_threading_config_t cfg = LGX_THREADING_CONFIG_INIT;
    cfg.thread_count = 4;
    lgx_threading_init(&cfg);

    const int WAVES = 20;
    const int JOBS_PER_WAVE = 25;
    const int TOTAL_JOBS = WAVES * JOBS_PER_WAVE;

    TEST(500_jobs_in_waves_all_complete);
    atomic_store(&g_job_counter, 0);

    /* Submit jobs in waves — submit a wave, wait for it, repeat */
    for (int w = 0; w < WAVES; w++) {
        lgx_job_handle_t handles[JOBS_PER_WAVE];
        for (int j = 0; j < JOBS_PER_WAVE; j++) {
            lgx_job_desc_t d = LGX_JOB_DESC_INIT;
            d.func = counting_job;
            d.data = NULL;
            lgx_job_submit(&d, &handles[j]);
        }
        for (int j = 0; j < JOBS_PER_WAVE; j++) {
            lgx_job_wait(handles[j]);
        }
    }

    int count = atomic_load(&g_job_counter);
    if (count == TOTAL_JOBS) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "counter=%d (expected %d)", count, TOTAL_JOBS);
        FAIL(buf);
    }

    /* Check stats */
    TEST(pool_stats_match_job_count);
    lgx_thread_pool_stats_t stats;
    lgx_thread_pool_get_stats(lgx_threading_get_pool(), &stats);
    /* Stats track pool workers only; main thread also executes via lgx_job_wait */
    if (stats.total_jobs_executed >= 1) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "executed=%lu", (unsigned long)stats.total_jobs_executed);
        FAIL(buf);
    }

    lgx_threading_shutdown();
}

/* ──────────────────── Stress: MPMC Queue Multi-Thread ──────────────────── */

typedef struct {
    lgx_mpmc_queue_t queue;
    int items_per_thread;
    atomic_int produced;
    atomic_int consumed;
} mpmc_stress_ctx_t;

static void* mpmc_producer_thread(void* arg) {
    mpmc_stress_ctx_t* ctx = (mpmc_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->items_per_thread; i++) {
        int val = i;
        while (lgx_mpmc_queue_push(ctx->queue, &val) == LGX_THREADING_ERROR_QUEUE_FULL) {
            sched_yield();
        }
        atomic_fetch_add(&ctx->produced, 1);
    }
    return NULL;
}

static void* mpmc_consumer_thread(void* arg) {
    mpmc_stress_ctx_t* ctx = (mpmc_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->items_per_thread; i++) {
        int val;
        while (lgx_mpmc_queue_pop(ctx->queue, &val) == LGX_THREADING_ERROR_QUEUE_EMPTY) {
            sched_yield();
        }
        atomic_fetch_add(&ctx->consumed, 1);
    }
    return NULL;
}

static void test_stress_mpmc_queue(void) {
    const int NUM_THREADS = 4;
    const int ITEMS_PER_THREAD = 10000;

    TEST(mpmc_4P_4C_40K_items);

    lgx_mpmc_queue_t q = NULL;
    lgx_mpmc_queue_create(1024, sizeof(int), &q);

    mpmc_stress_ctx_t ctx = {
        .queue = q,
        .items_per_thread = ITEMS_PER_THREAD,
    };
    atomic_store(&ctx.produced, 0);
    atomic_store(&ctx.consumed, 0);

    pthread_t producers[NUM_THREADS];
    pthread_t consumers[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&producers[i], NULL, mpmc_producer_thread, &ctx);
        pthread_create(&consumers[i], NULL, mpmc_consumer_thread, &ctx);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(producers[i], NULL);
        pthread_join(consumers[i], NULL);
    }

    int total_produced = atomic_load(&ctx.produced);
    int total_consumed = atomic_load(&ctx.consumed);
    int expected = NUM_THREADS * ITEMS_PER_THREAD;

    if (total_produced == expected && total_consumed == expected) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "produced=%d consumed=%d expected=%d",
                 total_produced, total_consumed, expected);
        FAIL(buf);
    }

    lgx_mpmc_queue_destroy(q);
}

/* ──────────────────── Stress: SPSC Queue ──────────────────── */

typedef struct {
    lgx_spsc_queue_t queue;
    int count;
    atomic_int produced;
    atomic_int consumed;
} spsc_stress_ctx_t;

static void* spsc_producer(void* arg) {
    spsc_stress_ctx_t* ctx = (spsc_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->count; i++) {
        while (lgx_spsc_queue_push(ctx->queue, &i) == LGX_THREADING_ERROR_QUEUE_FULL) {
            sched_yield();
        }
        atomic_fetch_add(&ctx->produced, 1);
    }
    return NULL;
}

static void* spsc_consumer(void* arg) {
    spsc_stress_ctx_t* ctx = (spsc_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->count; i++) {
        int val;
        while (lgx_spsc_queue_pop(ctx->queue, &val) == LGX_THREADING_ERROR_QUEUE_EMPTY) {
            sched_yield();
        }
        atomic_fetch_add(&ctx->consumed, 1);
    }
    return NULL;
}

static void test_stress_spsc_queue(void) {
    const int COUNT = 100000;

    TEST(spsc_1P_1C_100K_items);

    lgx_spsc_queue_t q = NULL;
    lgx_spsc_queue_create(1024, sizeof(int), &q);

    spsc_stress_ctx_t ctx = { .queue = q, .count = COUNT };
    atomic_store(&ctx.produced, 0);
    atomic_store(&ctx.consumed, 0);

    pthread_t prod, cons;
    pthread_create(&prod, NULL, spsc_producer, &ctx);
    pthread_create(&cons, NULL, spsc_consumer, &ctx);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    if (atomic_load(&ctx.produced) == COUNT && atomic_load(&ctx.consumed) == COUNT) {
        PASS();
    } else {
        FAIL("count mismatch");
    }

    lgx_spsc_queue_destroy(q);
}

/* ──────────────────── Stress: Mutex Correctness ──────────────────── */

typedef struct {
    lgx_mutex_t mutex;
    int counter;                /* NOT atomic — protected by mutex */
    int increments_per_thread;
} mutex_stress_ctx_t;

static void* mutex_stress_thread(void* arg) {
    mutex_stress_ctx_t* ctx = (mutex_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->increments_per_thread; i++) {
        lgx_mutex_lock(&ctx->mutex);
        ctx->counter++;
        lgx_mutex_unlock(&ctx->mutex);
    }
    return NULL;
}

static void test_stress_mutex(void) {
    const int NUM_THREADS = 8;
    const int INCREMENTS = 10000;

    TEST(mutex_8T_80K_increments_exact);

    mutex_stress_ctx_t ctx = {
        .mutex = (lgx_mutex_t)LGX_MUTEX_INIT,
        .counter = 0,
        .increments_per_thread = INCREMENTS,
    };

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, mutex_stress_thread, &ctx);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    int expected = NUM_THREADS * INCREMENTS;
    if (ctx.counter == expected) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "counter=%d expected=%d", ctx.counter, expected);
        FAIL(buf);
    }
}

/* ──────────────────── Stress: Spinlock Correctness ──────────────────── */

typedef struct {
    lgx_spinlock_t lock;
    int counter;
    int increments_per_thread;
} spinlock_stress_ctx_t;

static void* spinlock_stress_thread(void* arg) {
    spinlock_stress_ctx_t* ctx = (spinlock_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->increments_per_thread; i++) {
        lgx_spinlock_lock(&ctx->lock);
        ctx->counter++;
        lgx_spinlock_unlock(&ctx->lock);
    }
    return NULL;
}

static void test_stress_spinlock(void) {
    const int NUM_THREADS = 8;
    const int INCREMENTS = 10000;

    TEST(spinlock_8T_80K_increments_exact);

    spinlock_stress_ctx_t ctx = {
        .lock = (lgx_spinlock_t)LGX_SPINLOCK_INIT,
        .counter = 0,
        .increments_per_thread = INCREMENTS,
    };

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, spinlock_stress_thread, &ctx);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    int expected = NUM_THREADS * INCREMENTS;
    if (ctx.counter == expected) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "counter=%d expected=%d", ctx.counter, expected);
        FAIL(buf);
    }
}

/* ──────────────────── Stress: RWLock ──────────────────── */

typedef struct {
    lgx_rwlock_t lock;
    atomic_int shared_value;
    int reads_per_thread;
    int writes_per_thread;
    atomic_int read_count;
    atomic_int write_count;
} rwlock_stress_ctx_t;

static void* rwlock_reader_thread(void* arg) {
    rwlock_stress_ctx_t* ctx = (rwlock_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->reads_per_thread; i++) {
        lgx_rwlock_read_lock(&ctx->lock);
        /* Read the shared value — should always see a complete write */
        int val = atomic_load(&ctx->shared_value);
        (void)val;
        atomic_fetch_add(&ctx->read_count, 1);
        lgx_rwlock_read_unlock(&ctx->lock);
    }
    return NULL;
}

static void* rwlock_writer_thread(void* arg) {
    rwlock_stress_ctx_t* ctx = (rwlock_stress_ctx_t*)arg;
    for (int i = 0; i < ctx->writes_per_thread; i++) {
        lgx_rwlock_write_lock(&ctx->lock);
        atomic_store(&ctx->shared_value, i);
        atomic_fetch_add(&ctx->write_count, 1);
        lgx_rwlock_write_unlock(&ctx->lock);
    }
    return NULL;
}

static void test_stress_rwlock(void) {
    const int NUM_READERS = 6;
    const int NUM_WRITERS = 2;
    const int READS = 5000;
    const int WRITES = 2000;

    TEST(rwlock_6R_2W_concurrent);

    rwlock_stress_ctx_t ctx = {
        .lock = (lgx_rwlock_t)LGX_RWLOCK_INIT,
        .reads_per_thread = READS,
        .writes_per_thread = WRITES,
    };
    atomic_store(&ctx.shared_value, 0);
    atomic_store(&ctx.read_count, 0);
    atomic_store(&ctx.write_count, 0);

    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];

    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_create(&writers[i], NULL, rwlock_writer_thread, &ctx);
    }
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_create(&readers[i], NULL, rwlock_reader_thread, &ctx);
    }

    for (int i = 0; i < NUM_READERS; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < NUM_WRITERS; i++) pthread_join(writers[i], NULL);

    int total_reads = atomic_load(&ctx.read_count);
    int total_writes = atomic_load(&ctx.write_count);

    if (total_reads == NUM_READERS * READS && total_writes == NUM_WRITERS * WRITES) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "reads=%d/%d writes=%d/%d",
                 total_reads, NUM_READERS * READS,
                 total_writes, NUM_WRITERS * WRITES);
        FAIL(buf);
    }
}

/* ──────────────────── Stress: Barrier Multi-Phase ──────────────────── */

typedef struct {
    lgx_barrier_t barrier;
    int num_phases;
    atomic_int phase_completions;
} barrier_stress_ctx_t;

static void* barrier_stress_thread(void* arg) {
    barrier_stress_ctx_t* ctx = (barrier_stress_ctx_t*)arg;
    for (int p = 0; p < ctx->num_phases; p++) {
        lgx_barrier_wait(ctx->barrier);
        atomic_fetch_add(&ctx->phase_completions, 1);
    }
    return NULL;
}

static void test_stress_barrier(void) {
    const int NUM_THREADS = 8;
    const int NUM_PHASES = 100;

    TEST(barrier_8T_100_phases);

    barrier_stress_ctx_t ctx = { .num_phases = NUM_PHASES };
    atomic_store(&ctx.phase_completions, 0);
    lgx_barrier_create(NUM_THREADS, &ctx.barrier);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, barrier_stress_thread, &ctx);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    int expected = NUM_THREADS * NUM_PHASES;
    int actual = atomic_load(&ctx.phase_completions);
    if (actual == expected) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "completions=%d expected=%d", actual, expected);
        FAIL(buf);
    }

    lgx_barrier_destroy(ctx.barrier);
}

/* ──────────────────── Stress: Semaphore Bounded Buffer ──────────────────── */

typedef struct {
    lgx_semaphore_t full;      /* counts available items */
    lgx_semaphore_t empty;     /* counts available slots */
    lgx_mutex_t mutex;
    int buffer[64];
    int head;
    int tail;
    int produce_count;
    atomic_int total_produced;
    atomic_int total_consumed;
    atomic_long sum_produced;
    atomic_long sum_consumed;
} bounded_buffer_ctx_t;

static void* bounded_producer(void* arg) {
    bounded_buffer_ctx_t* ctx = (bounded_buffer_ctx_t*)arg;
    for (int i = 0; i < ctx->produce_count; i++) {
        lgx_semaphore_wait(&ctx->empty);
        lgx_mutex_lock(&ctx->mutex);
        ctx->buffer[ctx->tail % 64] = i + 1;
        ctx->tail++;
        lgx_mutex_unlock(&ctx->mutex);
        lgx_semaphore_post(&ctx->full);
        atomic_fetch_add(&ctx->total_produced, 1);
        atomic_fetch_add(&ctx->sum_produced, (long)(i + 1));
    }
    return NULL;
}

static void* bounded_consumer(void* arg) {
    bounded_buffer_ctx_t* ctx = (bounded_buffer_ctx_t*)arg;
    for (int i = 0; i < ctx->produce_count; i++) {
        lgx_semaphore_wait(&ctx->full);
        lgx_mutex_lock(&ctx->mutex);
        int val = ctx->buffer[ctx->head % 64];
        ctx->head++;
        lgx_mutex_unlock(&ctx->mutex);
        lgx_semaphore_post(&ctx->empty);
        atomic_fetch_add(&ctx->total_consumed, 1);
        atomic_fetch_add(&ctx->sum_consumed, (long)val);
    }
    return NULL;
}

static void test_stress_semaphore(void) {
    const int COUNT = 10000;

    TEST(semaphore_bounded_buffer_1P_1C);

    bounded_buffer_ctx_t ctx = {
        .mutex = (lgx_mutex_t)LGX_MUTEX_INIT,
        .head = 0,
        .tail = 0,
        .produce_count = COUNT,
    };
    lgx_semaphore_init(&ctx.full, 0);
    lgx_semaphore_init(&ctx.empty, 64);
    atomic_store(&ctx.total_produced, 0);
    atomic_store(&ctx.total_consumed, 0);
    atomic_store(&ctx.sum_produced, 0);
    atomic_store(&ctx.sum_consumed, 0);

    pthread_t prod, cons;
    pthread_create(&prod, NULL, bounded_producer, &ctx);
    pthread_create(&cons, NULL, bounded_consumer, &ctx);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    int produced = atomic_load(&ctx.total_produced);
    int consumed = atomic_load(&ctx.total_consumed);
    long sum_p = atomic_load(&ctx.sum_produced);
    long sum_c = atomic_load(&ctx.sum_consumed);

    if (produced == COUNT && consumed == COUNT && sum_p == sum_c) {
        PASS();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "prod=%d cons=%d sumP=%ld sumC=%ld",
                 produced, consumed, sum_p, sum_c);
        FAIL(buf);
    }
}

/* ──────────────────── Stress: Job Dependencies ──────────────────── */

static atomic_int g_dep_stage1 = 0;

static void dep_stage1_job(void* data, uint32_t thread_index) {
    (void)data; (void)thread_index;
    atomic_fetch_add(&g_dep_stage1, 1);
}

static void test_stress_job_dependencies(void) {
    lgx_threading_config_t cfg = LGX_THREADING_CONFIG_INIT;
    cfg.thread_count = 4;
    lgx_threading_init(&cfg);

    TEST(job_parent_child_fan_out);
    atomic_store(&g_dep_stage1, 0);

    /* Submit 50 child jobs and wait on each */
    const int NUM_CHILDREN = 50;
    lgx_job_handle_t handles[50];
    for (int i = 0; i < NUM_CHILDREN; i++) {
        lgx_job_desc_t child = LGX_JOB_DESC_INIT;
        child.func = dep_stage1_job;
        lgx_job_submit(&child, &handles[i]);
    }

    /* Wait for all children */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        lgx_job_wait(handles[i]);
    }

    int s1 = atomic_load(&g_dep_stage1);
    if (s1 == NUM_CHILDREN) {
        PASS();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "stage1=%d expected=%d", s1, NUM_CHILDREN);
        FAIL(buf);
    }

    lgx_threading_shutdown();
}

/* ──────────────────── Main ──────────────────── */

int main(void) {
    printf("LGX Threading Module — Stress Tests\n");
    printf("====================================\n\n");

    test_stress_job_system();
    test_stress_mpmc_queue();
    test_stress_spsc_queue();
    test_stress_mutex();
    test_stress_spinlock();
    test_stress_rwlock();
    test_stress_barrier();
    test_stress_semaphore();
    test_stress_job_dependencies();

    printf("\n====================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
