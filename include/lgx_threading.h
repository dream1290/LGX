/**
 * @file lgx_threading.h
 * @brief LGX Threading Module - Public API
 * 
 * Provides a work-stealing job system, thread pool, lock-free data structures,
 * synchronization primitives, and optional fiber support for Linux games.
 * 
 * This is part of the LGX Platform v1.1 release.
 * 
 * Thread safety:
 *   - All lgx_job_* functions: thread-safe
 *   - All lgx_mpmc_queue_* functions: thread-safe (lock-free)
 *   - All lgx_spsc_queue_* functions: thread-compatible (1 producer, 1 consumer)
 *   - All lgx_mutex_* functions: thread-safe
 *   - lgx_threading_init/shutdown: NOT thread-safe (call from main thread only)
 */

#ifndef LGX_THREADING_H
#define LGX_THREADING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────── Version ──────────────────── */

#define LGX_THREADING_VERSION_MAJOR 1
#define LGX_THREADING_VERSION_MINOR 1
#define LGX_THREADING_VERSION_PATCH 0
#define LGX_THREADING_VERSION_STRING "1.1.0"

/* ──────────────────── Error Codes ──────────────────── */

typedef enum {
    LGX_THREADING_SUCCESS                = 0,
    LGX_THREADING_ERROR_INVALID_PARAM    = -1,
    LGX_THREADING_ERROR_OUT_OF_MEMORY    = -2,
    LGX_THREADING_ERROR_NOT_INITIALIZED  = -3,
    LGX_THREADING_ERROR_ALREADY_INIT     = -4,
    LGX_THREADING_ERROR_OPERATION_FAILED = -5,

    /* Module-specific errors (start at -100) */
    LGX_THREADING_ERROR_QUEUE_FULL       = -100,
    LGX_THREADING_ERROR_QUEUE_EMPTY      = -101,
    LGX_THREADING_ERROR_TIMEOUT          = -102,
    LGX_THREADING_ERROR_DEADLOCK         = -103,
    LGX_THREADING_ERROR_FIBER_OVERFLOW   = -104,
    LGX_THREADING_ERROR_POOL_SHUTDOWN    = -105,
    LGX_THREADING_ERROR_JOB_FAILED       = -106,
} lgx_threading_error_e;

/* ──────────────────── Opaque Handles ──────────────────── */

typedef struct lgx_thread_pool_s*  lgx_thread_pool_t;
typedef struct lgx_job_system_s*   lgx_job_system_t;
typedef struct lgx_job_handle_s*   lgx_job_handle_t;
typedef struct lgx_mpmc_queue_s*   lgx_mpmc_queue_t;
typedef struct lgx_spsc_queue_s*   lgx_spsc_queue_t;
typedef struct lgx_barrier_s*      lgx_barrier_t;
typedef struct lgx_fiber_s*        lgx_fiber_t;

/* ──────────────────── Enumerations ──────────────────── */

typedef enum {
    LGX_JOB_PRIORITY_LOW      = 0,
    LGX_JOB_PRIORITY_NORMAL   = 1,
    LGX_JOB_PRIORITY_HIGH     = 2,
    LGX_JOB_PRIORITY_CRITICAL = 3,
} lgx_job_priority_e;

/* ──────────────────── Configuration ──────────────────── */

/**
 * Threading module configuration.
 * Uses struct_size pattern for forward compatibility.
 */
typedef struct {
    uint32_t struct_size;       /**< sizeof(lgx_threading_config_t) */
    uint32_t thread_count;     /**< 0 = auto (one per physical core) */
    bool     numa_aware;       /**< Pin threads to NUMA nodes */
    bool     set_thread_names; /**< Name threads for debugger */
    size_t   per_thread_arena; /**< Per-thread frame arena bytes (0 = default 2MB) */
    const char* name_prefix;   /**< Thread name prefix (e.g. "lgx_worker") */
} lgx_threading_config_t;

/**
 * Helper macro: zero-initialize config with struct_size already set.
 */
#define LGX_THREADING_CONFIG_INIT { \
    .struct_size = sizeof(lgx_threading_config_t), \
    .thread_count = 0, \
    .numa_aware = false, \
    .set_thread_names = true, \
    .per_thread_arena = 0, \
    .name_prefix = "lgx_worker" \
}

/* ──────────────────── Job Descriptors ──────────────────── */

/**
 * Job function signature.
 * @param data         User-provided data pointer
 * @param thread_index Index of the worker thread executing this job (0 .. N-1)
 */
typedef void (*lgx_job_func_t)(void* data, uint32_t thread_index);

/**
 * Job description for submission.
 */
typedef struct {
    uint32_t           struct_size; /**< sizeof(lgx_job_desc_t) */
    lgx_job_func_t     func;       /**< Function to execute */
    void*              data;       /**< User data passed to func */
    lgx_job_priority_e priority;   /**< Scheduling priority */
    lgx_job_handle_t   parent;     /**< Parent job (NULL = no dependency) */
} lgx_job_desc_t;

#define LGX_JOB_DESC_INIT { \
    .struct_size = sizeof(lgx_job_desc_t), \
    .func = NULL, \
    .data = NULL, \
    .priority = LGX_JOB_PRIORITY_NORMAL, \
    .parent = NULL \
}

/* ──────────────────── Statistics ──────────────────── */

typedef struct {
    uint32_t total_threads;
    uint32_t active_threads;
    uint32_t idle_threads;
    uint64_t total_jobs_executed;
    uint64_t total_steals;
    double   avg_utilization;  /**< 0.0 - 1.0 */
    double   steal_ratio;     /**< steals / (steals + local_pops), 0.0 - 1.0 */
} lgx_thread_pool_stats_t;

/* ──────────────────── Module Lifecycle ──────────────────── */

/**
 * Initialize the threading module.
 * Must be called before any other lgx_threading_* function.
 * @param config  Configuration (NULL = defaults)
 * @return LGX_THREADING_SUCCESS on success
 */
lgx_threading_error_e lgx_threading_init(const lgx_threading_config_t* config);

/**
 * Shut down the threading module.
 * Waits for all pending jobs, then joins worker threads.
 * @return LGX_THREADING_SUCCESS on success
 */
lgx_threading_error_e lgx_threading_shutdown(void);

/**
 * Get module version.
 */
void lgx_threading_version(uint32_t* major, uint32_t* minor, uint32_t* patch);

/* ──────────────────── Error Handling ──────────────────── */

/**
 * Get human-readable error string.
 */
const char* lgx_threading_error_string(lgx_threading_error_e error);

/**
 * Get thread-local detail string for the last error.
 */
const char* lgx_threading_get_last_error_detail(void);

/* ──────────────────── Thread Pool ──────────────────── */

/**
 * Get the global thread pool created during init.
 * Returns NULL if not initialized.
 */
lgx_thread_pool_t lgx_threading_get_pool(void);

/**
 * Get thread pool statistics.
 */
lgx_threading_error_e lgx_thread_pool_get_stats(
    lgx_thread_pool_t pool,
    lgx_thread_pool_stats_t* stats
);

/* ──────────────────── Job System ──────────────────── */

/**
 * Submit a single job to the scheduler.
 * @param desc       Job descriptor
 * @param out_handle Receives job handle (may be NULL if you don't need to wait)
 */
lgx_threading_error_e lgx_job_submit(
    const lgx_job_desc_t* desc,
    lgx_job_handle_t* out_handle
);

/**
 * Submit a batch of jobs atomically.
 * @param descs       Array of job descriptors
 * @param count       Number of jobs
 * @param out_handles Receives job handles (may be NULL)
 */
lgx_threading_error_e lgx_job_submit_batch(
    const lgx_job_desc_t* descs,
    size_t count,
    lgx_job_handle_t* out_handles
);

/**
 * Wait for a job to complete.
 * Does useful work (executes other jobs) while waiting.
 * @param handle Job handle from lgx_job_submit
 */
lgx_threading_error_e lgx_job_wait(lgx_job_handle_t handle);

/**
 * Check if a job is complete (non-blocking).
 */
bool lgx_job_is_complete(lgx_job_handle_t handle);

/* ──────────────────── MPMC Queue ──────────────────── */

/**
 * Create a bounded multi-producer multi-consumer queue.
 * @param capacity     Max elements (must be power of 2)
 * @param element_size Size of each element in bytes
 * @param out_queue    Receives queue handle
 */
lgx_threading_error_e lgx_mpmc_queue_create(
    size_t capacity,
    size_t element_size,
    lgx_mpmc_queue_t* out_queue
);

void lgx_mpmc_queue_destroy(lgx_mpmc_queue_t queue);

lgx_threading_error_e lgx_mpmc_queue_push(
    lgx_mpmc_queue_t queue,
    const void* element
);

lgx_threading_error_e lgx_mpmc_queue_pop(
    lgx_mpmc_queue_t queue,
    void* out_element
);

size_t lgx_mpmc_queue_size(lgx_mpmc_queue_t queue);

/* ──────────────────── SPSC Queue ──────────────────── */

lgx_threading_error_e lgx_spsc_queue_create(
    size_t capacity,
    size_t element_size,
    lgx_spsc_queue_t* out_queue
);

void lgx_spsc_queue_destroy(lgx_spsc_queue_t queue);

lgx_threading_error_e lgx_spsc_queue_push(
    lgx_spsc_queue_t queue,
    const void* element
);

lgx_threading_error_e lgx_spsc_queue_pop(
    lgx_spsc_queue_t queue,
    void* out_element
);

/* ──────────────────── Lock-Free Stack (Treiber) ──────────────────── */

typedef struct lgx_lfstack_s* lgx_lfstack_t;

/**
 * Create a lock-free stack (Treiber stack with tagged pointers for ABA prevention).
 * @param element_size Size of each element in bytes
 * @param out_stack    Receives stack handle
 */
lgx_threading_error_e lgx_lfstack_create(
    size_t element_size,
    lgx_lfstack_t* out_stack
);

void lgx_lfstack_destroy(lgx_lfstack_t stack);

lgx_threading_error_e lgx_lfstack_push(
    lgx_lfstack_t stack,
    const void* element
);

lgx_threading_error_e lgx_lfstack_pop(
    lgx_lfstack_t stack,
    void* out_element
);

size_t lgx_lfstack_size(lgx_lfstack_t stack);

/* ──────────────────── Concurrent Hash Map ──────────────────── */

typedef struct lgx_concurrent_map_s* lgx_concurrent_map_t;

/**
 * Create a concurrent hash map with striped locks.
 * @param bucket_count Number of buckets (will be rounded up to power of 2)
 * @param out_map      Receives map handle
 */
lgx_threading_error_e lgx_concurrent_map_create(
    size_t bucket_count,
    lgx_concurrent_map_t* out_map
);

void lgx_concurrent_map_destroy(lgx_concurrent_map_t map);

lgx_threading_error_e lgx_concurrent_map_insert(
    lgx_concurrent_map_t map,
    uint64_t key,
    void* value
);

lgx_threading_error_e lgx_concurrent_map_lookup(
    lgx_concurrent_map_t map,
    uint64_t key,
    void** out_value
);

lgx_threading_error_e lgx_concurrent_map_remove(
    lgx_concurrent_map_t map,
    uint64_t key
);

size_t lgx_concurrent_map_size(lgx_concurrent_map_t map);

/* ──────────────────── Synchronization Primitives ──────────────────── */

/**
 * Lightweight futex-based mutex (4 bytes).
 */
typedef struct { _Atomic uint32_t state; } lgx_mutex_t;

#define LGX_MUTEX_INIT { .state = 0 }

lgx_threading_error_e lgx_mutex_init(lgx_mutex_t* mutex);
lgx_threading_error_e lgx_mutex_lock(lgx_mutex_t* mutex);
lgx_threading_error_e lgx_mutex_try_lock(lgx_mutex_t* mutex);
void lgx_mutex_unlock(lgx_mutex_t* mutex);

/**
 * Read-write lock.
 */
typedef struct { _Atomic uint32_t state; } lgx_rwlock_t;

#define LGX_RWLOCK_INIT { .state = 0 }

lgx_threading_error_e lgx_rwlock_init(lgx_rwlock_t* lock);
lgx_threading_error_e lgx_rwlock_read_lock(lgx_rwlock_t* lock);
lgx_threading_error_e lgx_rwlock_write_lock(lgx_rwlock_t* lock);
void lgx_rwlock_read_unlock(lgx_rwlock_t* lock);
void lgx_rwlock_write_unlock(lgx_rwlock_t* lock);

/**
 * Spinlock (TTAS with pause hints).
 */
typedef struct { _Atomic uint32_t locked; } lgx_spinlock_t;

#define LGX_SPINLOCK_INIT { .locked = 0 }

void lgx_spinlock_lock(lgx_spinlock_t* lock);
bool lgx_spinlock_try_lock(lgx_spinlock_t* lock);
void lgx_spinlock_unlock(lgx_spinlock_t* lock);

/**
 * Barrier.
 */
lgx_threading_error_e lgx_barrier_create(uint32_t count, lgx_barrier_t* out_barrier);
lgx_threading_error_e lgx_barrier_wait(lgx_barrier_t barrier);
void lgx_barrier_destroy(lgx_barrier_t barrier);

/**
 * Counting semaphore.
 */
typedef struct { _Atomic uint32_t count; } lgx_semaphore_t;

lgx_threading_error_e lgx_semaphore_init(lgx_semaphore_t* sem, uint32_t initial);
lgx_threading_error_e lgx_semaphore_wait(lgx_semaphore_t* sem);
lgx_threading_error_e lgx_semaphore_try_wait(lgx_semaphore_t* sem);
void lgx_semaphore_post(lgx_semaphore_t* sem);

/* ──────────────────── Fiber System ──────────────────── */

/**
 * Fiber function signature.
 * @param data User-provided data pointer
 */
typedef void (*lgx_fiber_func_t)(void* data);

/**
 * Fiber state enum.
 */
typedef enum {
    LGX_FIBER_STATE_IDLE       = 0,   /**< In pool, not running */
    LGX_FIBER_STATE_RUNNING    = 1,   /**< Currently executing */
    LGX_FIBER_STATE_SUSPENDED  = 2,   /**< Yielded or waiting */
    LGX_FIBER_STATE_COMPLETED  = 3,   /**< Function returned */
} lgx_fiber_state_e;

/**
 * Create a fiber with a function and stack size.
 * @param func       Function to execute
 * @param data       User data passed to func
 * @param stack_size Stack size in bytes (0 = default 64KB)
 * @param out_fiber  Receives fiber handle
 */
lgx_threading_error_e lgx_fiber_create(
    lgx_fiber_func_t func,
    void* data,
    size_t stack_size,
    lgx_fiber_t* out_fiber
);

/**
 * Destroy a fiber and release its stack.
 */
void lgx_fiber_destroy(lgx_fiber_t fiber);

/**
 * Yield the current fiber (cooperative switch to scheduler).
 * Must be called from within a fiber context.
 */
lgx_threading_error_e lgx_fiber_yield(void);

/**
 * Suspend the current fiber until a job completes.
 * @param handle Job handle to wait on
 */
lgx_threading_error_e lgx_fiber_wait_job(lgx_job_handle_t handle);

/**
 * Resume a suspended fiber.
 * @param fiber Fiber to resume
 */
lgx_threading_error_e lgx_fiber_resume(lgx_fiber_t fiber);

/**
 * Get the current state of a fiber.
 */
lgx_fiber_state_e lgx_fiber_get_state(lgx_fiber_t fiber);

/* ──────────────────── Fiber Pool ──────────────────── */

/**
 * Initialize the global fiber pool with pre-allocated fibers.
 * @param count      Number of fibers to pre-allocate (0 = default 128)
 * @param stack_size Stack size per fiber (0 = default)
 */
lgx_threading_error_e lgx_fiber_pool_init(size_t count, size_t stack_size);

/**
 * Acquire a fiber from the pool (or create new if pool exhausted).
 */
lgx_threading_error_e lgx_fiber_pool_acquire(
    lgx_fiber_func_t func, void* data, lgx_fiber_t* out_fiber);

/**
 * Release a fiber back to the pool for reuse.
 */
lgx_threading_error_e lgx_fiber_pool_release(lgx_fiber_t fiber);

/**
 * Shut down the fiber pool, destroying all cached fibers.
 */
void lgx_fiber_pool_shutdown(void);

/* ──────────────────── Diagnostics ──────────────────── */

/**
 * Dump internal threading state to stderr for crash diagnostics.
 */
void lgx_threading_dump_state(void);

#ifdef __cplusplus
}
#endif

#endif /* LGX_THREADING_H */
