/**
 * @file lgx_threading.c
 * @brief LGX Threading Module - Core Implementation
 *
 * Module lifecycle, error handling, version query, and thread pool management.
 */

#define _GNU_SOURCE  /* for pthread_setname_np, sched_setaffinity */

#include "lgx_threading_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>


/* ──────────────────── Thread-Local Error Detail ──────────────────── */

static _Thread_local char g_error_detail[LGX_THREADING_ERROR_DETAIL_SIZE] = {0};

void lgx_threading_set_error_detail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_error_detail, sizeof(g_error_detail), fmt, args);
    va_end(args);
}

/* ──────────────────── Module State ──────────────────── */

static lgx_threading_state_t g_state = { .initialized = false };

/* ──────────────────── Time ──────────────────── */

uint64_t lgx_threading_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ──────────────────── NUMA Detection ──────────────────── */

void lgx_threading_detect_numa(lgx_numa_topology_t* topo) {
    memset(topo, 0, sizeof(*topo));

    /* Count online CPUs */
    long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpus < 1) ncpus = 1;
    if (ncpus > LGX_THREADING_MAX_CORES) ncpus = LGX_THREADING_MAX_CORES;
    topo->total_cores = (uint32_t)ncpus;

    /* Try to detect NUMA nodes from sysfs */
    char path[256];
    uint32_t node_count = 0;

    for (uint32_t node = 0; node < LGX_THREADING_MAX_NUMA_NODES; node++) {
        snprintf(path, sizeof(path), "/sys/devices/system/node/node%u", node);
        if (access(path, F_OK) != 0) break;
        node_count++;
    }

    if (node_count == 0) {
        /* Non-NUMA fallback: 1 node, all cores */
        topo->node_count = 1;
        topo->cores_per_node[0] = topo->total_cores;
        for (uint32_t c = 0; c < topo->total_cores; c++) {
            topo->core_to_node[c] = 0;
        }
        return;
    }

    topo->node_count = node_count;

    /* Parse CPU lists per node */
    for (uint32_t node = 0; node < node_count; node++) {
        snprintf(path, sizeof(path),
                 "/sys/devices/system/node/node%u/cpulist", node);
        FILE* f = fopen(path, "r");
        if (!f) continue;

        char buf[512];
        if (fgets(buf, sizeof(buf), f)) {
            /* Parse comma-separated ranges like "0-3,8-11" */
            char* tok = strtok(buf, ",\n");
            while (tok) {
                uint32_t lo, hi;
                if (sscanf(tok, "%u-%u", &lo, &hi) == 2) {
                    for (uint32_t c = lo; c <= hi && c < LGX_THREADING_MAX_CORES; c++) {
                        topo->core_to_node[c] = node;
                        topo->cores_per_node[node]++;
                    }
                } else if (sscanf(tok, "%u", &lo) == 1) {
                    if (lo < LGX_THREADING_MAX_CORES) {
                        topo->core_to_node[lo] = node;
                        topo->cores_per_node[node]++;
                    }
                }
                tok = strtok(NULL, ",\n");
            }
        }
        fclose(f);
    }
}

/* ──────────────────── Work-Stealing Deque ──────────────────── */

void lgx_work_deque_init(lgx_work_deque_t* dq, size_t capacity) {
    /* Round up to power of 2 */
    size_t cap = 1;
    while (cap < capacity) cap <<= 1;

    dq->buffer = calloc(cap, sizeof(lgx_job_internal_t*));
    dq->capacity = cap;
    dq->mask = cap - 1;
    atomic_store(&dq->top, 0);
    atomic_store(&dq->bottom, 0);
}

void lgx_work_deque_destroy(lgx_work_deque_t* dq) {
    free(dq->buffer);
    dq->buffer = NULL;
}

/* Push to bottom (owner thread only — no contention) */
void lgx_work_deque_push(lgx_work_deque_t* dq, lgx_job_internal_t* job) {
    int64_t b = atomic_load_explicit(&dq->bottom, memory_order_relaxed);
    dq->buffer[b & dq->mask] = job;
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&dq->bottom, b + 1, memory_order_relaxed);
}

/* Pop from bottom (owner thread only — LIFO, hot cache) */
lgx_job_internal_t* lgx_work_deque_pop(lgx_work_deque_t* dq) {
    int64_t b = atomic_load_explicit(&dq->bottom, memory_order_relaxed) - 1;
    atomic_store_explicit(&dq->bottom, b, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    int64_t t = atomic_load_explicit(&dq->top, memory_order_relaxed);

    if (t <= b) {
        lgx_job_internal_t* job = dq->buffer[b & dq->mask];
        if (t == b) {
            /* Last item — race with steal */
            if (!atomic_compare_exchange_strong_explicit(
                    &dq->top, &t, t + 1,
                    memory_order_seq_cst, memory_order_relaxed)) {
                job = NULL;  /* lost race */
            }
            atomic_store_explicit(&dq->bottom, t + 1, memory_order_relaxed);
        }
        return job;
    }
    /* Empty */
    atomic_store_explicit(&dq->bottom, t, memory_order_relaxed);
    return NULL;
}

/* Steal from top (other threads — FIFO, load balance) */
lgx_job_internal_t* lgx_work_deque_steal(lgx_work_deque_t* dq) {
    int64_t t = atomic_load_explicit(&dq->top, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    int64_t b = atomic_load_explicit(&dq->bottom, memory_order_acquire);

    if (t < b) {
        lgx_job_internal_t* job = dq->buffer[t & dq->mask];
        if (!atomic_compare_exchange_strong_explicit(
                &dq->top, &t, t + 1,
                memory_order_seq_cst, memory_order_relaxed)) {
            return NULL;  /* lost CAS race */
        }
        return job;
    }
    return NULL;
}

/* ──────────────────── Worker Thread ──────────────────── */

static void* worker_entry(void* arg) {
    lgx_worker_t* self = (lgx_worker_t*)arg;
    struct lgx_thread_pool_s* pool = &g_state.pool;

    while (!atomic_load_explicit(&pool->shutdown_requested, memory_order_acquire)) {
        lgx_job_internal_t* job = NULL;

        /* 1. Drain global submission queue into own deque */
        for (;;) {
            uint64_t tail = atomic_load_explicit(&pool->submit_tail, memory_order_relaxed);
            uint64_t head = atomic_load_explicit(&pool->submit_head, memory_order_acquire);
            if (tail >= head) break;
            if (atomic_compare_exchange_weak_explicit(
                    &pool->submit_tail, &tail, tail + 1,
                    memory_order_acq_rel, memory_order_relaxed)) {
                /* Spin-wait for producer to write job pointer (immediate after fetch_add) */
                lgx_job_internal_t* j;
                while (!(j = pool->submit_queue[tail % LGX_THREADING_SUBMIT_QUEUE_CAP])) {
                    /* Producer writes right after fetch_add — 1-2 cycle spin */
                }
                pool->submit_queue[tail % LGX_THREADING_SUBMIT_QUEUE_CAP] = NULL;  /* clear for reuse */
                lgx_work_deque_push(&self->deque, j);
            }
        }

        /* 2. Pop from own deque (LIFO — cache hot) */
        job = lgx_work_deque_pop(&self->deque);

        /* 3. If empty, try stealing from a random victim */
        if (!job) {
            for (uint32_t attempt = 0; attempt < pool->worker_count; attempt++) {
                uint32_t victim = (self->index + attempt + 1) % pool->worker_count;
                job = lgx_work_deque_steal(&pool->workers[victim].deque);
                if (job) {
                    atomic_fetch_add(&self->steals_performed, 1);
                    break;
                }
            }
        }

        /* 3. If we got a job, execute it */
        if (job) {
            job->worker_index = self->index;
            job->start_time_ns = lgx_threading_now_ns();

            job->func(job->data, self->index);

            /* Mark complete */
            atomic_store_explicit(&job->completion_flag, 1, memory_order_release);

            /* Propagate completion to parent */
            if (job->parent) {
                uint32_t remaining = atomic_fetch_sub(&job->parent->remaining_children, 1);
                if (remaining == 1) {
                    /* All children done — mark parent complete */
                    atomic_store_explicit(&job->parent->completion_flag, 1,
                                          memory_order_release);
                }
            }

            atomic_fetch_add(&self->jobs_executed, 1);
        } else {
            /* 4. No work — spin briefly before parking */
            bool found_late = false;
            for (int spin = 0; spin < 1000; spin++) {
                /* Re-check own deque */
                job = lgx_work_deque_pop(&self->deque);
                if (job) { found_late = true; break; }
                /* Try stealing again */
                for (uint32_t v = 0; v < pool->worker_count; v++) {
                    if (v == self->index) continue;
                    job = lgx_work_deque_steal(&pool->workers[v].deque);
                    if (job) { found_late = true; break; }
                }
                if (found_late) break;
#if defined(__x86_64__) || defined(__i386__)
                __builtin_ia32_pause();
#elif defined(__aarch64__)
                __asm__ __volatile__("yield");
#endif
            }

            if (found_late && job) {
                /* Found work during spin — execute it */
                job->worker_index = self->index;
                job->start_time_ns = lgx_threading_now_ns();
                job->func(job->data, self->index);
                atomic_store_explicit(&job->completion_flag, 1, memory_order_release);
                if (job->parent) {
                    uint32_t remaining = atomic_fetch_sub(&job->parent->remaining_children, 1);
                    if (remaining == 1) {
                        atomic_store_explicit(&job->parent->completion_flag, 1,
                                              memory_order_release);
                    }
                }
                atomic_fetch_add(&self->jobs_executed, 1);
            } else {
                /* Still no work — park on condition variable */
                pthread_mutex_lock(&pool->wake_mutex);
                if (!atomic_load(&pool->shutdown_requested)) {
                    struct timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_nsec += 1000000;  /* 1ms timeout */
                    if (ts.tv_nsec >= 1000000000) {
                        ts.tv_sec++;
                        ts.tv_nsec -= 1000000000;
                    }
                    pthread_cond_timedwait(&pool->wake_cond, &pool->wake_mutex, &ts);
                }
                pthread_mutex_unlock(&pool->wake_mutex);
            }
        }
    }

    return NULL;
}

/* ──────────────────── Thread Pool Lifecycle ──────────────────── */

static lgx_threading_error_e pool_create(struct lgx_thread_pool_s* pool,
                                          const lgx_threading_config_t* config) {
    memset(pool, 0, sizeof(*pool));

    /* Determine thread count */
    uint32_t count = config->thread_count;
    if (count == 0) {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        count = (n > 0) ? (uint32_t)n : 1;
        /* Leave 1 core for main thread on multi-core systems */
        if (count > 1) count--;
    }
    if (count > LGX_THREADING_MAX_WORKERS) count = LGX_THREADING_MAX_WORKERS;
    pool->worker_count = count;

    /* Detect NUMA topology */
    lgx_threading_detect_numa(&pool->numa);

    /* Initialize wake signaling */
    pthread_mutex_init(&pool->wake_mutex, NULL);
    pthread_cond_init(&pool->wake_cond, NULL);
    atomic_store(&pool->shutdown_requested, false);

    /* Initialize submission queue */
    atomic_store(&pool->submit_head, 0);
    atomic_store(&pool->submit_tail, 0);
    memset(pool->submit_queue, 0, sizeof(pool->submit_queue));

    /* Allocate job pool */
    pool->job_pool_capacity = 65536;
    pool->job_pool = calloc(pool->job_pool_capacity, sizeof(lgx_job_internal_t));
    if (!pool->job_pool) {
        lgx_threading_set_error_detail("Failed to allocate job pool (%u jobs)",
                                        pool->job_pool_capacity);
        return LGX_THREADING_ERROR_OUT_OF_MEMORY;
    }
    atomic_store(&pool->job_pool_head, 0);

    /* Save config */
    pool->config = *config;

    /* Create worker threads */
    for (uint32_t i = 0; i < count; i++) {
        lgx_worker_t* w = &pool->workers[i];
        w->index = i;
        w->running = true;
        w->numa_node = pool->numa.core_to_node[i % pool->numa.total_cores];
        atomic_store(&w->jobs_executed, 0);
        atomic_store(&w->steals_performed, 0);
        atomic_store(&w->idle_ns, 0);

        lgx_work_deque_init(&w->deque, LGX_THREADING_DEQUE_INITIAL_CAP);

        pthread_attr_t attr;
        pthread_attr_init(&attr);

        /* Set CPU affinity if NUMA-aware */
        if (config->numa_aware && pool->numa.node_count > 1) {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i % pool->numa.total_cores, &cpuset);
            pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);
        }

        int rc = pthread_create(&w->thread, &attr, worker_entry, w);
        pthread_attr_destroy(&attr);

        if (rc != 0) {
            lgx_threading_set_error_detail(
                "pthread_create failed for worker %u: %s", i, strerror(rc));
            /* Shut down already-created threads */
            pool->worker_count = i;
            atomic_store(&pool->shutdown_requested, true);
            pthread_cond_broadcast(&pool->wake_cond);
            for (uint32_t j = 0; j < i; j++) {
                pthread_join(pool->workers[j].thread, NULL);
            }
            free(pool->job_pool);
            return LGX_THREADING_ERROR_OPERATION_FAILED;
        }

        /* Set thread name */
        if (config->set_thread_names) {
            char name[16];
            snprintf(name, sizeof(name), "%s_%u",
                     config->name_prefix ? config->name_prefix : "lgx", i);
            pthread_setname_np(w->thread, name);
        }
    }

    return LGX_THREADING_SUCCESS;
}

static void pool_destroy(struct lgx_thread_pool_s* pool) {
    /* Signal shutdown */
    atomic_store_explicit(&pool->shutdown_requested, true, memory_order_release);
    pthread_cond_broadcast(&pool->wake_cond);

    /* Join all workers */
    for (uint32_t i = 0; i < pool->worker_count; i++) {
        pthread_join(pool->workers[i].thread, NULL);
        lgx_work_deque_destroy(&pool->workers[i].deque);
    }

    /* Cleanup */
    pthread_mutex_destroy(&pool->wake_mutex);
    pthread_cond_destroy(&pool->wake_cond);
    free(pool->job_pool);
    pool->job_pool = NULL;
}

/* ──────────────────── Module Lifecycle ──────────────────── */

lgx_threading_error_e lgx_threading_init(const lgx_threading_config_t* config) {
    if (g_state.initialized) {
        lgx_threading_set_error_detail("Threading module already initialized");
        return LGX_THREADING_ERROR_ALREADY_INIT;
    }

    /* Apply defaults */
    lgx_threading_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        lgx_threading_config_t default_cfg = LGX_THREADING_CONFIG_INIT;
        cfg = default_cfg;
    }

    lgx_threading_error_e err = pool_create(&g_state.pool, &cfg);
    if (err != LGX_THREADING_SUCCESS) return err;

    g_state.config = cfg;
    g_state.initialized = true;
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_threading_shutdown(void) {
    if (!g_state.initialized) {
        lgx_threading_set_error_detail("Threading module not initialized");
        return LGX_THREADING_ERROR_NOT_INITIALIZED;
    }

    pool_destroy(&g_state.pool);
    g_state.initialized = false;
    return LGX_THREADING_SUCCESS;
}

/* ──────────────────── Version ──────────────────── */

void lgx_threading_version(uint32_t* major, uint32_t* minor, uint32_t* patch) {
    if (major) *major = LGX_THREADING_VERSION_MAJOR;
    if (minor) *minor = LGX_THREADING_VERSION_MINOR;
    if (patch) *patch = LGX_THREADING_VERSION_PATCH;
}

/* ──────────────────── Error Strings ──────────────────── */

const char* lgx_threading_error_string(lgx_threading_error_e error) {
    switch (error) {
        case LGX_THREADING_SUCCESS:                return "Success";
        case LGX_THREADING_ERROR_INVALID_PARAM:    return "Invalid parameter";
        case LGX_THREADING_ERROR_OUT_OF_MEMORY:    return "Out of memory";
        case LGX_THREADING_ERROR_NOT_INITIALIZED:  return "Not initialized";
        case LGX_THREADING_ERROR_ALREADY_INIT:     return "Already initialized";
        case LGX_THREADING_ERROR_OPERATION_FAILED: return "Operation failed";
        case LGX_THREADING_ERROR_QUEUE_FULL:       return "Queue full";
        case LGX_THREADING_ERROR_QUEUE_EMPTY:      return "Queue empty";
        case LGX_THREADING_ERROR_TIMEOUT:          return "Timeout";
        case LGX_THREADING_ERROR_DEADLOCK:         return "Deadlock detected";
        case LGX_THREADING_ERROR_FIBER_OVERFLOW:   return "Fiber stack overflow";
        case LGX_THREADING_ERROR_POOL_SHUTDOWN:    return "Thread pool shutting down";
        case LGX_THREADING_ERROR_JOB_FAILED:       return "Job execution failed";
        default:                                    return "Unknown error";
    }
}

const char* lgx_threading_get_last_error_detail(void) {
    return g_error_detail;
}

/* ──────────────────── Thread Pool API ──────────────────── */

lgx_thread_pool_t lgx_threading_get_pool(void) {
    if (!g_state.initialized) return NULL;
    return &g_state.pool;
}

lgx_threading_error_e lgx_thread_pool_get_stats(
        lgx_thread_pool_t pool,
        lgx_thread_pool_stats_t* stats) {
    if (!pool || !stats) {
        lgx_threading_set_error_detail("NULL pool or stats pointer");
        return LGX_THREADING_ERROR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(*stats));
    stats->total_threads = pool->worker_count;

    uint64_t total_executed = 0;
    uint64_t total_steals = 0;

    for (uint32_t i = 0; i < pool->worker_count; i++) {
        total_executed += atomic_load(&pool->workers[i].jobs_executed);
        total_steals   += atomic_load(&pool->workers[i].steals_performed);
    }

    stats->total_jobs_executed = total_executed;
    stats->total_steals = total_steals;
    stats->active_threads = pool->worker_count;
    stats->idle_threads = 0;

    /* Compute steal ratio: steals / (steals + local_pops) */
    uint64_t local_pops = (total_executed > total_steals) ? (total_executed - total_steals) : 0;
    uint64_t total_ops = total_steals + local_pops;
    stats->steal_ratio = (total_ops > 0) ? (double)total_steals / (double)total_ops : 0.0;
    stats->avg_utilization = (pool->worker_count > 0)
        ? (double)total_executed / (double)(pool->worker_count * 1000 + 1) : 0.0;
    if (stats->avg_utilization > 1.0) stats->avg_utilization = 1.0;

    return LGX_THREADING_SUCCESS;
}

/* ──────────────────── Job Submission ──────────────────── */

static lgx_job_internal_t* alloc_job(struct lgx_thread_pool_s* pool) {
    uint32_t idx = atomic_fetch_add(&pool->job_pool_head, 1);
    idx = idx % pool->job_pool_capacity;  /* wrap around — ring buffer */
    return &pool->job_pool[idx];
}

lgx_threading_error_e lgx_job_submit(
        const lgx_job_desc_t* desc,
        lgx_job_handle_t* out_handle) {
    if (!g_state.initialized) {
        lgx_threading_set_error_detail("Threading module not initialized");
        return LGX_THREADING_ERROR_NOT_INITIALIZED;
    }
    if (!desc || !desc->func) {
        lgx_threading_set_error_detail("NULL job descriptor or function");
        return LGX_THREADING_ERROR_INVALID_PARAM;
    }

    struct lgx_thread_pool_s* pool = &g_state.pool;

    lgx_job_internal_t* job = alloc_job(pool);
    job->func = desc->func;
    job->data = desc->data;
    job->priority = desc->priority;
    job->submit_time_ns = lgx_threading_now_ns();
    job->start_time_ns = 0;
    job->worker_index = 0;
    atomic_store(&job->remaining_children, 0);
    atomic_store(&job->completion_flag, 0);

    /* Set up parent-child relationship */
    if (desc->parent) {
        job->parent = desc->parent->job;
        atomic_fetch_add(&job->parent->remaining_children, 1);
    } else {
        job->parent = NULL;
    }

    /* Push to global submission queue */
    uint64_t pos = atomic_fetch_add_explicit(&pool->submit_head, 1, memory_order_acq_rel);
    pool->submit_queue[pos % LGX_THREADING_SUBMIT_QUEUE_CAP] = job;

    /* Wake ALL sleeping workers */
    pthread_cond_broadcast(&pool->wake_cond);

    /* Return handle */
    if (out_handle) {
        /* Allocate handle struct — for now, stack-compatible pointer */
        static _Thread_local struct lgx_job_handle_s handle_storage[256];
        static _Thread_local uint32_t handle_idx = 0;
        struct lgx_job_handle_s* h = &handle_storage[handle_idx++ % 256];
        h->job = job;
        *out_handle = h;
    }

    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_job_submit_batch(
        const lgx_job_desc_t* descs,
        size_t count,
        lgx_job_handle_t* out_handles) {
    if (!g_state.initialized) return LGX_THREADING_ERROR_NOT_INITIALIZED;
    if (!descs || count == 0) return LGX_THREADING_ERROR_INVALID_PARAM;

    struct lgx_thread_pool_s* pool = &g_state.pool;

    /* Submit all jobs to global submission queue (batched) */
    for (size_t i = 0; i < count; i++) {
        if (!descs[i].func) return LGX_THREADING_ERROR_INVALID_PARAM;

        lgx_job_internal_t* job = alloc_job(pool);
        job->func = descs[i].func;
        job->data = descs[i].data;
        job->priority = descs[i].priority;
        job->submit_time_ns = lgx_threading_now_ns();
        job->start_time_ns = 0;
        job->worker_index = 0;
        atomic_store(&job->remaining_children, 0);
        atomic_store(&job->completion_flag, 0);

        if (descs[i].parent) {
            job->parent = descs[i].parent->job;
            atomic_fetch_add(&job->parent->remaining_children, 1);
        } else {
            job->parent = NULL;
        }

        /* Push to global submission queue */
        uint64_t pos = atomic_fetch_add_explicit(&pool->submit_head, 1, memory_order_acq_rel);
        pool->submit_queue[pos % LGX_THREADING_SUBMIT_QUEUE_CAP] = job;

        if (out_handles) {
            static _Thread_local struct lgx_job_handle_s batch_handles[65536];
            static _Thread_local uint32_t batch_idx = 0;
            struct lgx_job_handle_s* h = &batch_handles[batch_idx++ % 65536];
            h->job = job;
            out_handles[i] = h;
        }
    }

    /* Wake all workers ONCE after entire batch is enqueued */
    pthread_cond_broadcast(&pool->wake_cond);

    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_job_wait(lgx_job_handle_t handle) {
    if (!handle || !handle->job) {
        return LGX_THREADING_ERROR_INVALID_PARAM;
    }

    struct lgx_thread_pool_s* pool = &g_state.pool;

    /* Active wait: drain submit queue and steal work while waiting */
    while (!atomic_load_explicit(&handle->job->completion_flag, memory_order_acquire)) {
        lgx_job_internal_t* stolen = NULL;

        /* 1. Try to pull from global submission queue */
        uint64_t tail = atomic_load_explicit(&pool->submit_tail, memory_order_relaxed);
        uint64_t head = atomic_load_explicit(&pool->submit_head, memory_order_acquire);
        if (tail < head) {
            if (atomic_compare_exchange_weak_explicit(
                    &pool->submit_tail, &tail, tail + 1,
                    memory_order_acq_rel, memory_order_relaxed)) {
                /* Spin-wait for producer to finish writing */
                while (!(stolen = pool->submit_queue[tail % LGX_THREADING_SUBMIT_QUEUE_CAP])) {
                    /* Brief spin — producer writes immediately after fetch_add */
                }
                pool->submit_queue[tail % LGX_THREADING_SUBMIT_QUEUE_CAP] = NULL;
            }
        }

        /* 2. If nothing in submit queue, try stealing from worker deques */
        if (!stolen) {
            for (uint32_t i = 0; i < pool->worker_count; i++) {
                stolen = lgx_work_deque_steal(&pool->workers[i].deque);
                if (stolen) break;
            }
        }

        if (stolen) {
            /* Execute the stolen job on this (main) thread */
            stolen->start_time_ns = lgx_threading_now_ns();
            stolen->func(stolen->data, UINT32_MAX);
            atomic_store_explicit(&stolen->completion_flag, 1, memory_order_release);

            if (stolen->parent) {
                uint32_t remaining = atomic_fetch_sub(&stolen->parent->remaining_children, 1);
                if (remaining == 1) {
                    atomic_store_explicit(&stolen->parent->completion_flag, 1,
                                          memory_order_release);
                }
            }
        } else {
            /* No work available — brief yield then check again */
            sched_yield();
        }
    }

    return LGX_THREADING_SUCCESS;
}

bool lgx_job_is_complete(lgx_job_handle_t handle) {
    if (!handle || !handle->job) return true;
    return atomic_load_explicit(&handle->job->completion_flag, memory_order_acquire) != 0;
}

/* ──────────────────── Synchronization Primitives ──────────────────── */

#include <linux/futex.h>
#include <sys/syscall.h>

static int futex_wait(uint32_t* addr, uint32_t expected) {
    return (int)syscall(SYS_futex, addr, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
                        expected, NULL, NULL, 0);
}

static int futex_wake(uint32_t* addr, int count) {
    return (int)syscall(SYS_futex, addr, FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
                        count, NULL, NULL, 0);
}

/* ── Mutex ── */

lgx_threading_error_e lgx_mutex_init(lgx_mutex_t* mutex) {
    if (!mutex) return LGX_THREADING_ERROR_INVALID_PARAM;
    atomic_store(&mutex->state, 0);
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_mutex_lock(lgx_mutex_t* mutex) {
    if (!mutex) return LGX_THREADING_ERROR_INVALID_PARAM;

    /* Fast path: CAS 0 → 1 (uncontended) */
    uint32_t expected = 0;
    if (atomic_compare_exchange_strong_explicit(
            &mutex->state, &expected, 1,
            memory_order_acquire, memory_order_relaxed)) {
        return LGX_THREADING_SUCCESS;
    }

    /* Slow path: set state to 2 (contended) and wait */
    do {
        if (expected == 2 ||
            !atomic_compare_exchange_strong_explicit(
                &mutex->state, &expected, 2,
                memory_order_relaxed, memory_order_relaxed)) {
            /* expected was reloaded by CAS failure */
        }
        futex_wait((uint32_t*)&mutex->state, 2);
        expected = 0;
    } while (!atomic_compare_exchange_strong_explicit(
                &mutex->state, &expected, 2,
                memory_order_acquire, memory_order_relaxed));

    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_mutex_try_lock(lgx_mutex_t* mutex) {
    if (!mutex) return LGX_THREADING_ERROR_INVALID_PARAM;

    uint32_t expected = 0;
    if (atomic_compare_exchange_strong_explicit(
            &mutex->state, &expected, 1,
            memory_order_acquire, memory_order_relaxed)) {
        return LGX_THREADING_SUCCESS;
    }
    return LGX_THREADING_ERROR_TIMEOUT;
}

void lgx_mutex_unlock(lgx_mutex_t* mutex) {
    if (!mutex) return;

    uint32_t prev = atomic_fetch_sub_explicit(&mutex->state, 1, memory_order_release);
    if (prev == 2) {
        /* There were waiters — wake one */
        atomic_store_explicit(&mutex->state, 0, memory_order_release);
        futex_wake((uint32_t*)&mutex->state, 1);
    }
}

/* ── Spinlock ── */

void lgx_spinlock_lock(lgx_spinlock_t* lock) {
    if (!lock) return;
    for (;;) {
        /* Test (read-only) before Test-And-Set */
        if (atomic_load_explicit(&lock->locked, memory_order_relaxed) == 0) {
            uint32_t expected = 0;
            if (atomic_compare_exchange_weak_explicit(
                    &lock->locked, &expected, 1,
                    memory_order_acquire, memory_order_relaxed)) {
                return;
            }
        }
        /* Pause hint for x86 */
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__("yield");
#endif
    }
}

bool lgx_spinlock_try_lock(lgx_spinlock_t* lock) {
    if (!lock) return false;
    uint32_t expected = 0;
    return atomic_compare_exchange_strong_explicit(
        &lock->locked, &expected, 1,
        memory_order_acquire, memory_order_relaxed);
}

void lgx_spinlock_unlock(lgx_spinlock_t* lock) {
    if (!lock) return;
    atomic_store_explicit(&lock->locked, 0, memory_order_release);
}

/* ── Read-Write Lock ── */
/*
 * State encoding:
 *   0          = unlocked
 *   > 0        = N readers holding the lock
 *   0x80000000 = writer holding the lock (high bit)
 */
#define RWLOCK_WRITER_BIT 0x80000000u

lgx_threading_error_e lgx_rwlock_init(lgx_rwlock_t* lock) {
    if (!lock) return LGX_THREADING_ERROR_INVALID_PARAM;
    atomic_store(&lock->state, 0);
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_rwlock_read_lock(lgx_rwlock_t* lock) {
    if (!lock) return LGX_THREADING_ERROR_INVALID_PARAM;
    for (;;) {
        uint32_t s = atomic_load_explicit(&lock->state, memory_order_relaxed);
        if (s & RWLOCK_WRITER_BIT) {
            sched_yield();
            continue;
        }
        if (atomic_compare_exchange_weak_explicit(
                &lock->state, &s, s + 1,
                memory_order_acquire, memory_order_relaxed)) {
            return LGX_THREADING_SUCCESS;
        }
    }
}

lgx_threading_error_e lgx_rwlock_write_lock(lgx_rwlock_t* lock) {
    if (!lock) return LGX_THREADING_ERROR_INVALID_PARAM;
    for (;;) {
        uint32_t expected = 0;
        if (atomic_compare_exchange_weak_explicit(
                &lock->state, &expected, RWLOCK_WRITER_BIT,
                memory_order_acquire, memory_order_relaxed)) {
            return LGX_THREADING_SUCCESS;
        }
        sched_yield();
    }
}

void lgx_rwlock_read_unlock(lgx_rwlock_t* lock) {
    if (!lock) return;
    atomic_fetch_sub_explicit(&lock->state, 1, memory_order_release);
}

void lgx_rwlock_write_unlock(lgx_rwlock_t* lock) {
    if (!lock) return;
    atomic_store_explicit(&lock->state, 0, memory_order_release);
}

/* ── Barrier ── */

struct lgx_barrier_s {
    uint32_t          threshold;
    _Atomic uint32_t  count;
    _Atomic uint32_t  generation;
};

lgx_threading_error_e lgx_barrier_create(uint32_t count, lgx_barrier_t* out) {
    if (!out || count == 0) return LGX_THREADING_ERROR_INVALID_PARAM;
    struct lgx_barrier_s* b = malloc(sizeof(struct lgx_barrier_s));
    if (!b) return LGX_THREADING_ERROR_OUT_OF_MEMORY;
    b->threshold = count;
    atomic_store(&b->count, 0);
    atomic_store(&b->generation, 0);
    *out = b;
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_barrier_wait(lgx_barrier_t barrier) {
    if (!barrier) return LGX_THREADING_ERROR_INVALID_PARAM;
    uint32_t gen = atomic_load(&barrier->generation);
    uint32_t arrived = atomic_fetch_add(&barrier->count, 1) + 1;

    if (arrived == barrier->threshold) {
        /* Last thread — reset and advance generation */
        atomic_store(&barrier->count, 0);
        atomic_fetch_add(&barrier->generation, 1);
        return LGX_THREADING_SUCCESS;
    }

    /* Spin until generation advances */
    while (atomic_load_explicit(&barrier->generation, memory_order_acquire) == gen) {
        sched_yield();
    }
    return LGX_THREADING_SUCCESS;
}

void lgx_barrier_destroy(lgx_barrier_t barrier) {
    free(barrier);
}

/* ── Semaphore ── */

lgx_threading_error_e lgx_semaphore_init(lgx_semaphore_t* sem, uint32_t initial) {
    if (!sem) return LGX_THREADING_ERROR_INVALID_PARAM;
    atomic_store(&sem->count, initial);
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_semaphore_wait(lgx_semaphore_t* sem) {
    if (!sem) return LGX_THREADING_ERROR_INVALID_PARAM;
    for (;;) {
        uint32_t c = atomic_load_explicit(&sem->count, memory_order_relaxed);
        if (c > 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &sem->count, &c, c - 1,
                    memory_order_acquire, memory_order_relaxed)) {
                return LGX_THREADING_SUCCESS;
            }
        } else {
            futex_wait((uint32_t*)&sem->count, 0);
        }
    }
}

lgx_threading_error_e lgx_semaphore_try_wait(lgx_semaphore_t* sem) {
    if (!sem) return LGX_THREADING_ERROR_INVALID_PARAM;
    uint32_t c = atomic_load(&sem->count);
    if (c > 0 && atomic_compare_exchange_strong(&sem->count, &c, c - 1)) {
        return LGX_THREADING_SUCCESS;
    }
    return LGX_THREADING_ERROR_TIMEOUT;
}

void lgx_semaphore_post(lgx_semaphore_t* sem) {
    if (!sem) return;
    atomic_fetch_add_explicit(&sem->count, 1, memory_order_release);
    futex_wake((uint32_t*)&sem->count, 1);
}

/* ──────────────────── MPMC Queue ──────────────────── */

typedef struct {
    _Atomic size_t sequence;
    char data[];  /* Flexible array member */
} lgx_mpmc_cell_t;

struct lgx_mpmc_queue_s {
    char*  buffer;           /* Array of cells */
    size_t cell_stride;      /* sizeof(lgx_mpmc_cell_t) + element_size, aligned */
    size_t capacity;
    size_t mask;
    size_t element_size;
    _Atomic size_t enqueue_pos __attribute__((aligned(64)));
    _Atomic size_t dequeue_pos __attribute__((aligned(64)));
};

static inline lgx_mpmc_cell_t* mpmc_cell(struct lgx_mpmc_queue_s* q, size_t idx) {
    return (lgx_mpmc_cell_t*)(q->buffer + (idx & q->mask) * q->cell_stride);
}

lgx_threading_error_e lgx_mpmc_queue_create(
        size_t capacity, size_t element_size, lgx_mpmc_queue_t* out) {
    if (!out || capacity == 0 || element_size == 0)
        return LGX_THREADING_ERROR_INVALID_PARAM;

    /* Must be power of 2 */
    if (capacity & (capacity - 1)) {
        lgx_threading_set_error_detail("MPMC capacity must be power of 2, got %zu", capacity);
        return LGX_THREADING_ERROR_INVALID_PARAM;
    }

    struct lgx_mpmc_queue_s* q = malloc(sizeof(*q));
    if (!q) return LGX_THREADING_ERROR_OUT_OF_MEMORY;

    q->capacity = capacity;
    q->mask = capacity - 1;
    q->element_size = element_size;
    q->cell_stride = sizeof(lgx_mpmc_cell_t) + element_size;
    /* Align cell stride to 8 bytes */
    q->cell_stride = (q->cell_stride + 7) & ~(size_t)7;

    q->buffer = calloc(capacity, q->cell_stride);
    if (!q->buffer) {
        free(q);
        return LGX_THREADING_ERROR_OUT_OF_MEMORY;
    }

    /* Initialize sequence counters */
    for (size_t i = 0; i < capacity; i++) {
        lgx_mpmc_cell_t* cell = (lgx_mpmc_cell_t*)(q->buffer + i * q->cell_stride);
        atomic_store(&cell->sequence, i);
    }

    atomic_store(&q->enqueue_pos, 0);
    atomic_store(&q->dequeue_pos, 0);

    *out = q;
    return LGX_THREADING_SUCCESS;
}

void lgx_mpmc_queue_destroy(lgx_mpmc_queue_t queue) {
    if (!queue) return;
    free(queue->buffer);
    free(queue);
}

lgx_threading_error_e lgx_mpmc_queue_push(lgx_mpmc_queue_t queue, const void* element) {
    if (!queue || !element) return LGX_THREADING_ERROR_INVALID_PARAM;

    size_t pos = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);
    for (;;) {
        lgx_mpmc_cell_t* cell = mpmc_cell(queue, pos);
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &queue->enqueue_pos, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                memcpy(cell->data, element, queue->element_size);
                atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release);
                return LGX_THREADING_SUCCESS;
            }
        } else if (diff < 0) {
            return LGX_THREADING_ERROR_QUEUE_FULL;
        } else {
            pos = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);
        }
    }
}

lgx_threading_error_e lgx_mpmc_queue_pop(lgx_mpmc_queue_t queue, void* out) {
    if (!queue || !out) return LGX_THREADING_ERROR_INVALID_PARAM;

    size_t pos = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);
    for (;;) {
        lgx_mpmc_cell_t* cell = mpmc_cell(queue, pos);
        size_t seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &queue->dequeue_pos, &pos, pos + 1,
                    memory_order_relaxed, memory_order_relaxed)) {
                memcpy(out, cell->data, queue->element_size);
                atomic_store_explicit(&cell->sequence, pos + queue->capacity,
                                      memory_order_release);
                return LGX_THREADING_SUCCESS;
            }
        } else if (diff < 0) {
            return LGX_THREADING_ERROR_QUEUE_EMPTY;
        } else {
            pos = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);
        }
    }
}

size_t lgx_mpmc_queue_size(lgx_mpmc_queue_t queue) {
    if (!queue) return 0;
    size_t eq = atomic_load(&queue->enqueue_pos);
    size_t dq = atomic_load(&queue->dequeue_pos);
    return (eq >= dq) ? (eq - dq) : 0;
}

/* ──────────────────── SPSC Queue ──────────────────── */

struct lgx_spsc_queue_s {
    void*  buffer;
    size_t capacity;
    size_t mask;
    size_t element_size;
    _Atomic size_t head __attribute__((aligned(64)));  /* consumer reads */
    _Atomic size_t tail __attribute__((aligned(64)));  /* producer writes */
};

lgx_threading_error_e lgx_spsc_queue_create(
        size_t capacity, size_t element_size, lgx_spsc_queue_t* out) {
    if (!out || capacity == 0 || element_size == 0)
        return LGX_THREADING_ERROR_INVALID_PARAM;

    /* Round up to power of 2 */
    size_t cap = 1;
    while (cap < capacity) cap <<= 1;

    struct lgx_spsc_queue_s* q = malloc(sizeof(*q));
    if (!q) return LGX_THREADING_ERROR_OUT_OF_MEMORY;

    q->capacity = cap;
    q->mask = cap - 1;
    q->element_size = element_size;
    q->buffer = calloc(cap, element_size);
    if (!q->buffer) {
        free(q);
        return LGX_THREADING_ERROR_OUT_OF_MEMORY;
    }

    atomic_store(&q->head, 0);
    atomic_store(&q->tail, 0);

    *out = q;
    return LGX_THREADING_SUCCESS;
}

void lgx_spsc_queue_destroy(lgx_spsc_queue_t queue) {
    if (!queue) return;
    free(queue->buffer);
    free(queue);
}

lgx_threading_error_e lgx_spsc_queue_push(lgx_spsc_queue_t queue, const void* element) {
    if (!queue || !element) return LGX_THREADING_ERROR_INVALID_PARAM;

    size_t t = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    size_t h = atomic_load_explicit(&queue->head, memory_order_acquire);

    if (t - h >= queue->capacity) {
        return LGX_THREADING_ERROR_QUEUE_FULL;
    }

    memcpy((char*)queue->buffer + (t & queue->mask) * queue->element_size,
           element, queue->element_size);
    atomic_store_explicit(&queue->tail, t + 1, memory_order_release);
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_spsc_queue_pop(lgx_spsc_queue_t queue, void* out) {
    if (!queue || !out) return LGX_THREADING_ERROR_INVALID_PARAM;

    size_t h = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t t = atomic_load_explicit(&queue->tail, memory_order_acquire);

    if (h >= t) {
        return LGX_THREADING_ERROR_QUEUE_EMPTY;
    }

    memcpy(out, (char*)queue->buffer + (h & queue->mask) * queue->element_size,
           queue->element_size);
    atomic_store_explicit(&queue->head, h + 1, memory_order_release);
    return LGX_THREADING_SUCCESS;
}

/* ──────────────────── Lock-Free Stack (Treiber) ──────────────────── */

typedef struct lgx_lfstack_node_s {
    struct lgx_lfstack_node_s* next;
    char data[];  /* flexible array member */
} lgx_lfstack_node_t;

struct lgx_lfstack_s {
    _Atomic(lgx_lfstack_node_t*) top;
    _Atomic(uint64_t) tag;       /* ABA prevention counter */
    _Atomic(size_t) count;
    size_t element_size;
};

lgx_threading_error_e lgx_lfstack_create(size_t element_size, lgx_lfstack_t* out_stack) {
    if (!out_stack || element_size == 0) return LGX_THREADING_ERROR_INVALID_PARAM;

    struct lgx_lfstack_s* s = calloc(1, sizeof(struct lgx_lfstack_s));
    if (!s) return LGX_THREADING_ERROR_OUT_OF_MEMORY;

    atomic_store(&s->top, NULL);
    atomic_store(&s->tag, 0);
    atomic_store(&s->count, 0);
    s->element_size = element_size;

    *out_stack = s;
    return LGX_THREADING_SUCCESS;
}

void lgx_lfstack_destroy(lgx_lfstack_t stack) {
    if (!stack) return;
    /* Free all remaining nodes */
    lgx_lfstack_node_t* node = atomic_load(&stack->top);
    while (node) {
        lgx_lfstack_node_t* next = node->next;
        free(node);
        node = next;
    }
    free(stack);
}

lgx_threading_error_e lgx_lfstack_push(lgx_lfstack_t stack, const void* element) {
    if (!stack || !element) return LGX_THREADING_ERROR_INVALID_PARAM;

    lgx_lfstack_node_t* node = malloc(sizeof(lgx_lfstack_node_t) + stack->element_size);
    if (!node) return LGX_THREADING_ERROR_OUT_OF_MEMORY;
    memcpy(node->data, element, stack->element_size);

    lgx_lfstack_node_t* old_top;
    do {
        old_top = atomic_load_explicit(&stack->top, memory_order_relaxed);
        node->next = old_top;
    } while (!atomic_compare_exchange_weak_explicit(
        &stack->top, &old_top, node,
        memory_order_release, memory_order_relaxed));

    atomic_fetch_add(&stack->tag, 1);
    atomic_fetch_add(&stack->count, 1);
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_lfstack_pop(lgx_lfstack_t stack, void* out_element) {
    if (!stack || !out_element) return LGX_THREADING_ERROR_INVALID_PARAM;

    lgx_lfstack_node_t* old_top;
    lgx_lfstack_node_t* new_top;
    do {
        old_top = atomic_load_explicit(&stack->top, memory_order_acquire);
        if (!old_top) return LGX_THREADING_ERROR_QUEUE_EMPTY;
        new_top = old_top->next;
    } while (!atomic_compare_exchange_weak_explicit(
        &stack->top, &old_top, new_top,
        memory_order_acq_rel, memory_order_relaxed));

    memcpy(out_element, old_top->data, stack->element_size);
    atomic_fetch_add(&stack->tag, 1);
    atomic_fetch_sub(&stack->count, 1);
    free(old_top);
    return LGX_THREADING_SUCCESS;
}

size_t lgx_lfstack_size(lgx_lfstack_t stack) {
    if (!stack) return 0;
    return atomic_load(&stack->count);
}

/* ──────────────────── Concurrent Hash Map ──────────────────── */

typedef struct lgx_map_entry_s {
    uint64_t key;
    void*    value;
    struct lgx_map_entry_s* next;
} lgx_map_entry_t;

typedef struct {
    lgx_mutex_t       lock;
    lgx_map_entry_t*  head;
} lgx_map_bucket_t;

struct lgx_concurrent_map_s {
    lgx_map_bucket_t* buckets;
    size_t            bucket_count;
    size_t            mask;
    _Atomic(size_t)   count;
};

static size_t next_pow2_map(size_t n) {
    if (n <= 1) return 1;
    n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16; n |= n >> 32;
    return n + 1;
}

lgx_threading_error_e lgx_concurrent_map_create(
        size_t bucket_count, lgx_concurrent_map_t* out_map) {
    if (!out_map) return LGX_THREADING_ERROR_INVALID_PARAM;
    if (bucket_count < 4) bucket_count = 4;
    bucket_count = next_pow2_map(bucket_count);

    struct lgx_concurrent_map_s* map = calloc(1, sizeof(struct lgx_concurrent_map_s));
    if (!map) return LGX_THREADING_ERROR_OUT_OF_MEMORY;

    map->buckets = calloc(bucket_count, sizeof(lgx_map_bucket_t));
    if (!map->buckets) { free(map); return LGX_THREADING_ERROR_OUT_OF_MEMORY; }

    map->bucket_count = bucket_count;
    map->mask = bucket_count - 1;
    atomic_store(&map->count, 0);

    for (size_t i = 0; i < bucket_count; i++) {
        lgx_mutex_init(&map->buckets[i].lock);
        map->buckets[i].head = NULL;
    }

    *out_map = map;
    return LGX_THREADING_SUCCESS;
}

void lgx_concurrent_map_destroy(lgx_concurrent_map_t map) {
    if (!map) return;
    for (size_t i = 0; i < map->bucket_count; i++) {
        lgx_map_entry_t* e = map->buckets[i].head;
        while (e) {
            lgx_map_entry_t* next = e->next;
            free(e);
            e = next;
        }
    }
    free(map->buckets);
    free(map);
}

static inline size_t map_hash(uint64_t key) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return (size_t)key;
}

lgx_threading_error_e lgx_concurrent_map_insert(
        lgx_concurrent_map_t map, uint64_t key, void* value) {
    if (!map) return LGX_THREADING_ERROR_INVALID_PARAM;

    size_t idx = map_hash(key) & map->mask;
    lgx_map_bucket_t* bucket = &map->buckets[idx];

    lgx_mutex_lock(&bucket->lock);

    /* Check if key already exists — update value */
    for (lgx_map_entry_t* e = bucket->head; e; e = e->next) {
        if (e->key == key) {
            e->value = value;
            lgx_mutex_unlock(&bucket->lock);
            return LGX_THREADING_SUCCESS;
        }
    }

    /* Insert new entry at head */
    lgx_map_entry_t* entry = malloc(sizeof(lgx_map_entry_t));
    if (!entry) {
        lgx_mutex_unlock(&bucket->lock);
        return LGX_THREADING_ERROR_OUT_OF_MEMORY;
    }
    entry->key = key;
    entry->value = value;
    entry->next = bucket->head;
    bucket->head = entry;
    atomic_fetch_add(&map->count, 1);

    lgx_mutex_unlock(&bucket->lock);
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_concurrent_map_lookup(
        lgx_concurrent_map_t map, uint64_t key, void** out_value) {
    if (!map || !out_value) return LGX_THREADING_ERROR_INVALID_PARAM;

    size_t idx = map_hash(key) & map->mask;
    lgx_map_bucket_t* bucket = &map->buckets[idx];

    lgx_mutex_lock(&bucket->lock);

    for (lgx_map_entry_t* e = bucket->head; e; e = e->next) {
        if (e->key == key) {
            *out_value = e->value;
            lgx_mutex_unlock(&bucket->lock);
            return LGX_THREADING_SUCCESS;
        }
    }

    lgx_mutex_unlock(&bucket->lock);
    return LGX_THREADING_ERROR_QUEUE_EMPTY;  /* "not found" */
}

lgx_threading_error_e lgx_concurrent_map_remove(
        lgx_concurrent_map_t map, uint64_t key) {
    if (!map) return LGX_THREADING_ERROR_INVALID_PARAM;

    size_t idx = map_hash(key) & map->mask;
    lgx_map_bucket_t* bucket = &map->buckets[idx];

    lgx_mutex_lock(&bucket->lock);

    lgx_map_entry_t** pp = &bucket->head;
    while (*pp) {
        if ((*pp)->key == key) {
            lgx_map_entry_t* victim = *pp;
            *pp = victim->next;
            free(victim);
            atomic_fetch_sub(&map->count, 1);
            lgx_mutex_unlock(&bucket->lock);
            return LGX_THREADING_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    lgx_mutex_unlock(&bucket->lock);
    return LGX_THREADING_ERROR_QUEUE_EMPTY;  /* "not found" */
}

size_t lgx_concurrent_map_size(lgx_concurrent_map_t map) {
    if (!map) return 0;
    return atomic_load(&map->count);
}

/* ──────────────────── Fiber Pool ──────────────────── */

/* Forward declare — defined in Fiber System section below */
void fiber_trampoline(void);

typedef struct lgx_fiber_pool_node_s {
    struct lgx_fiber_pool_node_s* next;
    struct lgx_fiber_s*           fiber;
} lgx_fiber_pool_node_t;

static struct {
    lgx_fiber_pool_node_t* top;
    lgx_mutex_t            lock;
    size_t                 total;
    size_t                 available;
    int                    initialized;
} g_fiber_pool = {0};

lgx_threading_error_e lgx_fiber_pool_init(size_t count, size_t stack_size) {
    if (g_fiber_pool.initialized) return LGX_THREADING_ERROR_ALREADY_INIT;
    if (count == 0) count = 128;
    if (stack_size == 0) stack_size = LGX_FIBER_DEFAULT_STACK_SIZE;

    lgx_mutex_init(&g_fiber_pool.lock);
    g_fiber_pool.top = NULL;
    g_fiber_pool.total = count;
    g_fiber_pool.available = 0;

    /* We pre-allocate nodes but fibers are created lazily on acquire */
    g_fiber_pool.initialized = 1;
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_fiber_pool_acquire(
        lgx_fiber_func_t func, void* data, lgx_fiber_t* out_fiber) {
    if (!func || !out_fiber) return LGX_THREADING_ERROR_INVALID_PARAM;

    lgx_mutex_lock(&g_fiber_pool.lock);

    /* Try to reuse a pooled fiber */
    if (g_fiber_pool.top) {
        lgx_fiber_pool_node_t* node = g_fiber_pool.top;
        g_fiber_pool.top = node->next;
        g_fiber_pool.available--;

        struct lgx_fiber_s* fiber = node->fiber;
        free(node);
        lgx_mutex_unlock(&g_fiber_pool.lock);

        /* Re-initialize the fiber with new function */
        fiber->func = func;
        fiber->data = data;
        fiber->state = LGX_FIBER_STATE_IDLE;
        fiber->waiting_on = NULL;

        /* Reset ucontext */
        getcontext(&fiber->context);
        size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
        fiber->context.uc_stack.ss_sp = (char*)fiber->stack_base + page_size;
        fiber->context.uc_stack.ss_size = fiber->stack_size;
        fiber->context.uc_link = NULL;

        makecontext(&fiber->context, fiber_trampoline, 0);

        *out_fiber = fiber;
        return LGX_THREADING_SUCCESS;
    }

    lgx_mutex_unlock(&g_fiber_pool.lock);

    /* No pooled fiber available — create a new one */
    return lgx_fiber_create(func, data, 0, out_fiber);
}

lgx_threading_error_e lgx_fiber_pool_release(lgx_fiber_t fiber) {
    if (!fiber) return LGX_THREADING_ERROR_INVALID_PARAM;

    lgx_fiber_pool_node_t* node = malloc(sizeof(lgx_fiber_pool_node_t));
    if (!node) {
        /* Can't pool it — just destroy */
        lgx_fiber_destroy(fiber);
        return LGX_THREADING_SUCCESS;
    }

    node->fiber = fiber;
    fiber->state = LGX_FIBER_STATE_IDLE;

    lgx_mutex_lock(&g_fiber_pool.lock);
    node->next = g_fiber_pool.top;
    g_fiber_pool.top = node;
    g_fiber_pool.available++;
    lgx_mutex_unlock(&g_fiber_pool.lock);

    return LGX_THREADING_SUCCESS;
}

void lgx_fiber_pool_shutdown(void) {
    if (!g_fiber_pool.initialized) return;

    lgx_fiber_pool_node_t* node = g_fiber_pool.top;
    while (node) {
        lgx_fiber_pool_node_t* next = node->next;
        lgx_fiber_destroy(node->fiber);
        free(node);
        node = next;
    }
    g_fiber_pool.top = NULL;
    g_fiber_pool.available = 0;
    g_fiber_pool.initialized = 0;
}



/* Thread-local: currently running fiber on this thread */
_Thread_local struct lgx_fiber_s* lgx_current_fiber = NULL;

/* Internal trampoline — entered via makecontext, runs user func, returns to scheduler */
void fiber_trampoline(void) {
    struct lgx_fiber_s* fiber = lgx_current_fiber;
    if (!fiber) return;

    /* Execute user function */
    fiber->func(fiber->data);

    /* Mark completed and return to scheduler */
    fiber->state = LGX_FIBER_STATE_COMPLETED;
    lgx_current_fiber = NULL;

    /* swapcontext back to the scheduler that launched us */
    if (fiber->scheduler_ctx) {
        setcontext(fiber->scheduler_ctx);
    }
    /* If no scheduler context, we just fall off — shouldn't happen */
}

lgx_threading_error_e lgx_fiber_create(
        lgx_fiber_func_t func,
        void* data,
        size_t stack_size,
        lgx_fiber_t* out_fiber) {
    if (!func || !out_fiber) {
        return LGX_THREADING_ERROR_INVALID_PARAM;
    }

    if (stack_size == 0) stack_size = LGX_FIBER_DEFAULT_STACK_SIZE;

    /* Allocate fiber struct */
    struct lgx_fiber_s* fiber = calloc(1, sizeof(struct lgx_fiber_s));
    if (!fiber) return LGX_THREADING_ERROR_OUT_OF_MEMORY;

    /* Allocate stack with guard page:
     * Layout: [guard page (4KB)] [usable stack (stack_size)]
     * The guard page is PROT_NONE → stack overflow triggers SIGSEGV */
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t total = page_size + stack_size;

    void* region = mmap(NULL, total, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (region == MAP_FAILED) {
        free(fiber);
        return LGX_THREADING_ERROR_OUT_OF_MEMORY;
    }

    /* Protect the first page (guard) */
    mprotect(region, page_size, PROT_NONE);

    fiber->stack_base = region;
    fiber->stack_size = stack_size;
    fiber->total_alloc = total;
    fiber->func = func;
    fiber->data = data;
    fiber->state = LGX_FIBER_STATE_IDLE;
    fiber->waiting_on = NULL;
    fiber->scheduler_ctx = NULL;

    /* Set up ucontext */
    getcontext(&fiber->context);
    fiber->context.uc_stack.ss_sp = (char*)region + page_size; /* past guard page */
    fiber->context.uc_stack.ss_size = stack_size;
    fiber->context.uc_link = NULL;
    makecontext(&fiber->context, fiber_trampoline, 0);

    *out_fiber = fiber;
    return LGX_THREADING_SUCCESS;
}

void lgx_fiber_destroy(lgx_fiber_t fiber) {
    if (!fiber) return;

    /* Unmap stack (guard page + usable) */
    if (fiber->stack_base) {
        munmap(fiber->stack_base, fiber->total_alloc);
    }
    free(fiber);
}

lgx_threading_error_e lgx_fiber_yield(void) {
    struct lgx_fiber_s* fiber = lgx_current_fiber;
    if (!fiber) {
        lgx_threading_set_error_detail("lgx_fiber_yield called outside fiber context");
        return LGX_THREADING_ERROR_OPERATION_FAILED;
    }

    /* Save state and return to scheduler */
    fiber->state = LGX_FIBER_STATE_SUSPENDED;
    lgx_current_fiber = NULL;
    swapcontext(&fiber->context, fiber->scheduler_ctx);

    /* When resumed, we return here */
    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_fiber_resume(lgx_fiber_t fiber) {
    if (!fiber) return LGX_THREADING_ERROR_INVALID_PARAM;
    if (fiber->state != LGX_FIBER_STATE_SUSPENDED &&
        fiber->state != LGX_FIBER_STATE_IDLE) {
        return LGX_THREADING_ERROR_OPERATION_FAILED;
    }

    /* Set up scheduler return context */
    ucontext_t scheduler;
    fiber->scheduler_ctx = &scheduler;
    fiber->state = LGX_FIBER_STATE_RUNNING;
    lgx_current_fiber = fiber;

    /* Switch to fiber — returns here when fiber yields or completes */
    swapcontext(&scheduler, &fiber->context);

    return LGX_THREADING_SUCCESS;
}

lgx_threading_error_e lgx_fiber_wait_job(lgx_job_handle_t handle) {
    struct lgx_fiber_s* fiber = lgx_current_fiber;
    if (!fiber) {
        lgx_threading_set_error_detail("lgx_fiber_wait_job called outside fiber context");
        return LGX_THREADING_ERROR_OPERATION_FAILED;
    }
    if (!handle) return LGX_THREADING_ERROR_INVALID_PARAM;

    /* If the job is already done, no need to suspend */
    if (lgx_job_is_complete(handle)) {
        return LGX_THREADING_SUCCESS;
    }

    /* Mark what we're waiting on, then yield */
    fiber->waiting_on = handle;
    fiber->state = LGX_FIBER_STATE_SUSPENDED;
    lgx_current_fiber = NULL;
    swapcontext(&fiber->context, fiber->scheduler_ctx);

    /* When resumed, the job should be complete */
    fiber->waiting_on = NULL;
    return LGX_THREADING_SUCCESS;
}

lgx_fiber_state_e lgx_fiber_get_state(lgx_fiber_t fiber) {
    if (!fiber) return LGX_FIBER_STATE_IDLE;
    return fiber->state;
}

/* ──────────────────── Diagnostics ──────────────────── */

void lgx_threading_dump_state(void) {
    if (!g_state.initialized) {
        fprintf(stderr, "[LGX Threading] Not initialized\n");
        return;
    }

    struct lgx_thread_pool_s* pool = &g_state.pool;
    fprintf(stderr, "[LGX Threading] State dump:\n");
    fprintf(stderr, "  Workers: %u\n", pool->worker_count);
    fprintf(stderr, "  NUMA nodes: %u\n", pool->numa.node_count);
    fprintf(stderr, "  Shutdown: %s\n",
            atomic_load(&pool->shutdown_requested) ? "yes" : "no");

    /* Submit queue depth */
    uint64_t sq_head = atomic_load(&pool->submit_head);
    uint64_t sq_tail = atomic_load(&pool->submit_tail);
    fprintf(stderr, "  Submit queue: %lu pending (head=%lu tail=%lu)\n",
            (unsigned long)(sq_head - sq_tail),
            (unsigned long)sq_head, (unsigned long)sq_tail);

    for (uint32_t i = 0; i < pool->worker_count; i++) {
        lgx_worker_t* w = &pool->workers[i];
        fprintf(stderr, "  Worker %u: jobs=%lu steals=%lu node=%u\n",
                i,
                (unsigned long)atomic_load(&w->jobs_executed),
                (unsigned long)atomic_load(&w->steals_performed),
                w->numa_node);
    }

    /* Fiber info */
    if (lgx_current_fiber) {
        fprintf(stderr, "  Current fiber: state=%d\n", lgx_current_fiber->state);
    } else {
        fprintf(stderr, "  Current fiber: none\n");
    }
}
