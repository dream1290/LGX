# Requirements Document: LGX Threading Module (v1.1)

## Introduction

The LGX Threading Module (`lgx_threading`) provides a high-performance, game-oriented threading and job system for Linux. It is the second module in the LGX Platform stack, building on the Memory Module (v1.0) to deliver work-stealing parallelism, lock-free data structures, and NUMA-aware thread management.

Threading is the foundation that all subsequent LGX modules depend on: Graphics needs command buffer generation, Audio needs background mixing, Networking needs async I/O, and Input needs event dispatch. The module targets Q2 2026 release as v1.1.

## Glossary

- **Job**: A unit of work (function pointer + data) submitted to the scheduler
- **Job System**: The work-stealing scheduler that distributes jobs across worker threads
- **Work Stealing**: A scheduling strategy where idle threads steal work from busy threads' queues
- **MPMC Queue**: Multi-Producer Multi-Consumer lock-free queue
- **SPSC Queue**: Single-Producer Single-Consumer lock-free queue
- **Fiber**: Lightweight cooperative thread (user-space context switch)
- **Thread Pool**: A managed set of worker threads
- **NUMA**: Non-Uniform Memory Access — memory architecture where access latency depends on locality
- **Barrier**: Synchronization point where N threads wait until all arrive
- **DAG**: Directed Acyclic Graph — used for expressing job dependencies

## Requirements

### Requirement 1: Job System

**User Story:** As a game developer, I want to submit units of work to a parallel scheduler, so that my game logic runs across all available CPU cores.

#### Acceptance Criteria

1. THE Module SHALL provide a job system with work-stealing scheduling across all available CPU cores
2. THE Module SHALL support job priorities (LOW, NORMAL, HIGH, CRITICAL)
3. THE Module SHALL support job dependencies via parent-child relationships (DAG scheduling)
4. THE Module SHALL support job batching — submitting multiple jobs atomically
5. THE Module SHALL provide `lgx_job_submit()` that returns immediately with a handle
6. THE Module SHALL provide `lgx_job_wait()` that blocks until a job (and its children) complete
7. THE Module SHALL achieve job submission latency < 100 ns (P99, uncontended)
8. THE Module SHALL achieve job dispatch overhead < 500 ns per job

### Requirement 2: Thread Pool

**User Story:** As a game developer, I want a managed pool of worker threads, so that I don't manage OS threads manually.

#### Acceptance Criteria

1. THE Module SHALL create one worker thread per physical CPU core by default
2. THE Module SHALL support configurable thread count via `lgx_threading_config_t`
3. THE Module SHALL support NUMA-aware thread placement (pin threads to their NUMA node)
4. THE Module SHALL support thread naming for debugger visibility (`pthread_setname_np`)
5. THE Module SHALL support thread affinity configuration (core pinning)
6. THE Module SHALL gracefully degrade on single-core systems (run all jobs on main thread)
7. THE Module SHALL report thread pool statistics (active threads, idle threads, job queue depth)

### Requirement 3: Lock-Free Data Structures

**User Story:** As a game developer, I want high-performance concurrent data structures, so that my game systems can communicate without lock contention.

#### Acceptance Criteria

1. THE Module SHALL provide a lock-free MPMC bounded queue for general producer-consumer patterns
2. THE Module SHALL provide a lock-free SPSC queue for single-producer single-consumer patterns (e.g., render thread communication)
3. THE Module SHALL provide a concurrent hash map for thread-safe key-value storage
4. THE Module SHALL provide a lock-free stack for LIFO patterns
5. ALL lock-free structures SHALL be wait-free for the fast path (bounded worst-case)
6. ALL lock-free structures SHALL use C11 atomics for portability
7. MPMC queue throughput SHALL exceed 10 million ops/sec on 4-core systems

### Requirement 4: Synchronization Primitives

**User Story:** As a game developer, I want lightweight synchronization primitives, so that I can coordinate between game systems with minimal overhead.

#### Acceptance Criteria

1. THE Module SHALL provide `lgx_mutex_t` — a lightweight mutex using Linux futex
2. THE Module SHALL provide `lgx_condvar_t` — a condition variable
3. THE Module SHALL provide `lgx_barrier_t` — a reusable barrier for N threads
4. THE Module SHALL provide `lgx_semaphore_t` — a counting semaphore
5. THE Module SHALL provide `lgx_rwlock_t` — a read-write lock (multiple readers, exclusive writer)
6. THE Module SHALL provide `lgx_spinlock_t` — a spinlock for ultra-short critical sections
7. ALL primitives SHALL support `try_lock` / `try_wait` non-blocking variants
8. Mutex lock/unlock latency SHALL be < 50 ns uncontended

### Requirement 5: Memory Integration

**User Story:** As a game developer, I want the threading module to integrate with LGX memory, so that per-thread allocations are fast and cache-friendly.

#### Acceptance Criteria

1. THE Module SHALL use `lgx_memory` frame arena for per-job temporary allocations
2. THE Module SHALL provide thread-local frame arenas (one per worker thread)
3. THE Module SHALL allow jobs to specify their memory allocation intent
4. THE Module SHALL automatically reset thread-local arenas at job completion
5. THE Module SHALL support custom allocators for job system internal structures
6. THE Module SHALL use NUMA-local memory for thread-local data when NUMA is available

### Requirement 6: Fiber / Coroutine Support

**User Story:** As a game developer, I want lightweight fibers, so that I can write async game logic without callback spaghetti.

#### Acceptance Criteria

1. THE Module SHALL provide fiber creation with configurable stack size (default: 64KB)
2. THE Module SHALL provide `lgx_fiber_yield()` for cooperative context switching
3. THE Module SHALL provide `lgx_fiber_wait()` to suspend a fiber until a job completes
4. THE Module SHALL support fiber pooling (pre-allocated fiber pool to avoid creation overhead)
5. Fiber context switch latency SHALL be < 100 ns
6. THE Module SHALL detect fiber stack overflow in debug builds

### Requirement 7: Platform Patterns Compliance

**User Story:** As a platform contributor, I want the threading module to follow LGX platform conventions, so that all modules feel consistent.

#### Acceptance Criteria

1. THE Module SHALL expose all APIs via C linkage (extern "C") with opaque handles
2. THE Module SHALL use `struct_size`-based configuration for forward compatibility
3. THE Module SHALL use ELF symbol versioning (`LGX_THREADING_1.1`)
4. THE Module SHALL use the standard error code taxonomy (LGX_SUCCESS = 0, negatives for errors)
5. THE Module SHALL provide `lgx_threading_version()` returning semantic version
6. THE Module SHALL provide thread-local error detail via `lgx_threading_get_last_error_detail()`
7. THE Module SHALL follow `lgx_threading_*` naming convention for all public symbols
8. THE Module SHALL support initialization without other modules (standalone mode with fallback allocator)

### Requirement 8: Observability and Debugging

**User Story:** As a game developer, I want to profile and debug threading behavior, so that I can identify parallelism bottlenecks.

#### Acceptance Criteria

1. THE Module SHALL provide job timing statistics (submission time, execution time, wait time)
2. THE Module SHALL provide thread utilization metrics (busy %, idle %, steal count)
3. THE Module SHALL provide work-stealing visualization data (per-core queue depths over time)
4. THE Module SHALL integrate with LGX tracing system (`lgx_trace_begin/end`)
5. THE Module SHALL support Tracy profiler integration for fiber-aware profiling
6. THE Module SHALL detect and report potential deadlocks (lock ordering violations) in debug builds
7. THE Module SHALL provide `lgx_threading_dump_state()` for crash diagnostics

### Requirement 9: Performance Targets

**User Story:** As a game developer, I want threading overhead to be negligible, so that parallelism provides net positive performance.

#### Acceptance Criteria

1. Job submission: < 100 ns P99 (uncontended)
2. Job dispatch: < 500 ns per job
3. Fiber switch: < 100 ns
4. Mutex lock/unlock: < 50 ns uncontended
5. MPMC enqueue/dequeue: < 200 ns P99
6. Thread pool startup: < 5 ms
7. Thread pool shutdown: < 10 ms
8. Memory overhead per worker thread: < 256 KB
9. Total module memory overhead: < 4 MB (8-core system)
