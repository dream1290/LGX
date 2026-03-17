/**
 * @file lgx_threading_internal.h
 * @brief LGX Threading Module - Internal Types and Declarations
 *
 * Private header — NOT part of the public API.
 * Do not include this header in user code.
 */

#ifndef LGX_THREADING_INTERNAL_H
#define LGX_THREADING_INTERNAL_H

#include "lgx_threading.h"
#include <pthread.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ──────────────────── Constants ──────────────────── */

#define LGX_THREADING_MAX_WORKERS        128
#define LGX_THREADING_DEFAULT_ARENA_SIZE (2 * 1024 * 1024)  /* 2 MB */
#define LGX_THREADING_DEQUE_INITIAL_CAP  1024
#define LGX_THREADING_MAX_NUMA_NODES     8
#define LGX_THREADING_MAX_CORES          256
#define LGX_THREADING_ERROR_DETAIL_SIZE  256

/* ──────────────────── NUMA Topology ──────────────────── */

typedef struct {
    uint32_t node_count;
    uint32_t total_cores;
    uint32_t cores_per_node[LGX_THREADING_MAX_NUMA_NODES];
    uint32_t core_to_node[LGX_THREADING_MAX_CORES];
    size_t   node_memory_bytes[LGX_THREADING_MAX_NUMA_NODES];
} lgx_numa_topology_t;

/* ──────────────────── Job (Internal) ──────────────────── */

typedef struct lgx_job_internal_s {
    lgx_job_func_t     func;
    void*              data;
    lgx_job_priority_e priority;
    _Atomic uint32_t   remaining_children;   /* counts down to 0 */
    _Atomic uint32_t   completion_flag;      /* 0 = pending, 1 = done */
    struct lgx_job_internal_s* parent;
    uint64_t           submit_time_ns;
    uint64_t           start_time_ns;
    uint32_t           worker_index;
} lgx_job_internal_t;

/* ──────────────────── Work-Stealing Deque ──────────────────── */

typedef struct {
    lgx_job_internal_t** buffer;
    _Atomic int64_t      top;     /* steal end (FIFO) */
    _Atomic int64_t      bottom;  /* push/pop end (LIFO) */
    size_t               capacity;
    size_t               mask;    /* capacity - 1 (power of 2) */
} lgx_work_deque_t;

/* ──────────────────── Worker Thread ──────────────────── */

typedef struct {
    pthread_t   thread;
    uint32_t    index;
    uint32_t    numa_node;
    bool        running;

    /* Per-worker work-stealing deque */
    lgx_work_deque_t deque;

    /* Statistics */
    _Atomic uint64_t jobs_executed;
    _Atomic uint64_t steals_performed;
    _Atomic uint64_t idle_ns;
} lgx_worker_t;

/* ──────────────────── Thread Pool (Internal) ──────────────────── */

#define LGX_THREADING_SUBMIT_QUEUE_CAP  65536  /* must be power of 2 */

struct lgx_thread_pool_s {
    lgx_worker_t        workers[LGX_THREADING_MAX_WORKERS];
    uint32_t            worker_count;
    _Atomic bool        shutdown_requested;
    lgx_numa_topology_t numa;

    /* Wake signaling */
    pthread_mutex_t     wake_mutex;
    pthread_cond_t      wake_cond;

    /* Job allocation pool (simple bump allocator for job structs) */
    lgx_job_internal_t* job_pool;
    _Atomic uint32_t    job_pool_head;
    uint32_t            job_pool_capacity;

    /* Global submission queue (MPMC ring buffer of job pointers) */
    /* Main thread pushes here; workers pull into their own deques */
    lgx_job_internal_t* submit_queue[LGX_THREADING_SUBMIT_QUEUE_CAP];
    _Atomic uint64_t    submit_head;  /* next write position */
    _Atomic uint64_t    submit_tail;  /* next read position */

    /* Configuration snapshot */
    lgx_threading_config_t config;
};

/* ──────────────────── Job System (Internal) ──────────────────── */

struct lgx_job_system_s {
    lgx_thread_pool_t pool;
};

/* ──────────────────── Job Handle (Internal) ──────────────────── */

struct lgx_job_handle_s {
    lgx_job_internal_t* job;
};

/* ──────────────────── Module State ──────────────────── */

typedef struct {
    bool                   initialized;
    struct lgx_thread_pool_s  pool;
    lgx_threading_config_t config;
} lgx_threading_state_t;

/* ──────────────────── Internal Functions ──────────────────── */

/* Error detail (thread-local) */
void lgx_threading_set_error_detail(const char* fmt, ...);

/* NUMA */
void lgx_threading_detect_numa(lgx_numa_topology_t* topo);

/* Work deque */
void lgx_work_deque_init(lgx_work_deque_t* dq, size_t capacity);
void lgx_work_deque_destroy(lgx_work_deque_t* dq);
void lgx_work_deque_push(lgx_work_deque_t* dq, lgx_job_internal_t* job);
lgx_job_internal_t* lgx_work_deque_pop(lgx_work_deque_t* dq);
lgx_job_internal_t* lgx_work_deque_steal(lgx_work_deque_t* dq);

/* Timing */
uint64_t lgx_threading_now_ns(void);

/* ──────────────────── Fiber Internals ──────────────────── */

#include <ucontext.h>

#define LGX_FIBER_DEFAULT_STACK_SIZE  (64 * 1024)   /* 64 KB */
#define LGX_FIBER_MAX_FIBERS          256

struct lgx_fiber_s {
    ucontext_t          context;        /**< Saved context (registers, stack) */
    lgx_fiber_func_t    func;           /**< User function */
    void*               data;           /**< User data */
    lgx_fiber_state_e   state;          /**< Current state */

    /* Stack with guard pages */
    void*               stack_base;     /**< mmap'd region (guard + usable) */
    size_t              stack_size;     /**< Usable stack bytes */
    size_t              total_alloc;    /**< guard + stack bytes */

    /* Waiting-on-job support */
    lgx_job_handle_t    waiting_on;     /**< Job handle this fiber waits on (or NULL) */

    /* Back-pointer to the scheduler context that launched this fiber */
    ucontext_t*         scheduler_ctx;  /**< Context to return to on yield/complete */
};

/* Thread-local: currently running fiber on this worker */
extern _Thread_local struct lgx_fiber_s* lgx_current_fiber;

#ifdef __cplusplus
}
#endif

#endif /* LGX_THREADING_INTERNAL_H */
