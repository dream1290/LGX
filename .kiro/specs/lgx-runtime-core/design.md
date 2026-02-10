# LGX Runtime Core - Design Document

## 1. Design Overview

The LGX Runtime Core provides a stable C ABI layer that games link against, managing initialization, versioning, memory, lifecycle, and platform services. It uses layered isolation with pinned libraries to ensure deterministic behavior across Linux distributions.

**Revolutionary Enhancement**: The Runtime Core implements a **Telescoping Architecture** with intent-based APIs that enable incremental enhancement across 4 layers without breaking ABI compatibility.

**Core Innovation**: APIs capture **intent** (what, why, how you'll use resources), not just requirements (how much). This enables:
- Layer 1: Optimal pool selection based on intent with hybrid allocation strategy
- Layer 2: Predictive pre-warming based on learned patterns with validation
- Layer 3: Hardware-aware optimization with software fallbacks
- Layer 4: Selective verification of critical components only

**Key Architectural Refinements** (based on engineering review):
- **Hybrid Allocation**: Lock-free for hot paths, lock-based for cold paths, jemalloc fallback
- **Hardware Adaptation**: Graceful degradation across hardware tiers (OPTIMAL, COMPATIBLE, DEGRADED)
- **Intent Validation**: Runtime validates developer intent and adapts based on observed patterns
- **Tiered Performance**: MVP, Competitive, and Best-in-class performance targets

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

// Capability Detection with Hardware Tiers
typedef enum lgx_capability {
    LGX_CAP_DX11_TRANSLATION,
    LGX_CAP_DX12_TRANSLATION,
    LGX_CAP_SECURITY_MODULE,
    LGX_CAP_RAYTRACING,
    LGX_CAP_MESH_SHADERS,
    LGX_CAP_NUMA_AWARENESS,
    LGX_CAP_HUGE_PAGES,
    LGX_CAP_VENDOR_ACCELERATION,
} lgx_capability_t;

typedef enum lgx_hardware_tier {
    LGX_HW_TIER_OPTIMAL,        // Native hardware path, all features available
    LGX_HW_TIER_COMPATIBLE,     // Emulated features with performance penalty
    LGX_HW_TIER_DEGRADED,       // Missing hardware features, software fallback
} lgx_hardware_tier_t;

typedef struct lgx_hardware_status {
    size_t struct_size;
    lgx_hardware_tier_t achieved_tier;
    uint32_t missing_capabilities;  // Bitmask of LGX_CAP_*
    const char* degradation_reason;
    const char* performance_impact_estimate;  // "10-20% slower"
    const char* remediation_steps;            // "Enable huge pages: ..."
} lgx_hardware_status_t;

bool lgx_runtime_has_capability(lgx_capability_t cap);
lgx_result_t lgx_runtime_query_capabilities(lgx_capability_t* caps, size_t* count);
lgx_hardware_status_t lgx_runtime_get_hardware_status(void);
    LGX_CAP_DX12_TRANSLATION,
    LGX_CAP_SECURITY_MODULE,
    LGX_CAP_RAYTRACING,
    LGX_CAP_MESH_SHADERS,
} lgx_capability_t;

bool lgx_runtime_has_capability(lgx_capability_t cap);
lgx_result_t lgx_runtime_query_capabilities(lgx_capability_t* caps, size_t* count);

// Memory Management (Hybrid Strategy)
typedef enum lgx_allocator_strategy {
    LGX_ALLOC_LOCK_FREE,      // Lock-free (hot path, small allocations)
    LGX_ALLOC_LOCK_BASED,     // Mutex-based (cold path, large allocations)
    LGX_ALLOC_JEMALLOC,       // Delegate to jemalloc (fallback)
    LGX_ALLOC_AUTO,           // Runtime selects best strategy
} lgx_allocator_strategy_t;

void* lgx_alloc(size_t size);
void* lgx_alloc_aligned(size_t size, size_t alignment);
void* lgx_alloc_with_strategy(size_t size, lgx_allocator_strategy_t strategy);
void lgx_free(void* ptr);
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats);

// Memory Management with Enhanced Intent (Layer 1+)
typedef enum lgx_access_pattern {
    LGX_ACCESS_SEQUENTIAL,          // Sequential access (streaming)
    LGX_ACCESS_RANDOM,              // Random access (lookup tables)
    LGX_ACCESS_WRITE_ONCE,          // Write once, read many
    LGX_ACCESS_UNKNOWN,             // Developer doesn't know (runtime will learn)
} lgx_access_pattern_t;

typedef enum lgx_lifetime {
    LGX_LIFETIME_FRAME,             // Lives for one frame
    LGX_LIFETIME_LEVEL,             // Lives for current level/scene
    LGX_LIFETIME_SESSION,           // Lives for entire game session
    LGX_LIFETIME_UNKNOWN,           // Developer doesn't know (runtime will learn)
} lgx_lifetime_t;

typedef enum lgx_performance_hint {
    LGX_HINT_CRITICAL_PATH,         // Frame-critical, needs low latency
    LGX_HINT_BACKGROUND,            // Background task, latency tolerant
    LGX_HINT_BANDWIDTH_HUNGRY,      // Needs high bandwidth
    LGX_HINT_COMPUTE_HEAVY,         // CPU-intensive operations
    LGX_HINT_GPU_SHARED,            // Shared with GPU (zero-copy preferred)
} lgx_performance_hint_t;

typedef enum lgx_intent_validation {
    LGX_INTENT_TRUST,           // Trust developer intent, no validation
    LGX_INTENT_VALIDATE_WARN,   // Validate, warn on mismatch
    LGX_INTENT_VALIDATE_ADAPT,  // Validate, auto-adapt allocations
} lgx_intent_validation_t;

// Base intent structure (Layer 1)
typedef struct lgx_allocation_intent_base {
    size_t struct_size;             // For forward compatibility
    size_t size;                    // How much memory
    lgx_access_pattern_t access_pattern;  // How you'll access it
    lgx_lifetime_t lifetime;        // How long you'll keep it
    lgx_performance_hint_t hint;    // Performance requirements
    lgx_intent_validation_t validation_policy;  // How to handle intent mismatches
} lgx_allocation_intent_base_t;

// Extended intent for Layer 2+ (optional)
typedef struct lgx_allocation_intent_l2 {
    size_t struct_size;
    lgx_allocation_intent_base_t base;  // Embed base
    
    // Layer 2 specific
    uint8_t priority;                    // 0-255, for predictive prefetch
    bool enable_predictive_prefetch;
} lgx_allocation_intent_l2_t;

// Intent-based allocation (enables future layers)
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent);
void* lgx_alloc_with_intent_ex(const void* intent, size_t intent_type_id);

// Intent validation and learning
typedef struct lgx_allocation_usage {
    size_t struct_size;
    uint64_t access_count;
    lgx_access_pattern_t observed_pattern;  // What we actually saw
    double pattern_confidence;               // 0.0 to 1.0
    lgx_lifetime_t observed_lifetime;
    uint64_t actual_lifetime_ms;            // Measured lifetime
} lgx_allocation_usage_t;

lgx_result_t lgx_alloc_get_usage_stats(void* ptr, lgx_allocation_usage_t* usage);

// Performance Characteristics (Layer 1) - Probabilistic, not Mathematical Proofs
typedef struct lgx_performance_characteristics {
    size_t struct_size;
    
    // Algorithmic guarantees (provable)
    const char* alloc_complexity;           // "O(1) for cache hit, O(log n) for cache miss"
    size_t max_memory_overhead;             // Provable bound
    
    // Statistical characteristics (measured, not proven)
    struct {
        uint64_t p50_ns;    // Median
        uint64_t p95_ns;    // 95th percentile
        uint64_t p99_ns;    // 99th percentile
        uint64_t p999_ns;   // 99.9th percentile
        double confidence_interval;  // 0.95 for 95% CI
        size_t sample_size;          // Samples used for measurement
    } measured_init_time;
    
    struct {
        uint64_t p50_ns;
        uint64_t p95_ns;
        uint64_t p99_ns;
        uint64_t p999_ns;
        double confidence_interval;
        size_t sample_size;
    } measured_alloc_time;
    
    // System configuration for measurements
    const char* kernel_version;
    const char* cpu_model;
    bool real_time_kernel;      // PREEMPT_RT applied?
    bool cpu_isolation;         // isolcpus configured?
    const char* measurement_conditions;  // "80% CPU load, 90% memory used"
} lgx_performance_characteristics_t;

lgx_result_t lgx_runtime_get_performance_characteristics(
    lgx_performance_characteristics_t* chars
);

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

// Enhanced Telemetry with Privacy Framework
typedef enum lgx_telemetry_overflow {
    LGX_TEL_DROP_OLDEST,     // Ring buffer behavior
    LGX_TEL_DROP_NEWEST,     // Preserve history
    LGX_TEL_SAMPLE,          // Statistical sampling (adaptive)
} lgx_telemetry_overflow_t;

typedef struct lgx_privacy_policy {
    size_t struct_size;
    
    // What we collect (user can inspect)
    bool collect_frame_times;           // ✅ Safe (just numbers)
    bool collect_allocation_sizes;      // ✅ Safe (just numbers)
    bool collect_cpu_model;             // ⚠️ Fingerprinting risk
    bool collect_gpu_model;             // ⚠️ Fingerprinting risk
    bool collect_kernel_version;        // ⚠️ Fingerprinting risk
    
    // What we NEVER collect (guaranteed)
    // ❌ File paths, process names, user names, IP addresses, any PII
    
    // Anonymization techniques
    bool add_noise;                     // Add random noise to data
    double noise_stddev;                // 0.05 = 5% noise
    bool aggregate_only;                // Only send aggregates, not raw data
} lgx_privacy_policy_t;

typedef struct lgx_telemetry_config {
    size_t struct_size;
    bool enabled;
    size_t ring_buffer_size;
    lgx_telemetry_overflow_t overflow_policy;
    
    // Adaptive sampling
    bool adaptive_sampling;         // Reduce sample rate when buffer fills
    double min_sample_rate;         // 0.01 = 1% minimum (emergency mode)
    
    // Privacy settings
    lgx_privacy_policy_t privacy_policy;
} lgx_telemetry_config_t;

lgx_result_t lgx_telemetry_configure(const lgx_telemetry_config_t* config);
lgx_result_t lgx_telemetry_enable(bool opt_in);
lgx_result_t lgx_telemetry_export(char* buffer, size_t buffer_size);

// Transparency: user can inspect what is collected
lgx_privacy_policy_t lgx_telemetry_get_privacy_policy(void);
lgx_result_t lgx_telemetry_export_collected_data(const char* output_path);

// Enhanced Error Handling with Recovery Guidance
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
    LGX_ERROR_HARDWARE_DEGRADED,        // Hardware features unavailable
    LGX_ERROR_INTENT_VALIDATION_FAILED, // Intent doesn't match usage
} lgx_result_t;

typedef enum lgx_error_severity {
    LGX_SEV_WARNING,       // Can continue, but degraded
    LGX_SEV_ERROR,         // Cannot continue current operation
    LGX_SEV_FATAL,         // Cannot continue at all
} lgx_error_severity_t;

typedef enum lgx_recovery_action {
    LGX_RECOVER_RETRY,         // Retry operation
    LGX_RECOVER_DEGRADE,       // Continue with reduced quality
    LGX_RECOVER_ABORT,         // Abort operation, continue game
    LGX_RECOVER_SHUTDOWN,      // Shutdown gracefully
} lgx_recovery_action_t;

const char* lgx_result_to_string(lgx_result_t result);

// Enhanced Error Context API (thread-local)
typedef struct lgx_error_context {
    size_t struct_size;
    lgx_result_t error_code;
    const char* error_message;
    const char* function_name;
    const char* file_name;
    int line_number;
    uint64_t timestamp_ns;          // When error occurred
} lgx_error_context_t;

typedef struct lgx_error_context_ex {
    lgx_error_context_t base;
    
    // Recovery guidance
    lgx_error_severity_t severity;
    lgx_recovery_action_t suggested_action;
    const char* recovery_steps;    // Human-readable recovery steps
    bool recoverable;
    void* context_data;            // Optional context for recovery
} lgx_error_context_ex_t;

lgx_error_context_t lgx_get_last_error(void);
lgx_error_context_ex_t lgx_get_last_error_ex(void);
void lgx_clear_last_error(void);

// Error Callback
typedef void (*lgx_error_callback_t)(const lgx_error_context_ex_t* context, void* user_data);
void lgx_set_error_handler(lgx_error_callback_t callback, void* user_data);

// Enhanced Health Check API
typedef struct lgx_health_status {
    size_t struct_size;
    bool is_healthy;
    lgx_hardware_tier_t hardware_tier;
    bool huge_pages_active;
    bool gpu_responsive;
    bool numa_awareness_active;
    size_t memory_usage_mb;
    size_t memory_limit_mb;
    uint32_t allocation_failures;
    uint32_t degraded_features;  // Bitmask of features in degraded mode
    double cpu_overhead_percent; // Measured CPU overhead
    const char* degradation_summary;  // Human-readable summary
} lgx_health_status_t;

lgx_result_t lgx_runtime_health_check(lgx_health_status_t* status);

// Dynamic Resource Limits Configuration
typedef struct lgx_resource_limits {
    size_t struct_size;
    
    // Memory limits
    size_t max_memory_bytes;          // Absolute limit
    double max_memory_percent;        // 0.25 = 25% of system RAM
    bool use_percentage;               // If true, use percent instead of absolute
    
    // Allocation rate limits
    size_t max_alloc_per_second;      // Absolute limit
    bool adaptive_rate_limiting;       // Adjust based on system load
    
    // File handle limits
    size_t max_open_files;            // Absolute limit
    double max_files_percent;         // 0.10 = 10% of ulimit
} lgx_resource_limits_t;

typedef struct lgx_effective_limits {
    size_t effective_max_memory;       // Computed from config + system
    size_t effective_max_alloc_rate;
    size_t effective_max_files;
    const char* limit_source;          // "config" or "system" or "calculated"
} lgx_effective_limits_t;

lgx_result_t lgx_runtime_configure_limits(const lgx_resource_limits_t* limits);
lgx_effective_limits_t lgx_runtime_get_effective_limits(void);

// Configuration Validation Framework
typedef struct lgx_config_constraints {
    size_t min_memory_pool_size;       // 1MB
    size_t max_memory_pool_size;       // 16GB
    size_t min_thread_cache_objects;   // 8
    size_t max_thread_cache_objects;   // 256
} lgx_config_constraints_t;

lgx_result_t lgx_config_validate(
    const lgx_runtime_config_t* config,
    lgx_config_constraints_t* violated_constraints  // OUT: what failed
);

lgx_result_t lgx_config_sanitize(
    lgx_runtime_config_t* config,
    bool strict  // If true, fail on any invalid param
);

// Observability Levels
typedef enum lgx_observability_level {
    LGX_OBS_NONE,          // No overhead
    LGX_OBS_MINIMAL,       // Counters only (<0.1% overhead)
    LGX_OBS_NORMAL,        // Counters + errors + warnings (<0.5% overhead)
    LGX_OBS_DETAILED,      // Above + debug logs (<2% overhead)
    LGX_OBS_EXHAUSTIVE,    // Everything including traces (>5% overhead)
} lgx_observability_level_t;

void lgx_set_observability_level(lgx_observability_level_t level);

// Performance Counters API
typedef enum lgx_counter {
    LGX_COUNTER_ALLOCATIONS,
    LGX_COUNTER_DEALLOCATIONS,
    LGX_COUNTER_CACHE_HITS,
    LGX_COUNTER_CACHE_MISSES,
    LGX_COUNTER_POOL_EXHAUSTIONS,
    LGX_COUNTER_INTENT_MISMATCHES,     // Intent vs actual usage mismatches
    LGX_COUNTER_HARDWARE_FALLBACKS,   // Times we fell back to software
    LGX_COUNTER_NUMA_MIGRATIONS,      // Memory moved between NUMA nodes
} lgx_counter_t;

uint64_t lgx_get_counter(lgx_counter_t counter);
void lgx_reset_counters(void);

// Performance Counter Registry (for custom counters)
typedef uint32_t lgx_custom_counter_t;

lgx_custom_counter_t lgx_register_counter(const char* name);
void lgx_increment_counter(lgx_custom_counter_t counter);
void lgx_add_to_counter(lgx_custom_counter_t counter, uint64_t value);
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

## 4. Memory Management Design - Specialized Allocators

### 4.0 Design Philosophy: Right Tool for the Job

**Problem with General-Purpose Allocators:**
- Trying to optimize malloc/free for all use cases leads to complexity
- Games have predictable allocation patterns that don't need general solutions
- A "perfect" general allocator (P99 = 2 μs) is still 200x slower than a frame arena (P99 = 0.01 μs)

**Solution: Specialized Allocators**
- Frame Arena: Ultra-fast bump pointer for temporary per-frame data (80% of allocations)
- GPU Pool: Pre-allocated GPU memory with alignment guarantees (15% of allocations)
- Persistent Heap: Fragmentation-resistant allocator for long-lived data (5% of allocations)

**Key Insight:** Most game allocations are frame-scoped. Optimize for the common case.

### 4.1 Frame Arena Allocator

**Purpose:** Handle temporary per-frame allocations (80% of game allocations)

**Design:**
```c
// Triple-buffered frame arenas (prevents use-after-free)
typedef struct lgx_frame_arena {
    uint8_t* base;              // Base address (huge page aligned)
    size_t capacity;            // Total capacity (e.g., 64MB)
    size_t offset;              // Current allocation offset (bump pointer)
    uint32_t frame_index;       // Current frame number
    uint64_t allocations;       // Allocation counter
} lgx_frame_arena_t;

// Global state: 3 arenas for triple-buffering
lgx_frame_arena_t g_frame_arenas[3];
uint32_t g_current_frame = 0;

// Ultra-fast allocation (bump pointer)
void* lgx_frame_alloc(size_t size) {
    lgx_frame_arena_t* arena = &g_frame_arenas[g_current_frame % 3];
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    // Bump pointer allocation (no locks, no free list)
    size_t old_offset = arena->offset;
    size_t new_offset = old_offset + size;
    
    if (unlikely(new_offset > arena->capacity)) {
        // Arena exhausted: allocate from overflow pool
        return lgx_heap_alloc(size);  // Fallback to persistent heap
    }
    
    arena->offset = new_offset;
    arena->allocations++;
    
    return arena->base + old_offset;
}

// No individual free - reset entire arena at frame boundary
void lgx_frame_reset(void) {
    g_current_frame++;
    lgx_frame_arena_t* arena = &g_frame_arenas[g_current_frame % 3];
    
    // Reset arena (instant, no deallocation needed)
    arena->offset = 0;
    arena->allocations = 0;
    arena->frame_index = g_current_frame;
}
```

**Performance Characteristics:**
- Allocation: O(1), ~5-10 CPU cycles (0.01-0.02 μs on 3 GHz CPU)
- Free: Not needed (entire arena reset at frame boundary)
- Memory overhead: ~0% (no metadata per allocation)
- Fragmentation: 0% (linear allocation)

**Triple-Buffering Strategy:**
- Frame N: Allocate from arena 0
- Frame N+1: Allocate from arena 1 (arena 0 still in use by GPU)
- Frame N+2: Allocate from arena 2 (arena 0, 1 still in use)
- Frame N+3: Reset arena 0, allocate from it (safe, 3 frames old)

**Capacity Planning:**
- Typical frame: 10-50 MB of temporary allocations
- Arena size: 64 MB per arena (192 MB total for 3 arenas)
- Overflow: Falls back to persistent heap (rare, <1% of allocations)

**Use Cases:**
- Command buffers
- Temporary vertex/index data
- String formatting
- Intermediate computation results
- UI layout calculations

**Adaptive Sizing and Overflow Handling (NEW):**

The frame arena now supports adaptive sizing to handle varying workload demands:

```c
// Enhanced frame arena with adaptive sizing
typedef struct lgx_frame_arena {
    uint8_t* base;              // Base address (huge page aligned)
    size_t capacity;            // Current capacity (starts at 64MB)
    size_t max_capacity;        // Maximum capacity (256MB)
    size_t offset;              // Current allocation offset (bump pointer)
    uint32_t frame_index;       // Current frame number
    uint64_t allocations;       // Allocation counter
    
    // Adaptive sizing
    size_t peak_usage;          // Peak usage this frame
    size_t rolling_avg_usage;   // Rolling average over last 60 frames
    uint32_t overflow_count;    // Number of overflows
    uint64_t last_warning_time; // Last overflow warning timestamp (rate limiting)
    
    // Debugging
    struct allocation_histogram* histogram;  // Size distribution
    struct call_site_tracker* tracker;       // Top allocation sites
} lgx_frame_arena_t;

// Configuration API
void lgx_config_set_frame_arena_size(lgx_runtime_config_t* config, size_t size);
void lgx_config_set_frame_arena_max_size(lgx_runtime_config_t* config, size_t max_size);

// Enhanced allocation with overflow handling
void* lgx_frame_alloc(size_t size) {
    lgx_frame_arena_t* arena = &g_frame_arenas[g_current_frame % 3];
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    // Bump pointer allocation
    size_t old_offset = arena->offset;
    size_t new_offset = old_offset + size;
    
    if (unlikely(new_offset > arena->capacity)) {
        // Arena exhausted: handle overflow
        arena->overflow_count++;
        
        // Rate-limited warning (max 1 per second)
        uint64_t now = lgx_time_now_ns();
        if (now - arena->last_warning_time > 1000000000ULL) {
            lgx_log(LGX_LOG_WARN, 
                "[LGX WARNING] Frame arena overflow! Requested %zu bytes, but only %zu bytes available.\n"
                "Frame %u, Arena %u, Total usage: %zu / %zu bytes (%.1f%%)\n"
                "Falling back to persistent heap for this allocation.",
                size, arena->capacity - old_offset,
                g_current_frame, g_current_frame % 3,
                old_offset, arena->capacity, 
                (double)old_offset / arena->capacity * 100.0);
            arena->last_warning_time = now;
        }
        
        // Emit telemetry event
        lgx_telemetry_emit_overflow(g_current_frame, size, old_offset, arena->capacity);
        
        // Attempt adaptive growth (if below max capacity)
        if (arena->capacity < arena->max_capacity) {
            size_t new_capacity = arena->capacity * 2;
            if (new_capacity > arena->max_capacity) {
                new_capacity = arena->max_capacity;
            }
            
            if (lgx_frame_arena_grow(arena, new_capacity)) {
                lgx_log(LGX_LOG_INFO, 
                    "[LGX INFO] Frame arena grown from %zu MB to %zu MB",
                    arena->capacity / (1024*1024), new_capacity / (1024*1024));
                arena->capacity = new_capacity;
                
                // Retry allocation in grown arena
                arena->offset = new_offset;
                arena->allocations++;
                return arena->base + old_offset;
            }
        }
        
        // Fallback to persistent heap
        return lgx_heap_alloc(size);
    }
    
    arena->offset = new_offset;
    arena->allocations++;
    
    // Track peak usage
    if (new_offset > arena->peak_usage) {
        arena->peak_usage = new_offset;
    }
    
    // Update histogram (debug builds)
    #ifdef LGX_DEBUG
    lgx_histogram_add(arena->histogram, size);
    lgx_call_site_track(arena->tracker, size, __FILE__, __LINE__);
    #endif
    
    return arena->base + old_offset;
}

// Enhanced reset with usage tracking
void lgx_frame_reset(void) {
    g_current_frame++;
    lgx_frame_arena_t* arena = &g_frame_arenas[g_current_frame % 3];
    
    // Update rolling average (exponential moving average)
    arena->rolling_avg_usage = (arena->rolling_avg_usage * 59 + arena->peak_usage) / 60;
    
    // Early warning if usage exceeds 80%
    if (arena->peak_usage > arena->capacity * 0.8) {
        lgx_log(LGX_LOG_WARN,
            "[LGX WARNING] Frame arena usage high: %zu / %zu bytes (%.1f%%). "
            "Consider increasing arena size to %zu MB.",
            arena->peak_usage, arena->capacity,
            (double)arena->peak_usage / arena->capacity * 100.0,
            (arena->peak_usage * 2) / (1024*1024));
    }
    
    // Reset arena
    arena->offset = 0;
    arena->allocations = 0;
    arena->peak_usage = 0;
    arena->frame_index = g_current_frame;
}

// Debugging API
typedef struct lgx_frame_arena_stats {
    size_t struct_size;
    size_t capacity;
    size_t current_usage;
    size_t peak_usage;
    size_t rolling_avg_usage;
    uint64_t allocations;
    uint32_t overflow_count;
    size_t recommended_size;  // Based on observed usage
} lgx_frame_arena_stats_t;

lgx_result_t lgx_frame_get_stats(lgx_frame_arena_stats_t* stats);
lgx_result_t lgx_frame_arena_dump(const char* output_path);  // Export allocation map
```

**Adaptive Sizing Strategy:**
1. Start with 64MB default capacity
2. Monitor peak usage per frame with rolling average
3. Warn when usage exceeds 80% (early warning)
4. On overflow, attempt to double arena size (max 256MB)
5. If growth fails or max reached, fall back to persistent heap
6. Recommend optimal size based on observed patterns

**Overflow Handling:**
- Rate-limited warnings (max 1 per second) to prevent log spam
- Telemetry events with full context (frame, size, usage)
- Graceful fallback to persistent heap (<5% performance penalty)
- Track overflow statistics for debugging

**Debugging Tools:**
- Allocation histogram: size distribution per frame
- Call site tracking: top allocation locations
- Usage visualization: arena usage over time
- Profiler integration: Tracy, Optick support

### 4.2 GPU Memory Pool

**Purpose:** Pre-allocated GPU-visible memory with alignment guarantees

**Design:**
```c
// GPU memory types (Vulkan memory types)
typedef enum lgx_gpu_memory_type {
    LGX_GPU_DEVICE_LOCAL,       // GPU-only (fastest, VRAM)
    LGX_GPU_HOST_VISIBLE,       // CPU-writable, GPU-readable (staging)
    LGX_GPU_HOST_CACHED,        // CPU-readable, GPU-writable (readback)
} lgx_gpu_memory_type_t;

// GPU memory pool (per memory type)
typedef struct lgx_gpu_pool {
    VkDeviceMemory memory;      // Vulkan memory handle
    uint8_t* mapped_ptr;        // CPU-mapped pointer (if host-visible)
    size_t capacity;            // Total capacity
    
    // Buddy allocator for GPU memory
    struct buddy_allocator* allocator;
    
    // Alignment requirements
    size_t buffer_alignment;    // 256 bytes (Vulkan spec)
    size_t image_alignment;     // 4096 bytes (page size)
} lgx_gpu_pool_t;

// GPU allocation with alignment
void* lgx_gpu_alloc(size_t size, size_t alignment, lgx_gpu_memory_type_t type) {
    lgx_gpu_pool_t* pool = &g_gpu_pools[type];
    
    // Allocate from buddy allocator (O(log n))
    size_t offset = buddy_alloc(pool->allocator, size, alignment);
    
    if (offset == BUDDY_ALLOC_FAILED) {
        // Pool exhausted: log error, return NULL
        lgx_set_error(LGX_ERROR_OUT_OF_MEMORY, "GPU pool exhausted");
        return NULL;
    }
    
    // Return CPU pointer (if host-visible) or GPU offset
    if (pool->mapped_ptr) {
        return pool->mapped_ptr + offset;
    } else {
        return (void*)(uintptr_t)offset;  // GPU offset
    }
}

void lgx_gpu_free(void* ptr, lgx_gpu_memory_type_t type) {
    lgx_gpu_pool_t* pool = &g_gpu_pools[type];
    
    // Calculate offset
    size_t offset;
    if (pool->mapped_ptr) {
        offset = (uint8_t*)ptr - pool->mapped_ptr;
    } else {
        offset = (uintptr_t)ptr;
    }
    
    // Free in buddy allocator
    buddy_free(pool->allocator, offset);
}
```

**Buddy Allocator for GPU Memory:**
- Binary tree of free blocks (power-of-2 sizes)
- Allocation: O(log n), ~100-200 ns
- Coalescing: Automatic when adjacent blocks freed
- Fragmentation: <10% for typical workloads

**Memory Type Strategy:**
- Device-local (VRAM): 80% of GPU memory budget (textures, render targets)
- Host-visible (staging): 15% of GPU memory budget (upload buffers)
- Host-cached (readback): 5% of GPU memory budget (query results, screenshots)

**Capacity Planning:**
- Device-local: 2 GB (typical game VRAM usage)
- Host-visible: 256 MB (staging buffers)
- Host-cached: 64 MB (readback buffers)

**Use Cases:**
- Textures and render targets (device-local)
- Vertex/index buffers (device-local)
- Uniform buffers (host-visible, updated per-frame)
- Staging buffers (host-visible, for uploads)
- Query results (host-cached, for readback)

### 4.3 Persistent Heap Allocator

**Purpose:** Long-lived allocations with fragmentation resistance

**Design:**
```c
// Segregated fit allocator (size classes + large block allocator)
typedef struct lgx_persistent_heap {
    // Small allocations: segregated free lists (16B - 4KB)
    struct free_list {
        void* head;
        size_t block_size;
        uint32_t free_count;
    } size_classes[16];
    
    // Large allocations: buddy allocator (>4KB)
    struct buddy_allocator* large_allocator;
    
    // Defragmentation support
    bool defrag_enabled;
    uint64_t last_defrag_time;
} lgx_persistent_heap_t;

void* lgx_heap_alloc(size_t size) {
    if (size <= 4096) {
        // Small allocation: use segregated free list
        int class_index = size_to_class(size);
        struct free_list* list = &g_heap.size_classes[class_index];
        
        if (list->head) {
            // Pop from free list (O(1))
            void* ptr = list->head;
            list->head = *(void**)ptr;
            list->free_count--;
            return ptr;
        } else {
            // Allocate new slab from large allocator
            void* slab = buddy_alloc(g_heap.large_allocator, 64 * 1024, 4096);
            partition_slab(slab, list->block_size, list);
            return lgx_heap_alloc(size);  // Retry
        }
    } else {
        // Large allocation: use buddy allocator
        return buddy_alloc(g_heap.large_allocator, size, 16);
    }
}

void lgx_heap_free(void* ptr) {
    // Determine if small or large allocation
    if (is_small_allocation(ptr)) {
        // Return to free list
        int class_index = ptr_to_class(ptr);
        struct free_list* list = &g_heap.size_classes[class_index];
        
        *(void**)ptr = list->head;
        list->head = ptr;
        list->free_count++;
    } else {
        // Free in buddy allocator
        buddy_free(g_heap.large_allocator, ptr);
    }
}
```

**Defragmentation Strategy:**
- Trigger: During loading screens (when frame rate doesn't matter)
- Method: Compact free lists, coalesce buddy blocks
- Time budget: 100 ms per defragmentation pass
- Frequency: Every 10 minutes of gameplay, or when fragmentation >5%

**Performance Characteristics:**
- Small allocations (<4KB): O(1), ~50-100 ns
- Large allocations (>4KB): O(log n), ~200-500 ns
- Fragmentation: <5% over 8-hour sessions (with defragmentation)

**Use Cases:**
- Level data (geometry, textures, audio)
- Asset caches (shader cache, texture cache)
- Long-lived game objects (player state, inventory)
- Networking buffers
- Logging buffers

### 4.4 Unified Intent-Based API

**Purpose:** Automatically route allocations to the right allocator

```c
typedef enum lgx_allocation_lifetime {
    LGX_LIFETIME_FRAME,         // Frame arena
    LGX_LIFETIME_LEVEL,         // Persistent heap
    LGX_LIFETIME_SESSION,       // Persistent heap
} lgx_allocation_lifetime_t;

typedef enum lgx_allocation_usage {
    LGX_USAGE_CPU_ONLY,         // Frame arena or persistent heap
    LGX_USAGE_GPU_ONLY,         // GPU pool (device-local)
    LGX_USAGE_CPU_TO_GPU,       // GPU pool (host-visible)
    LGX_USAGE_GPU_TO_CPU,       // GPU pool (host-cached)
} lgx_allocation_usage_t;

typedef struct lgx_allocation_intent {
    size_t size;
    lgx_allocation_lifetime_t lifetime;
    lgx_allocation_usage_t usage;
    size_t alignment;           // 0 = default (16 bytes)
} lgx_allocation_intent_t;

void* lgx_alloc_with_intent(const lgx_allocation_intent_t* intent) {
    // Route to appropriate allocator
    if (intent->usage == LGX_USAGE_CPU_ONLY) {
        if (intent->lifetime == LGX_LIFETIME_FRAME) {
            return lgx_frame_alloc(intent->size);
        } else {
            return lgx_heap_alloc(intent->size);
        }
    } else {
        // GPU allocation
        lgx_gpu_memory_type_t type;
        if (intent->usage == LGX_USAGE_GPU_ONLY) {
            type = LGX_GPU_DEVICE_LOCAL;
        } else if (intent->usage == LGX_USAGE_CPU_TO_GPU) {
            type = LGX_GPU_HOST_VISIBLE;
        } else {
            type = LGX_GPU_HOST_CACHED;
        }
        
        size_t alignment = intent->alignment ? intent->alignment : 256;
        return lgx_gpu_alloc(intent->size, alignment, type);
    }
}
```

**Convenience Macros:**
```c
// Common allocation patterns
#define lgx_alloc_frame(size) \
    lgx_alloc_with_intent(&(lgx_allocation_intent_t){ \
        .size = size, \
        .lifetime = LGX_LIFETIME_FRAME, \
        .usage = LGX_USAGE_CPU_ONLY \
    })

#define lgx_alloc_persistent(size) \
    lgx_alloc_with_intent(&(lgx_allocation_intent_t){ \
        .size = size, \
        .lifetime = LGX_LIFETIME_SESSION, \
        .usage = LGX_USAGE_CPU_ONLY \
    })

#define lgx_alloc_gpu_texture(size) \
    lgx_alloc_with_intent(&(lgx_allocation_intent_t){ \
        .size = size, \
        .lifetime = LGX_LIFETIME_LEVEL, \
        .usage = LGX_USAGE_GPU_ONLY, \
        .alignment = 4096 \
    })
```

### 4.5 Memory Budget and Capacity Planning

**Total Memory Budget: 2.5 GB**
- Frame arenas: 192 MB (3 × 64 MB)
- GPU device-local: 2 GB
- GPU host-visible: 256 MB
- GPU host-cached: 64 MB
- Persistent heap: 512 MB (grows as needed)
- Runtime overhead: <200 MB

**Allocation Distribution (Typical Game):**
- Frame arena: 80% of allocations, 10% of memory
- GPU pool: 15% of allocations, 85% of memory
- Persistent heap: 5% of allocations, 5% of memory

**Performance Targets:**
- Frame arena: P99 < 0.1 μs (100 ns)
- GPU pool: P99 < 10 μs
- Persistent heap: P99 < 20 μs
- Overall: 95% of allocations < 0.1 μs (frame arena)

### 4.1 Memory Pool Architecture (DEPRECATED - Phase 0 Only)

**Note:** The following architecture was used in Phase 0 for prototyping and optimization experiments. Phase 1+ uses specialized allocators (frame arena, GPU pool, persistent heap) instead.

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

// Security Enhancements API
typedef enum lgx_security_level {
    LGX_SECURITY_STANDARD,      // Normal operations
    LGX_SECURITY_SENSITIVE,     // Constant-time operations
    LGX_SECURITY_CRITICAL,      // Maximum protection
} lgx_security_level_t;

// Side-channel resistant operations
#ifdef __x86_64__
#define LGX_SPECULATION_BARRIER() asm volatile("lfence" ::: "memory")
#else
#define LGX_SPECULATION_BARRIER() __sync_synchronize()
#endif

// Safe array access with speculation barrier
static inline bool lgx_safe_array_access(
    const void* array, 
    size_t index, 
    size_t array_size,
    void* result
) {
    if (index >= array_size) {
        return false;
    }
    
    LGX_SPECULATION_BARRIER();  // Prevent speculative read
    
    memcpy(result, (const uint8_t*)array + index, 1);
    return true;
}

// Sensitive data allocation
void* lgx_alloc_sensitive(size_t size);
void lgx_free_sensitive(void* ptr);
void* lgx_alloc_constant_time(size_t size);

// Secure random number generation
lgx_result_t lgx_random_bytes(void* buffer, size_t size);

// Security configuration
typedef struct lgx_security_config {
    size_t struct_size;
    bool constant_time_alloc;        // Allocation time independent of size
    bool randomize_allocations;      // ASLR for heap allocations
    bool zero_free_memory;           // Clear memory on free
    bool isolate_sensitive_data;     // Separate pool for sensitive data
} lgx_security_config_t;

lgx_result_t lgx_runtime_configure_security(const lgx_security_config_t* config);

// Chaos Testing Framework
typedef struct lgx_chaos_config {
    size_t struct_size;
    bool inject_memory_pressure;     // Randomly fail allocations
    double failure_rate;              // 0.01 = 1% of allocations fail
    
    bool inject_latency_spikes;      // Add random delays
    uint64_t max_latency_spike_ns;   // Up to 1ms delays
    
    bool inject_numa_imbalance;      // Simulate NUMA issues
    bool inject_gpu_hangs;           // Simulate driver hangs
} lgx_chaos_config_t;

lgx_result_t lgx_runtime_enable_chaos_testing(const lgx_chaos_config_t* config);