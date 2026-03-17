# LGX Threading Module — API Reference

## Module Lifecycle

| Function | Description |
|----------|-------------|
| `lgx_threading_init(config)` | Initialize the threading module with optional config (thread count, NUMA) |
| `lgx_threading_shutdown()` | Shut down all workers, drain queues, join threads |
| `lgx_threading_version()` | Returns `{1, 1, 0}` version struct |
| `lgx_threading_error_string(err)` | Human-readable error string for error code |
| `lgx_threading_get_last_error_detail()` | Thread-local detailed error message |

## Thread Pool

| Function | Description |
|----------|-------------|
| `lgx_threading_get_pool()` | Get the global thread pool handle |
| `lgx_thread_pool_get_stats(pool, stats)` | Fill `lgx_thread_pool_stats_t` with utilization, steal ratio |
| `lgx_threading_dump_state()` | Dump all worker states, queue depths, fiber info to stderr |

## Job System

| Function | Description |
|----------|-------------|
| `lgx_job_submit(desc, handle)` | Submit a single job. Returns handle for waiting |
| `lgx_job_submit_batch(descs, n, handles)` | Submit N jobs atomically |
| `lgx_job_wait(handle)` | Block until job completes (does useful work while waiting) |
| `lgx_job_is_complete(handle)` | Non-blocking completion check |

### Job Descriptor
```c
lgx_job_desc_t desc = LGX_JOB_DESC_INIT;
desc.func = my_job_function;  // void (*)(void* data, uint32_t thread_id)
desc.data = my_data;
desc.parent = parent_handle;  // optional dependency
```

## Synchronization Primitives

| Type | Functions |
|------|-----------|
| `lgx_mutex_t` | `init`, `lock`, `try_lock`, `unlock` — 3-state futex mutex |
| `lgx_spinlock_t` | `lock`, `try_lock`, `unlock` — TTAS with pause hints |
| `lgx_rwlock_t` | `init`, `read_lock`, `write_lock`, `read_unlock`, `write_unlock` |
| `lgx_barrier_t` | `create`, `wait`, `destroy` — reusable with generation counter |
| `lgx_semaphore_t` | `init`, `wait`, `try_wait`, `post` — counting semaphore |

## Lock-Free Data Structures

### MPMC Queue (Vyukov)
```c
lgx_mpmc_queue_t q;
lgx_mpmc_queue_create(1024, sizeof(int), &q);  // power-of-2 capacity
lgx_mpmc_queue_push(q, &item);
lgx_mpmc_queue_pop(q, &out);
lgx_mpmc_queue_destroy(q);
```

### SPSC Queue
```c
lgx_spsc_queue_t q;
lgx_spsc_queue_create(1024, sizeof(int), &q);
lgx_spsc_queue_push(q, &item);
lgx_spsc_queue_pop(q, &out);
lgx_spsc_queue_destroy(q);
```

### Lock-Free Stack (Treiber)
```c
lgx_lfstack_t s;
lgx_lfstack_create(sizeof(int), &s);
lgx_lfstack_push(s, &item);   // CAS-based, ABA-safe
lgx_lfstack_pop(s, &out);     // LIFO order
lgx_lfstack_size(s);
lgx_lfstack_destroy(s);
```

### Concurrent Hash Map
```c
lgx_concurrent_map_t m;
lgx_concurrent_map_create(64, &m);  // 64 buckets, striped locks
lgx_concurrent_map_insert(m, key, value);
lgx_concurrent_map_lookup(m, key, &value);
lgx_concurrent_map_remove(m, key);
lgx_concurrent_map_size(m);
lgx_concurrent_map_destroy(m);
```

## Fiber System

### Basic Usage
```c
void my_fiber_func(void* data) {
    // Do work...
    lgx_fiber_yield();    // Cooperatively yield
    // Resumed here later
    lgx_fiber_wait_job(job_handle);  // Suspend until job done
}

lgx_fiber_t fiber;
lgx_fiber_create(my_fiber_func, data, 0, &fiber);  // 0 = default 64KB stack
lgx_fiber_resume(fiber);   // Run until yield/complete
lgx_fiber_resume(fiber);   // Continue from yield point
lgx_fiber_destroy(fiber);
```

### Fiber Pool
```c
lgx_fiber_pool_init(128, 0);  // Pre-allocate 128 fiber slots
lgx_fiber_t f;
lgx_fiber_pool_acquire(func, data, &f);  // Get from pool or create new
lgx_fiber_resume(f);
lgx_fiber_pool_release(f);  // Return to pool for reuse
lgx_fiber_pool_shutdown();
```

### States
`LGX_FIBER_STATE_IDLE` → `RUNNING` → `SUSPENDED` ↔ `RUNNING` → `COMPLETED`

## Getting Started

```c
#include "lgx_threading.h"

void my_job(void* data, uint32_t tid) {
    printf("Hello from worker %u!\n", tid);
}

int main(void) {
    // 1. Initialize with 4 threads
    lgx_threading_config_t cfg = LGX_THREADING_CONFIG_INIT;
    cfg.thread_count = 4;
    lgx_threading_init(&cfg);

    // 2. Submit work
    lgx_job_desc_t d = LGX_JOB_DESC_INIT;
    d.func = my_job;
    lgx_job_handle_t h;
    lgx_job_submit(&d, &h);
    lgx_job_wait(h);

    // 3. Clean up
    lgx_threading_shutdown();
}
```

## Examples

### Parallel For (Fan-Out)
```c
void process_chunk(void* data, uint32_t tid) {
    int chunk_id = *(int*)data;
    // Process chunk_id...
}

// Fan out 50 parallel chunks
lgx_job_handle_t handles[50];
int chunks[50];
for (int i = 0; i < 50; i++) {
    chunks[i] = i;
    lgx_job_desc_t d = LGX_JOB_DESC_INIT;
    d.func = process_chunk;
    d.data = &chunks[i];
    lgx_job_submit(&d, &handles[i]);
}
for (int i = 0; i < 50; i++) lgx_job_wait(handles[i]);
```

### Producer-Consumer with MPMC Queue
```c
lgx_mpmc_queue_t q;
lgx_mpmc_queue_create(256, sizeof(int), &q);

// Producer thread
for (int i = 0; i < 1000; i++)
    lgx_mpmc_queue_push(q, &i);

// Consumer thread
int val;
while (lgx_mpmc_queue_pop(q, &val) == LGX_THREADING_SUCCESS)
    process(val);
```

### Fork-Join with Fibers
```c
void compute_fiber(void* data) {
    // Submit child job
    lgx_job_desc_t d = LGX_JOB_DESC_INIT;
    d.func = heavy_work;
    lgx_job_handle_t child;
    lgx_job_submit(&d, &child);

    // Suspend fiber until child completes
    lgx_fiber_wait_job(child);

    // Continue with result
    use_result(child);
}
```
