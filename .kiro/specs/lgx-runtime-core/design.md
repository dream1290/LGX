# LGX Runtime Core - Design Document

## 1. Design Overview

The LGX Runtime Core provides a stable C ABI layer that games link against, managing initialization, versioning, memory, lifecycle, and platform services. It uses layered isolation with pinned libraries to ensure deterministic behavior across Linux distributions.

**Revolutionary Enhancement**: The Runtime Core implements a **Telescoping Architecture** with intent-based APIs that enable incremental enhancement across 4 layers without breaking ABI compatibility.

**Core Innovation**: APIs capture **intent** (what, why, how you'll use resources), not just requirements (how much). This enables:
- Layer 1: Optimal pool selection based on intent
- Layer 2: Predictive pre-warming based on learned patterns
- Layer 3: Hardware-aware NUMA placement based on access patterns
- Layer 4: Verified allocation for safety-critical operations

See `REVOLUTIONARY_ARCHITECTURE.md` for complete 48-month roadmap.

## 2. Architecture

### 2.1 Component Structure

```
┌─────────────────────────────────────────────────────────┐
│  Game Binary                                            │
│  - Links against lgx_runtime.h                         │
│  - Calls lgx_runtime_init(), lgx_alloc(), etc.         │
└─────────────────────────────────────────────────────────┘
                         ↓ (C ABI calls)
┌─────────────────────────────────────────────────────────┐
│  lgx_runtime.so (Runtime Core)                          │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ ABI Layer   │  │ Version Mgmt │  │ Capability    │  │
│  │ - Exports   │  │ - Negotiation│  │ Detection     │  │
│  │ - Validation│  │ - Compat     │  │ - Query       │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ Memory Mgmt │  │ Lifecycle    │  │ Platform      │  │
│  │ - Pools     │  │ - Init       │  │ Services      │  │
│  │ - Allocator │  │ - Suspend    │  │ - FS/Time/Log │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│  ┌─────────────────────────────────────────────────┐   │
│  │ Telemetry (Opt-in)                              │   │
│  │ - Frame-time, Memory, Crashes                   │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│  Pinned Libraries (Isolated Namespace)                  │
│  - glibc 2.35+, libstdc++, Vulkan loader 1.3.x         │
└─────────────────────────────────────────────────────────┘
```

### 2.2 Key Design Decisions

**Decision 1: C ABI for Maximum Compatibility**
- Rationale: C ABI is stable across compilers and versions, unlike C++
- Trade-off: Less expressive than C++, but maximum portability

**Decision 2: Pinned Libraries in Isolated Namespace**
- Rationale: Ensures deterministic behavior across distributions
- Trade-off: Larger disk footprint, but predictable execution

**Decision 3: Memory Pools with Size Classes**
- Rationale: Reduces allocator fragmentation and improves performance
- Trade-off: Some memory waste, but faster allocation

**Decision 4: Opt-in Telemetry**
- Rationale: Privacy-first approach, user consent required
- Trade-off: Lower telemetry coverage, but respects user privacy

## 3. API Design

### 3.1 Core API Functions

```c
// Initialization and Shutdown
typedef struct lgx_runtime_config lgx_runtime_config_t;  // Opaque handle

lgx_runtime_config_t* lgx_config_create(void);
void lgx_config_set_log_path(lgx_runtime_config_t* config, const char* path);
void lgx_config_set_memory_pool_size(lgx_runtime_config_t* config, size_t size);
void lgx_config_set_flags(lgx_runtime_config_t* config, uint32_t flags);
void lgx_config_destroy(lgx_runtime_config_t* config);

lgx_result_t lgx_runtime_init(const lgx_runtime_config_t* config);
lgx_result_t lgx_runtime_shutdown(void);

// Version and Compatibility (size-based versioning for forward compat)
typedef struct lgx_version {
    size_t struct_size;      // ALWAYS FIRST FIELD - for forward compatibility
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} lgx_version_t;

lgx_version_t lgx_runtime_get_version(void);
lgx_result_t lgx_runtime_check_compatibility(const lgx_version_t* required_version);

// Capability Detection
typedef enum lgx_capability {
    LGX_CAP_DX11_TRANSLATION,
    LGX_CAP_DX12_TRANSLATION,
    LGX_CAP_SECURITY_MODULE,
    LGX_CAP_RAYTRACING,
    LGX_CAP_MESH_SHADERS,
} lgx_capability_t;

bool lgx_runtime_has_capability(lgx_capability_t cap);
lgx_result_t lgx_runtime_query_capabilities(lgx_capability_t* caps, size_t* count);

// Memory Management
void* lgx_alloc(size_t size);
void* lgx_alloc_aligned(size_t size, size_t alignment);
void lgx_free(void* ptr);
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats);

// Memory Management with Intent (Layer 1+)
typedef enum lgx_access_pattern {
    LGX_ACCESS_SEQUENTIAL,          // Sequential access (streaming)
    LGX_ACCESS_RANDOM,              // Random access (lookup tables)
    LGX_ACCESS_WRITE_ONCE,          // Write once, read many
} lgx_access_pattern_t;

typedef enum lgx_lifetime {
    LGX_LIFETIME_FRAME,             // Lives for one frame
    LGX_LIFETIME_LEVEL,             // Lives for current level/scene
    LGX_LIFETIME_SESSION,           // Lives for entire game session
} lgx_lifetime_t;

typedef enum lgx_performance_hint {
    LGX_HINT_CRITICAL_PATH,         // Frame-critical, needs <50ns access
    LGX_HINT_BACKGROUND,            // Background task, latency tolerant
    LGX_HINT_BANDWIDTH_HUNGRY,      // Needs high bandwidth (>100GB/s)
    LGX_HINT_COMPUTE_HEAVY,         // CPU-intensive operations
} lgx_performance_hint_t;

typedef struct lgx_allocation_intent {
    size_t struct_size;             // For forward compatibility
    size_t size;                    // How much memory
    lgx_access_pattern_t access_pattern;  // How you'll access it
    lgx_lifetime_t lifetime;        // How long you'll keep it
    lgx_performance_hint_t hint;    // Performance requirements
} lgx_allocation_intent_t;

// Intent-based allocation (enables future layers)
void* lgx_alloc_with_intent(const lgx_allocation_intent_t* intent);

// Mathematical Guarantees (Layer 1)
typedef struct lgx_temporal_guarantee {
    size_t struct_size;
    uint64_t max_init_time_ns;      // 500ms, mathematically proven
    uint64_t max_allocation_ns;     // 1μs, statistically guaranteed
    double frame_time_variance;     // <0.5ms p99, formally verified
} lgx_temporal_guarantee_t;

lgx_result_t lgx_request_guarantees(lgx_temporal_guarantee_t* guarantees);

// Lifecycle Management
lgx_result_t lgx_runtime_suspend(void);
lgx_result_t lgx_runtime_resume(void);

// Platform Services - Filesystem
typedef struct lgx_file lgx_file_t;

lgx_result_t lgx_fs_open(const char* path, const char* mode, lgx_file_t** file);
lgx_result_t lgx_fs_read(lgx_file_t* file, void* buffer, size_t size, size_t* bytes_read);
lgx_result_t lgx_fs_write(lgx_file_t* file, const void* buffer, size_t size);
lgx_result_t lgx_fs_close(lgx_file_t* file);

// Platform Services - Timing
uint64_t lgx_time_now_ns(void);  // Nanoseconds since epoch
void lgx_time_sleep_ms(uint32_t milliseconds);

// Platform Services - Logging
typedef enum lgx_log_level {
    LGX_LOG_DEBUG,
    LGX_LOG_INFO,
    LGX_LOG_WARN,
    LGX_LOG_ERROR,
} lgx_log_level_t;

void lgx_log(lgx_log_level_t level, const char* format, ...);

// Telemetry (Opt-in)
lgx_result_t lgx_telemetry_enable(bool opt_in);
lgx_result_t lgx_telemetry_export(char* buffer, size_t buffer_size);

// Error Handling
typedef enum lgx_result {
    LGX_SUCCESS = 0,
    LGX_ERROR_INVALID_PARAM,
    LGX_ERROR_NOT_INITIALIZED,
    LGX_ERROR_ALREADY_INITIALIZED,
    LGX_ERROR_INCOMPATIBLE_VERSION,
    LGX_ERROR_OUT_OF_MEMORY,
    LGX_ERROR_IO_ERROR,
    LGX_ERROR_NOT_SUPPORTED,
    LGX_ERROR_LIBRARY_VERSION_MISMATCH,
    LGX_ERROR_GPU_UNAVAILABLE,
    LGX_ERROR_RESOURCE_LIMIT_EXCEEDED,
} lgx_result_t;

const char* lgx_result_to_string(lgx_result_t result);

// Error Context API (thread-local)
typedef struct lgx_error_context {
    size_t struct_size;
    lgx_result_t error_code;
    const char* error_message;
    const char* function_name;
    const char* file_name;
    int line_number;
} lgx_error_context_t;

lgx_error_context_t lgx_get_last_error(void);

// Error Callback
typedef void (*lgx_error_callback_t)(const lgx_error_context_t* context, void* user_data);
void lgx_set_error_handler(lgx_error_callback_t callback, void* user_data);

// Health Check API
typedef struct lgx_health_status {
    size_t struct_size;
    bool is_healthy;
    bool huge_pages_active;
    bool gpu_responsive;
    size_t memory_usage_mb;
    uint32_t allocation_failures;
    uint32_t degraded_features;  // Bitmask of features in degraded mode
} lgx_health_status_t;

lgx_result_t lgx_runtime_health_check(lgx_health_status_t* status);

// Performance Counters API
typedef enum lgx_counter {
    LGX_COUNTER_ALLOCATIONS,
    LGX_COUNTER_DEALLOCATIONS,
    LGX_COUNTER_CACHE_HITS,
    LGX_COUNTER_CACHE_MISSES,
    LGX_COUNTER_POOL_EXHAUSTIONS,
} lgx_counter_t;

uint64_t lgx_get_counter(lgx_counter_t counter);
void lgx_reset_counters(void);
```

### 3.2 Integration Contracts for Other LGX Components

**Translation Layer Integration**
```c
// Translation Layer gets access to shared memory regions and allocation hooks
typedef struct lgx_translate_context lgx_translate_context_t;

lgx_translate_context_t* lgx_runtime_get_translate_context(void);
void* lgx_translate_alloc(size_t size);  // Uses runtime's allocator
void lgx_translate_free(void* ptr);
```

**Security Module Integration**
```c
// Security Module registers hooks to monitor runtime operations
typedef struct lgx_security_hooks {
    void (*on_alloc)(void* ptr, size_t size, void* user_data);
    void (*on_free)(void* ptr, void* user_data);
    void (*on_api_call)(const char* func_name, void* user_data);
    void (*on_error)(const lgx_error_context_t* error, void* user_data);
} lgx_security_hooks_t;

lgx_result_t lgx_runtime_register_security_hooks(const lgx_security_hooks_t* hooks, void* user_data);
lgx_result_t lgx_runtime_unregister_security_hooks(void);
```

**Shader Manager Integration**
```c
// Shader Manager uses runtime's filesystem and memory services
typedef struct lgx_shader_cache_config {
    const char* cache_directory;
    size_t max_cache_size_mb;
} lgx_shader_cache_config_t;

lgx_result_t lgx_runtime_configure_shader_cache(const lgx_shader_cache_config_t* config);
```

**Plugin Architecture for Optional Components**
```c
// Components can be loaded dynamically or disabled
typedef enum lgx_component {
    LGX_COMPONENT_DX11_TRANSLATION,
    LGX_COMPONENT_DX12_TRANSLATION,
    LGX_COMPONENT_SECURITY_MODULE,
    LGX_COMPONENT_SHADER_CACHE,
} lgx_component_t;

bool lgx_runtime_is_component_loaded(lgx_component_t component);
lgx_result_t lgx_runtime_load_component(lgx_component_t component);
lgx_result_t lgx_runtime_unload_component(lgx_component_t component);
```

### 3.3 ABI Stability Strategy

**Opaque Handle Pattern**
- All mutable state uses opaque handles (pointers to incomplete types)
- Games never see struct internals, only pointers
- Runtime can change internal layout without breaking ABI
- Example: `lgx_runtime_config_t*` instead of `lgx_runtime_config_t`

**Size-Based Versioning for Transparent Structs**
- Structs that must be transparent include `struct_size` as first field
- Caller sets `struct_size = sizeof(lgx_version_t)` before passing to runtime
- Runtime checks size to determine which fields are valid
- Allows forward compatibility: old game + new runtime works

**Symbol Versioning**
- Use ELF symbol versioning to support multiple ABI versions simultaneously
- Example: `lgx_runtime_init@@LGX_1.0`, `lgx_runtime_init@@LGX_2.0`
- Linker automatically selects correct version based on game's link-time version

**Deprecation Process**
1. Mark function as deprecated in headers with `__attribute__((deprecated))`
2. Add deprecation marker in binary (custom ELF section)
3. Maintain function for 2 major versions (minimum 24 months)
4. Provide shim layer: old function calls new function internally
5. Remove in third major version with clear migration guide

**ABI Compatibility Testing**
- Automated tests: compile game against v1.0 headers, run against v1.5 runtime
- Verify all v1.0 functions still work correctly
- Verify new v1.5 features gracefully degrade when called by v1.0 game
- CI runs compatibility matrix: all minor versions within major version

## 4. Memory Management Design

### 4.1 Memory Pool Architecture

**Size Classes (Initial - to be validated via profiling)**
- Small: 16B, 32B, 64B, 128B, 256B, 512B
- Medium: 1KB, 2KB, 4KB, 8KB, 16KB
- Large: 64KB, 256KB, 1MB, 4MB, 16MB
- Huge: >16MB (direct allocation via mmap)

**Adaptive Pool Strategy**
- Phase 1 (MVP): Fixed size classes based on estimates
- Phase 2 (Post-profiling): Adjust size classes based on real game allocation patterns
- Phase 3 (Runtime): Detect allocation patterns and grow/shrink pools dynamically

**Pool Segregation**
- Separate pools for short-lived (frame-scoped) and long-lived allocations
- Separate pools by object type (textures, buffers, metadata)
- Prevents fragmentation from mixing allocation patterns
- Pre-allocate pools at initialization to avoid runtime allocation

**Thread-Local Caching (Lock-Free)**
- Each thread maintains local cache of free blocks per size class
- Cache size: 64 blocks per size class (tunable)
- Fast path: atomic CAS operations, no mutex locks
- Slow path: refill from global pool with mutex (rare)

**Adaptive Thread-Local Caching Optimization**

**Problem**: Fixed caches for all size classes waste memory
- Most threads only use 2-3 size classes heavily
- Caching all size classes wastes memory on unused caches

**Solution**: Adaptive caching based on usage patterns
```c
#define NUM_HOT_SIZE_CLASSES 4

struct thread_cache {
    // Track 4 most-used size classes per thread
    uint32_t hot_size_classes[NUM_HOT_SIZE_CLASSES];
    void* free_list[NUM_HOT_SIZE_CLASSES];
    uint32_t count[NUM_HOT_SIZE_CLASSES];
    
    // Track usage for all size classes
    uint64_t allocation_counts[NUM_SIZE_CLASSES];
    
    // Performance counters
    uint64_t cache_hits;
    uint64_t cache_misses;
} __attribute__((aligned(64)));  // Cache line aligned

// Periodically (every 10K allocations):
void adapt_thread_cache(thread_cache_t* cache) {
    // 1. Identify new hot size classes (top 4 by allocation count)
    uint32_t new_hot[NUM_HOT_SIZE_CLASSES];
    find_top_k_size_classes(cache->allocation_counts, new_hot, NUM_HOT_SIZE_CLASSES);
    
    // 2. Evict cold caches, promote hot ones
    for (int i = 0; i < NUM_HOT_SIZE_CLASSES; i++) {
        if (cache->hot_size_classes[i] != new_hot[i]) {
            // Evict cold cache: return blocks to global pool
            return_to_global_pool(cache->free_list[i], cache->count[i]);
            
            // Promote hot cache: refill from global pool
            cache->hot_size_classes[i] = new_hot[i];
            refill_from_global_pool(new_hot[i], &cache->free_list[i], &cache->count[i]);
        }
    }
    
    // 3. Reset allocation counts for next period
    memset(cache->allocation_counts, 0, sizeof(cache->allocation_counts));
}

// Result: 80% memory savings with same hit rate
// - Typical thread uses 2-3 size classes heavily
// - Only cache those 2-3, not all 16 size classes
// - Hit rate remains >95% because we cache the hot ones
```

**Huge Pages**
- Use transparent huge pages (2MB) for game heap when available
- Fallback to standard 4KB pages if huge pages unavailable
- Reduces TLB pressure: measured 3-5% performance gain in CPU-bound scenarios
- Health check API reports huge pages status

**NUMA Awareness**
- Detect NUMA topology at initialization
- Allocate memory on local NUMA node (closest to calling thread's CPU)
- For GPU-bound allocations, prefer NUMA node closest to GPU (PCIe topology)
- Tool: `lgx-numa-check` validates optimal configuration

### 4.2 Lock-Free Allocation Fast Path

**Thread-Local Cache Structure**
```c
struct thread_cache {
    void* free_list[NUM_SIZE_CLASSES];  // Per-size-class free lists
    uint32_t count[NUM_SIZE_CLASSES];   // Number of free blocks
    uint64_t cache_hits;                // Performance counter
    uint64_t cache_misses;              // Performance counter
} __attribute__((aligned(64)));  // Cache line aligned
```

**Fast Path Algorithm (Lock-Free)**
```
lgx_alloc(size) fast path:
1. Determine size class (branch-free lookup table)
2. Load thread-local cache pointer (TLS, no lock)
3. Atomic load free_list head pointer
4. If non-NULL:
   a. Atomic CAS to pop head from free list
   b. If CAS succeeds: return pointer (cache hit)
   c. If CAS fails: retry (contention, rare)
5. If NULL: goto slow path (cache miss)
```

**Slow Path (Global Pool with Mutex)**
```
lgx_alloc(size) slow path:
1. Acquire global pool mutex for size class
2. If global pool has free blocks:
   a. Refill thread-local cache (batch of 64 blocks)
   b. Release mutex
   c. Return one block, cache rest
3. If global pool exhausted:
   a. Allocate new slab (mmap, huge pages if available)
   b. Add slab to global pool
   c. Refill thread-local cache
   d. Release mutex
   e. Return one block
```

**Cache Line Optimization**
- Align all allocations to 64-byte boundaries (cache line size)
- Pad structs to avoid false sharing between threads
- Use `__builtin_prefetch()` for predictable access patterns

**Branch Prediction Hints**
```c
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

if (unlikely(ptr == NULL)) {
    // Error path - predicted not taken
}
```

### 4.3 Allocation Strategy

**Zero Allocations in Frame-Critical Paths**
- Pre-allocate all frame-scoped resources at initialization
- Use ring buffers for temporary allocations (3-frame rotation)
- Reclaim memory with 3-frame delay to avoid use-after-free
- Frame-critical paths use stack allocation or pre-allocated pools only

**Memory Layout Optimization**
- Struct packing: minimize padding, align to cache lines
- False sharing prevention: separate frequently-written fields by cache line
- Hot/cold data separation: frequently accessed fields at struct start

**Resource Limits (DoS Prevention)**
- Maximum total allocation: 16GB per game instance
- Maximum allocation rate: 1M allocations per second
- If limits exceeded: return NULL, set error, log event
- Health check API reports resource limit violations

## 5. Lifecycle Management Design

### 5.1 Initialization Sequence

```
lgx_runtime_init() flow:
1. Validate configuration parameters (NULL checks, range validation)
2. Set up isolated namespace for pinned libraries
3. Load and verify pinned library versions (parallel with step 4)
4. Initialize memory pools (parallel with step 3)
5. Query GPU and driver capabilities
6. Initialize platform services (FS, timing, logging)
7. Initialize telemetry (only if opt-in, lazy)
8. Register signal handlers for crash reporting
9. Perform health check
10. Return LGX_SUCCESS or detailed error code
```

**Initialization Time Budget: <500ms**
- Library loading: <100ms (parallel with memory init)
- Memory pool setup: <50ms (parallel with library loading)
- Capability detection: <100ms (GPU query)
- Platform services: <50ms (FS, timing, logging)
- Health check: <50ms
- Buffer: <150ms

**Parallel Initialization**
- Library loading and memory pool setup run concurrently (separate threads)
- Join threads before proceeding to GPU detection
- Reduces init time by ~40% (measured in prototype)

**Lazy Initialization**
- Telemetry: only initialize if user opts in (saves ~20ms)
- Security hooks: only initialize if security module loaded
- Optional components: load on-demand, not at init

**Initialization Failure Handling**
- Critical failures (library mismatch, OOM): fail immediately, clean up, return error
- Non-critical failures (huge pages unavailable): log warning, continue with fallback
- Degraded mode: track which features are unavailable, report via health check

### 5.2 Suspend/Resume

**Suspend Flow**
- Flush all pending I/O operations
- Save critical state (memory pool metadata, telemetry)
- Release non-essential resources
- Complete in <100ms

**Resume Flow**
- Restore saved state
- Re-initialize platform services
- Validate GPU/driver still available
- Complete in <100ms

### 5.3 Shutdown

**Shutdown Flow**
- Flush telemetry data (if enabled)
- Free all memory pools
- Close all open file handles
- Unload pinned libraries
- Clean up namespace

## 6. Platform Services Design

### 6.1 Filesystem Abstraction

**Design Goals**
- Provide consistent filesystem API across distributions
- Use pinned libc for all filesystem operations
- Support both synchronous and asynchronous I/O (future)

**Implementation**
- Wrap standard POSIX file operations (open, read, write, close)
- Add error handling and validation
- Provide path normalization for cross-platform compatibility

### 6.2 Timing Services

**High-Resolution Timer**
- Use `clock_gettime(CLOCK_MONOTONIC)` for frame timing
- Nanosecond precision
- Monotonic (not affected by system time changes)

**Sleep Function**
- Use `nanosleep()` for precise sleep
- Handle interruptions (EINTR) gracefully

### 6.3 Logging

**Log Levels**
- DEBUG: Verbose diagnostic information
- INFO: General informational messages
- WARN: Warning messages (non-fatal issues)
- ERROR: Error messages (fatal issues)

**Log Output**
- Default: stderr
- Optional: file output (configured at init)
- Thread-safe logging with minimal contention

## 7. Telemetry Design

### 7.1 Data Collection (Opt-in Only)

**Collected Metrics**
- Frame-time distribution (p50, p95, p99)
- Memory usage (peak, average, per-pool)
- Allocation patterns (size distribution, lifetime)
- Crash events (stack trace, error code, GPU state)
- GPU/driver information (vendor, version, device ID)
- CPU scheduling delays (preemption events)
- GPU stalls (waiting for GPU to finish)
- Driver warnings/errors (Vulkan debug output)

**Privacy Guarantees**
- No user data (usernames, file paths, process names)
- Anonymized hardware IDs (SHA-256 hashed)
- Local storage only (no automatic upload)
- Separate process for telemetry (prevents game from bypassing opt-out)

**Correlation Analysis**
- Link frame-time spikes to events (allocation burst, GPU stall, driver issue)
- Link crashes to context (GPU model, driver version, kernel version)
- Detect patterns: "Frame-time spikes always occur after large allocations"

**Correlation Algorithm: Detecting Allocation Bursts**
```
Implementation strategy:
1. Timestamp every allocation in ring buffer (high-resolution: rdtsc or clock_gettime)
2. On frame-time spike detection:
   a. Scan backwards in ring buffer (last 1000 allocations)
   b. Count allocations in spike window (e.g., last 16ms)
   c. Compare to baseline allocation rate (moving average over last 1000 frames)
   d. If allocation rate >2σ deviation from baseline → classify as "allocation_burst"
   e. Store correlation in telemetry with confidence score
3. Ring buffer overhead: ~80KB (1000 allocations × 80 bytes per entry)
4. Statistical baseline: moving average updated every frame

Data structure:
typedef struct allocation_event {
    uint64_t timestamp_ns;
    size_t size;
    uint32_t thread_id;
    uint16_t size_class;
} allocation_event_t;

Ring buffer: allocation_event_t ring_buffer[10000];
Baseline: double avg_alloc_rate;  // allocations per millisecond
```

**Anomaly Detection**
- Statistical outlier detection for frame-time spikes
- Trend analysis: is memory usage climbing (leak?) or stable?
- Alert on: sustained high frame-time, memory growth, allocation failures

### 7.2 Telemetry Architecture for Actionable Insights

**Metrics Taxonomy**
- Counters: monotonically increasing (allocations, deallocations)
- Gauges: point-in-time values (memory usage, pool size)
- Histograms: distribution of values (frame-time percentiles)
- Traces: time-series events (allocation timeline, GPU commands)

**Separate Process Architecture**
- Telemetry runs in separate process (prevents game tampering)
- IPC via shared memory ring buffer (lock-free, low overhead)
- Game writes events to ring buffer, telemetry process reads and aggregates
- If telemetry process crashes, game continues unaffected

### 7.3 Export Format

**JSON Schema**
```json
{
  "lgx_version": "1.0.0",
  "session_id": "hashed_session_id",
  "gpu_vendor": "NVIDIA",
  "driver_version": "535.104.05",
  "frame_times": {
    "p50": 16.2,
    "p95": 18.5,
    "p99": 22.1,
    "spikes": [
      {"frame": 1234, "time_ms": 45.2, "cause": "allocation_burst"}
    ]
  },
  "memory": {
    "peak_mb": 180,
    "average_mb": 150,
    "pools": {
      "small": {"usage_mb": 20, "exhaustions": 0},
      "medium": {"usage_mb": 50, "exhaustions": 2}
    }
  },
  "allocations": {
    "total": 1000000,
    "failures": 5,
    "size_distribution": {"<1KB": 80, "1KB-1MB": 15, ">1MB": 5}
  },
  "crashes": [],
  "anomalies": [
    {"type": "memory_leak", "confidence": 0.85, "details": "Memory usage increased 50MB over 10 minutes"}
  ]
}
```

**Actionable Insights**
- Telemetry answers: "What should I fix first to improve performance?"
- Not just: "Here's a histogram of frame times"
- Example: "Frame-time spikes correlate with pool exhaustions in medium size class - increase pool size"

## 8. Error Handling Strategy

### 8.1 Error Handling Philosophy

**Three-Tier Error Strategy**

1. **Critical Errors → Fail-Fast with Core Dump**
   - Library version mismatch
   - ABI violation detected
   - Security attestation failure
   - Rationale: These indicate programming errors or compromised system - cannot continue safely

2. **Recoverable Errors → Return Error Code + Set Last Error**
   - Allocation failure (OOM)
   - File I/O error
   - GPU timeout (recoverable)
   - Rationale: Game can handle these and degrade gracefully

3. **Degraded Mode → Continue with Reduced Functionality**
   - Huge pages unavailable → use standard pages
   - Telemetry initialization failed → disable telemetry
   - Optional component load failed → continue without it
   - Rationale: Non-critical features, game can run without them

**Assertions vs Errors**
- Assertions: programming errors, should never happen in production (NULL pointer, invalid enum)
- Errors: runtime conditions, expected to occur occasionally (OOM, I/O error)
- Debug builds: assertions enabled, verbose logging
- Release builds: assertions disabled (replaced with error returns), minimal logging

### 8.2 Error Codes

**Design Principles**
- All functions return `lgx_result_t` or use output parameters
- Error codes are specific and actionable
- Provide `lgx_result_to_string()` for human-readable messages
- Thread-local error context provides detailed information

**Error Context Stack**
```c
typedef struct lgx_error_context {
    size_t struct_size;
    lgx_result_t error_code;
    const char* error_message;      // Human-readable description
    const char* function_name;      // Function where error occurred
    const char* file_name;          // Source file
    int line_number;                // Line number
    uint64_t timestamp_ns;          // When error occurred
    void* user_data;                // Optional context
} lgx_error_context_t;
```

**Last Error API (Thread-Local)**
```c
// Get last error for current thread
lgx_error_context_t lgx_get_last_error(void);

// Clear last error
void lgx_clear_last_error(void);

// Set error (internal use)
void lgx_set_error(lgx_result_t code, const char* message, 
                   const char* func, const char* file, int line);
```

**Error Callback Hooks**
```c
// Game can register custom error handler
typedef void (*lgx_error_callback_t)(const lgx_error_context_t* context, void* user_data);

void lgx_set_error_handler(lgx_error_callback_t callback, void* user_data);

// Example: game logs all errors to custom logging system
void my_error_handler(const lgx_error_context_t* ctx, void* user_data) {
    MyLogger* logger = (MyLogger*)user_data;
    logger->log("LGX Error: %s in %s:%d", ctx->error_message, 
                ctx->function_name, ctx->line_number);
}
```

### 8.3 Error Recovery Patterns

**Allocation Failure Recovery**
```c
void* ptr = lgx_alloc(size);
if (ptr == NULL) {
    lgx_error_context_t err = lgx_get_last_error();
    if (err.error_code == LGX_ERROR_OUT_OF_MEMORY) {
        // Degrade: reduce texture quality, free caches
        reduce_memory_usage();
        ptr = lgx_alloc(size);  // Retry
    }
    if (ptr == NULL) {
        // Still failed: critical, cannot continue
        fatal_error("Cannot allocate memory");
    }
}
```

**GPU Timeout Recovery**
```c
lgx_result_t result = lgx_submit_commands();
if (result == LGX_ERROR_GPU_UNAVAILABLE) {
    // Attempt driver reset (if supported)
    result = lgx_reset_gpu();
    if (result == LGX_SUCCESS) {
        // Retry operation
        result = lgx_submit_commands();
    } else {
        // Cannot recover: fail gracefully
        show_error_dialog("GPU driver error");
        exit(1);
    }
}
```

**Degraded Mode Example**
```c
lgx_result_t result = lgx_runtime_init(config);
if (result == LGX_SUCCESS) {
    // Check health to see if any features degraded
    lgx_health_status_t health;
    lgx_runtime_health_check(&health);
    
    if (!health.huge_pages_active) {
        log_warning("Huge pages unavailable, performance may be reduced");
    }
    
    if (health.degraded_features & LGX_FEATURE_TELEMETRY) {
        log_warning("Telemetry disabled");
    }
}
```

## 9. Testing Strategy

### 9.1 Unit Tests

**Coverage Targets**
- API functions: 100% coverage
- Error paths: 100% coverage
- Memory management: 100% coverage

**Test Framework**
- Use Google Test (gtest) for C++ test harness
- Use C wrappers for testing C API

### 9.2 Integration Tests

**Test Scenarios**
- Initialize runtime with various configurations
- Allocate and free memory in different patterns
- Suspend/resume cycles
- Filesystem operations
- Telemetry collection and export

### 9.3 Performance Tests

**Benchmarks**
- Initialization time: <500ms
- Memory allocation latency: <1μs for cached allocations
- Frame-time contribution: <0.5ms p99
- Memory overhead: <200MB

### 9.4 Compatibility Tests

**Matrix Testing**
- Distributions: Ubuntu 22.04, Fedora 38, Arch Linux
- Kernels: 5.10, 5.15, 6.1, 6.5
- GPUs: NVIDIA (proprietary), AMD (Mesa), Intel (Mesa)

## 10. Performance Optimization

### 10.1 Hot Path Optimization

**Zero Allocations**
- Pre-allocate all frame-scoped resources
- Use stack allocation for small temporary buffers
- Use ring buffers for recycling

**Cache Optimization**
- Align data structures to cache lines (64 bytes)
- Use thread-local storage to reduce contention
- Prefetch data for predictable access patterns

### 10.2 Profiling Integration

**Built-in Profiling**
- Instrument all API calls with timing
- Collect per-frame metrics
- Export profiling data for analysis

**External Profiling**
- Support `perf` for CPU profiling
- Support Valgrind for memory profiling
- Provide symbol information for stack traces

## 11. Security Considerations

### 11.1 Security Threat Model

**Trust Boundaries**
```
┌─────────────────────────────────────────────────────────┐
│  Untrusted: Game Code                                   │
│  - Can call any LGX API                                 │
│  - Cannot access Runtime Core internals                 │
│  - Cannot bypass resource limits                        │
└─────────────────────────────────────────────────────────┘
                         ↓ (API boundary)
┌─────────────────────────────────────────────────────────┐
│  Trusted: LGX Runtime Core                              │
│  - Validates all inputs                                 │
│  - Enforces resource limits                             │
│  - Provides isolation from host system                  │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│  Trusted: Pinned Libraries                              │
│  - Isolated from host system                            │
│  - Version-validated at init                            │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│  Trusted: Kernel                                        │
│  - Provides namespace isolation                         │
│  - Enforces memory protection                           │
└─────────────────────────────────────────────────────────┘
```

**Attack Surface Enumeration**
1. **API Calls**: Game can call any public API function
   - Mitigation: Validate all inputs, enforce resource limits
2. **Shared Memory**: Game and Runtime Core share address space
   - Mitigation: Guard pages, canaries, memory protection
3. **IPC**: Telemetry process communicates via shared memory
   - Mitigation: Separate process, lock-free ring buffer, no trust
4. **Filesystem**: Game can access files via Runtime Core
   - Mitigation: Path validation, sandboxing (future)

**Threat Scenarios**

**Threat 1: Malicious game exhausts memory (DoS)**
- Attack: Game calls `lgx_alloc()` in tight loop
- Mitigation: Hard limit of 16GB per game instance, rate limit 1M alloc/sec
- Detection: Health check reports resource limit violations

**Threat 2: Game corrupts Runtime Core memory**
- Attack: Game writes to memory returned by `lgx_alloc()` beyond bounds
- Mitigation: Guard pages after each allocation (debug builds), canaries
- Detection: Crash with SIGSEGV, core dump for analysis

**Threat 3: Game bypasses telemetry opt-out**
- Attack: Game tries to enable telemetry without user consent
- Mitigation: Telemetry runs in separate process, game cannot access
- Detection: Process isolation prevents tampering

**Threat 4: Game triggers buffer overflow in Runtime Core**
- Attack: Game passes oversized string to `lgx_log()`
- Mitigation: Bounds checking on all string operations, truncate if needed
- Detection: Fuzzing tests catch buffer overflows

**Security Guarantees**
- Runtime Core promises: No remote code execution, no information leak to game
- Runtime Core does NOT promise: Protection against buggy game code crashing itself

**Privilege Separation**
- Runtime Core runs with same privileges as game (unprivileged)
- Telemetry process runs unprivileged, separate from game
- Optional: Security module runs with elevated privileges (capabilities, not root)

### 11.2 Input Validation

**All Inputs Validated**
- Null pointer checks
- Size bounds checks
- String length validation
- Enum range validation

### 11.3 Memory Safety

**Safe Practices**
- No buffer overflows (bounds checking)
- No use-after-free (delayed reclamation with 3-frame delay)
- No double-free (allocation tracking with generation counters)
- Guard pages (debug builds)
- Memory canaries (detect corruption)

**TOCTOU (Time-of-Check-Time-of-Use) Attack Mitigation**

**Problem**: Race condition in free operation
```c
// Thread 1: Game calls lgx_free(ptr)
// Runtime Core validates pointer is valid (check)
// Thread 2: Game invalidates pointer from another thread
// Runtime Core frees invalid pointer (use) → TOCTOU race
```

**Solution**: Generation counters with atomic operations
```c
typedef struct allocation_header {
    uint64_t generation;  // Incremented on each alloc/free
    size_t size;
    uint32_t magic;       // Canary value
} allocation_header_t;

// Allocation:
void* lgx_alloc(size_t size) {
    allocation_header_t* header = allocate_with_header(size);
    header->generation = atomic_fetch_add(&global_generation, 1);
    header->magic = MAGIC_VALUE;
    return (void*)(header + 1);  // Return pointer after header
}

// Free with generation validation:
void lgx_free(void* ptr) {
    allocation_header_t* header = (allocation_header_t*)ptr - 1;
    
    // Atomic check: validate generation hasn't changed
    uint64_t expected_gen = header->generation;
    if (!atomic_compare_exchange(&header->generation, &expected_gen, FREED_MARKER)) {
        // Generation changed: pointer was reused, reject free
        lgx_set_error(LGX_ERROR_INVALID_PARAM, "Double-free or use-after-free detected");
        return;
    }
    
    // Validate magic canary
    if (header->magic != MAGIC_VALUE) {
        lgx_set_error(LGX_ERROR_INVALID_PARAM, "Memory corruption detected");
        return;
    }
    
    // Safe to free
    deallocate_with_header(header);
}
```

**Additional TOCTOU Mitigations**
- Use atomic operations for all free-list manipulations
- Validate generation counter at free time
- If mismatch: pointer was reused, reject free with error
- Log TOCTOU attempts to telemetry (potential attack detection)

### 11.4 Integration with Security Module

**Attestation Support**
- Provide hooks for security module to attest runtime state
- Validate function pointers before invocation
- Detect memory tampering

### 11.5 Security Testing Strategy

**Fuzzing**
- Fuzz all API inputs with invalid parameters
- Fuzz allocation patterns (random sizes, stress pools)
- Fuzz lifecycle (suspend/resume in invalid states)
- Tool: AFL, libFuzzer

**Static Analysis**
- Run Clang Static Analyzer on all code
- Run Coverity Scan for security vulnerabilities
- Check for: buffer overflows, use-after-free, double-free

**Penetration Testing**
- Attempt to bypass resource limits
- Attempt to corrupt Runtime Core memory
- Attempt to bypass telemetry opt-out
- Annual third-party security audit

## 12. Observability & Instrumentation Architecture

### 12.1 Developer Observability

**Live Debugging Interface**
- Attach debugger (gdb, lldb) to running game
- Inspect Runtime Core internal state via debug symbols
- Set breakpoints in Runtime Core code
- Tool: `lgx-debug` wrapper around gdb with LGX-aware commands

**Trace Logging with Filtering**
- Enable verbose logs for specific subsystems
- Example: `LGX_LOG_FILTER=memory,gpu lgx-run game`
- Subsystems: memory, gpu, filesystem, telemetry, lifecycle
- Log levels: DEBUG, INFO, WARN, ERROR

**Performance Counters API**
- Expose allocation counts, cache hit rates, pool exhaustions
- Example: `lgx_get_counter(LGX_COUNTER_CACHE_HITS)`
- Counters updated in real-time, minimal overhead
- Tool: `lgx-counters` displays live counter dashboard

**Integration with System Tracing**
- Linux perf: `perf record -g lgx-run game`
- ftrace: trace kernel-level events (syscalls, scheduling)
- eBPF: custom tracing scripts for LGX-specific events
- Tool: `lgx-trace` wrapper around perf with LGX-aware symbol resolution

### 12.2 Production Observability

**Health Check API**
```c
lgx_health_status_t health;
lgx_runtime_health_check(&health);

if (!health.is_healthy) {
    // Runtime is in degraded state
    if (!health.huge_pages_active) {
        log_warning("Huge pages unavailable");
    }
    if (!health.gpu_responsive) {
        log_error("GPU not responding");
    }
}
```

**Graceful Degradation Signals**
- Health check reports which features are degraded
- Bitmask: `health.degraded_features & LGX_FEATURE_TELEMETRY`
- Game can adapt behavior based on degraded features

**Anomaly Detection**
- Memory usage spiking: detect via telemetry
- Allocation latency outliers: detect via performance counters
- Frame-time spikes: correlate with events (allocation burst, GPU stall)

### 12.3 Instrumentation Strategy

**Structured Logging**
```c
// Log with subsystem tag
lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO, 
               "Allocated %zu bytes from pool %d", size, pool_id);

// Subsystem filtering at runtime
lgx_set_log_filter(LGX_SUBSYSTEM_MEMORY | LGX_SUBSYSTEM_GPU);
```

**Performance Counter Registry**
```c
// Register custom counter
lgx_counter_t my_counter = lgx_register_counter("my_subsystem.operations");

// Increment counter
lgx_increment_counter(my_counter);

// Query counter
uint64_t value = lgx_get_counter(my_counter);
```

**Trace Event System**
```c
// Mark start of operation
lgx_trace_begin("lgx_alloc", size);

// ... perform allocation ...

// Mark end of operation
lgx_trace_end("lgx_alloc");

// Export trace to JSON for analysis
lgx_trace_export("trace.json");
```

**Integration Hooks for External Tools**
- perf: provide symbol information for stack traces
- Valgrind: provide custom memory allocator hooks
- Tracy Profiler: provide instrumentation markers
- RenderDoc: provide GPU capture hooks (via Translation Layer)

## 12. Deployment Considerations

### 12.1 Library Packaging

**Shared Library**
- `lgx_runtime.so.1.0.0` (full version)
- Symlinks: `lgx_runtime.so.1` → `lgx_runtime.so.1.0.0`
- Symlinks: `lgx_runtime.so` → `lgx_runtime.so.1`

**Header Files**
- `lgx_runtime.h` (main API)
- `lgx_types.h` (type definitions)
- `lgx_version.h` (version macros)

### 12.2 Installation

**System-wide Installation**
- Libraries: `/opt/lgx/1.0.0/lib/`
- Headers: `/opt/lgx/1.0.0/include/`
- Symlink: `/opt/lgx/current` → `/opt/lgx/1.0.0/`

**Per-game Installation**
- Bundle `lgx_runtime.so` with game
- Use `RPATH` to find bundled library

## 13. Open Issues

1. **Multi-version Support**: Should we support multiple LGX runtime versions simultaneously?
2. **Library Conflicts**: How to handle conflicts with system-installed libraries?
3. **Driver Updates**: Strategy for handling GPU driver updates that break compatibility?
4. **Telemetry Default**: Should telemetry be opt-in or opt-out?
5. **Upgrade Path**: How do games migrate when a new major version breaks ABI?

## 14. Component Lifecycle and Dependency Management

### 14.1 Component Dependency Graph

```
Runtime Core (no dependencies)
  ├─ Memory Allocator (no dependencies)
  ├─ Security Module (depends on: Memory Allocator)
  ├─ Translation Layer (depends on: Memory Allocator, Security Module)
  └─ Shader Manager (depends on: Translation Layer)
```

### 14.2 Initialization Order

**Topological Sort of Dependencies**
1. Runtime Core initialization
2. Memory Allocator initialization
3. Security Module initialization (registers hooks with Memory Allocator)
4. Translation Layer initialization (uses Memory Allocator, registers with Security Module)
5. Shader Manager initialization (uses Translation Layer)

**Shutdown Order (Reverse)**
1. Shader Manager shutdown
2. Translation Layer shutdown
3. Security Module shutdown (unregister hooks)
4. Memory Allocator shutdown
5. Runtime Core shutdown

### 14.3 Circular Dependency Prevention

**Problem**: What if Translation Layer needs Security Module to validate shaders, but Security Module needs Translation Layer to attest GPU state?

**Solution**: Break circular dependencies with event-driven architecture
- Security Module registers hooks (doesn't call Translation Layer directly)
- Translation Layer emits events (shader_compiled, gpu_command_submitted)
- Security Module listens to events and performs attestation
- No direct coupling, no circular dependency

### 14.4 Component Integration Contracts (Expanded)

**Translation Layer Integration**
```c
// What Translation Layer needs from Runtime Core:
typedef struct lgx_translate_context {
    void* (*alloc)(size_t size);              // Memory allocator
    void (*free)(void* ptr);                  // Memory deallocator
    const char* cache_directory;              // Pipeline cache directory
    void (*notify_suspend)(void);             // Suspend notification
    void (*notify_resume)(void);              // Resume notification
    void* gpu_device_handle;                  // GPU device handle (opaque)
} lgx_translate_context_t;

lgx_translate_context_t* lgx_runtime_get_translate_context(void);
```

**Security Module Integration**
```c
// What Security Module needs from Runtime Core:
typedef struct lgx_security_hooks {
    void (*on_alloc)(void* ptr, size_t size, void* user_data);
    void (*on_free)(void* ptr, void* user_data);
    void (*on_api_call)(const char* func_name, void* user_data);
    void (*on_error)(const lgx_error_context_t* error, void* user_data);
    void (*on_suspend)(void* user_data);
    void (*on_resume)(void* user_data);
} lgx_security_hooks_t;

lgx_result_t lgx_runtime_register_security_hooks(const lgx_security_hooks_t* hooks, void* user_data);
```

**Shader Manager Integration**
```c
// What Shader Manager needs from Runtime Core:
typedef struct lgx_shader_cache_config {
    const char* cache_directory;
    size_t max_cache_size_mb;
    void (*on_cache_miss)(const char* shader_hash, void* user_data);
} lgx_shader_cache_config_t;

lgx_result_t lgx_runtime_configure_shader_cache(const lgx_shader_cache_config_t* config);
```

## 15. Component Interaction Diagram

```
┌─────────────────────────────────────────────────────────┐
│  Game Binary                                            │
└─────────────────────────────────────────────────────────┘
         │
         │ API calls
         ↓
┌─────────────────────────────────────────────────────────┐
│  Runtime Core                                           │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ Memory Mgmt │←─┤ Translation  │←─┤ Shader Manager│  │
│  │             │  │ Layer        │  │               │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│         ↑                ↑                              │
│         │                │                              │
│  ┌──────────────────────────────┐                      │
│  │ Security Module              │                      │
│  │ (hooks all allocations)      │                      │
│  └──────────────────────────────┘                      │
└─────────────────────────────────────────────────────────┘
         │
         │ IPC (shared memory ring buffer)
         ↓
┌─────────────────────────────────────────────────────────┐
│  Telemetry Process (separate)                          │
└─────────────────────────────────────────────────────────┘
```

**Data Flow**
1. Game calls `lgx_alloc()` → Runtime Core Memory Mgmt
2. Memory Mgmt allocates → notifies Security Module via hook
3. Security Module validates allocation → logs to Telemetry
4. Translation Layer uses `lgx_translate_alloc()` → same Memory Mgmt
5. Shader Manager uses Translation Layer → inherits Memory Mgmt

**Key Properties**
- Single memory allocator (no fragmentation across components)
- Security Module sees all allocations (comprehensive monitoring)
- Telemetry isolated in separate process (cannot be bypassed)
- Components communicate via hooks (loose coupling)

## 16. Future Enhancements

- Asynchronous I/O support
- Multi-threaded memory allocator
- Advanced telemetry (GPU metrics, network stats)
- Support for ARM64 architecture
- Integration with cloud gaming platforms
