/**
 * LGX Runtime Types - Core Type Definitions
 * 
 * This header defines all core types used by the LGX Runtime API.
 * Uses size-based versioning for forward compatibility.
 */

#ifndef LGX_TYPES_H
#define LGX_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward compatibility macro
#define LGX_STRUCT_SIZE(type) sizeof(type)

// Result codes
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
    LGX_ERROR_HARDWARE_DEGRADED,
    LGX_ERROR_INTENT_VALIDATION_FAILED,
    LGX_ERROR_SYSTEM_ERROR,
    LGX_ERROR_INVALID_STATE,
    LGX_ERROR_TIMEOUT,
    LGX_ERROR_NOT_FOUND,
    LGX_ERROR_COUNT
} lgx_result_t;

// Version structure with size-based versioning
typedef struct lgx_version {
    size_t struct_size;      // ALWAYS FIRST FIELD - for forward compatibility
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} lgx_version_t;

// Hardware tier classification
typedef enum lgx_hardware_tier {
    LGX_HW_TIER_OPTIMAL,        // Native hardware path, all features available
    LGX_HW_TIER_COMPATIBLE,     // Emulated features with performance penalty
    LGX_HW_TIER_DEGRADED,       // Missing hardware features, software fallback
} lgx_hardware_tier_t;

// Capability flags
typedef enum lgx_capability {
    LGX_CAP_HUGE_PAGES = 0,
    LGX_CAP_NUMA_AWARENESS = 1,
    LGX_CAP_GPU_ACCELERATION = 2,
    LGX_CAP_FAST_ALLOCATOR = 3,
    LGX_CAP_TELEMETRY = 4,
    LGX_CAP_DX11_TRANSLATION = 5,
    LGX_CAP_DX12_TRANSLATION = 6,
    LGX_CAP_SECURITY_MODULE = 7,
    LGX_CAP_RAYTRACING = 8,
    LGX_CAP_MESH_SHADERS = 9,
} lgx_capability_t;

// Performance tier classification
typedef enum lgx_performance_tier {
    LGX_PERFORMANCE_TIER_1,           // MVP - Minimum viable product
    LGX_PERFORMANCE_TIER_2,           // Competitive - Target performance
    LGX_PERFORMANCE_TIER_3,           // Best-in-class - Aspirational performance
} lgx_performance_tier_t;

// Intent-based allocation enums
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

// Error severity levels
typedef enum lgx_error_severity {
    LGX_SEV_WARNING,       // Can continue, but degraded
    LGX_SEV_ERROR,         // Cannot continue current operation
    LGX_SEV_FATAL,         // Cannot continue at all
} lgx_error_severity_t;

// Recovery action recommendations
typedef enum lgx_recovery_action {
    LGX_RECOVER_RETRY,         // Retry operation
    LGX_RECOVER_DEGRADE,       // Continue with reduced quality
    LGX_RECOVER_ABORT,         // Abort operation, continue game
    LGX_RECOVER_SHUTDOWN,      // Shutdown gracefully
} lgx_recovery_action_t;

// Log levels
typedef enum lgx_log_level {
    LGX_LOG_DEBUG,
    LGX_LOG_INFO,
    LGX_LOG_WARN,
    LGX_LOG_ERROR,
} lgx_log_level_t;

// Log subsystems (for structured logging with filtering)
typedef enum lgx_log_subsystem {
    LGX_SUBSYSTEM_CORE,           // Runtime core
    LGX_SUBSYSTEM_MEMORY,         // Memory management
    LGX_SUBSYSTEM_GPU,            // GPU operations
    LGX_SUBSYSTEM_FILESYSTEM,     // Filesystem operations
    LGX_SUBSYSTEM_TELEMETRY,      // Telemetry collection
    LGX_SUBSYSTEM_LIFECYCLE,      // Lifecycle management
    LGX_SUBSYSTEM_HARDWARE,       // Hardware adaptation
    LGX_SUBSYSTEM_SECURITY,       // Security operations
    LGX_SUBSYSTEM_COUNT           // Number of subsystems
} lgx_log_subsystem_t;

// Observability levels
typedef enum lgx_observability_level {
    LGX_OBS_NONE,          // No overhead
    LGX_OBS_MINIMAL,       // Counters only (<0.1% overhead)
    LGX_OBS_NORMAL,        // Counters + errors + warnings (<0.5% overhead)
    LGX_OBS_DETAILED,      // Above + debug logs (<2% overhead)
    LGX_OBS_EXHAUSTIVE,    // Everything including traces (>5% overhead)
} lgx_observability_level_t;

// Performance counter types
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

// Custom counter handle
typedef uint32_t lgx_custom_counter_t;

// Configuration flags
#define LGX_CONFIG_ENABLE_TELEMETRY     (1 << 0)
#define LGX_CONFIG_ENABLE_DEBUG_LOGGING (1 << 1)
#define LGX_CONFIG_STRICT_VALIDATION    (1 << 2)
#define LGX_CONFIG_ENABLE_HUGE_PAGES    (1 << 3)
#define LGX_CONFIG_ENABLE_NUMA          (1 << 4)

// Opaque handles
typedef struct lgx_runtime_config lgx_runtime_config_t;
typedef struct lgx_file lgx_file_t;

// Hardware status structure
typedef struct lgx_hardware_status {
    size_t struct_size;
    lgx_hardware_tier_t achieved_tier;
    uint32_t missing_capabilities;  // Bitmask of missing features
    const char* degradation_reason;
    const char* performance_impact_estimate;  // "10-20% slower"
    const char* remediation_steps;            // "Enable huge pages: ..."
    
    // Hardware details
    bool huge_pages_available;
    bool numa_topology_detected;
    bool gpu_acceleration_available;
    const char* gpu_vendor;
    const char* cpu_features;
    uint32_t numa_node_count;
} lgx_hardware_status_t;

// Performance targets for each tier
typedef struct lgx_performance_targets {
    size_t struct_size;
    
    // Tier 1 targets (MVP)
    uint32_t tier1_init_time_ms;
    uint64_t tier1_alloc_latency_ns;
    size_t tier1_memory_overhead_mb;
    float tier1_cpu_overhead_percent;
    
    // Tier 2 targets (Competitive)
    uint32_t tier2_init_time_ms;
    uint64_t tier2_alloc_latency_ns;
    size_t tier2_memory_overhead_mb;
    float tier2_cpu_overhead_percent;
    
    // Tier 3 targets (Best-in-class)
    uint32_t tier3_init_time_ms;
    uint64_t tier3_alloc_latency_ns;
    size_t tier3_memory_overhead_mb;
    float tier3_cpu_overhead_percent;
} lgx_performance_targets_t;

// Health status enumeration
typedef enum lgx_health_level {
    LGX_HEALTH_GOOD,       // All systems operating normally
    LGX_HEALTH_WARNING,    // Some degradation, but functional
    LGX_HEALTH_CRITICAL,   // Severe degradation, may fail soon
    LGX_HEALTH_FAILED      // System has failed
} lgx_health_level_t;

// Health status structure
typedef struct lgx_health_status {
    size_t struct_size;
    lgx_health_level_t overall_health;
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
    
    // Resource usage details
    size_t frame_arena_usage_mb;
    size_t gpu_pool_usage_mb;
    size_t persistent_heap_usage_mb;
    
    // Performance metrics
    uint64_t avg_alloc_latency_ns;
    uint64_t p99_alloc_latency_ns;
    uint32_t cache_hit_rate_percent;
    
    // Early warning indicators
    bool approaching_memory_limit;    // >80% of limit
    bool high_allocation_failure_rate; // >1% failures
    bool high_cpu_overhead;           // >5% overhead
} lgx_health_status_t;

// Performance assessment result
typedef struct lgx_performance_assessment {
    size_t struct_size;
    lgx_performance_tier_t achieved_tier;
    bool meets_tier1;
    bool meets_tier2;
    bool meets_tier3;
    
    // Measured values
    uint32_t measured_init_time_ms;
    uint64_t measured_alloc_latency_ns;
    size_t measured_memory_overhead_mb;
    float measured_cpu_overhead_percent;
    
    // Bottleneck analysis
    const char* bottleneck_description;
    const char* improvement_suggestions;
} lgx_performance_assessment_t;

// Chaos testing configuration
typedef struct lgx_chaos_config {
    size_t struct_size;
    bool inject_memory_pressure;     // Randomly fail allocations
    double failure_rate;              // 0.01 = 1% of allocations fail
    
    bool inject_latency_spikes;      // Add random delays
    uint64_t max_latency_spike_ns;   // Up to 1ms delays
    double latency_spike_rate;       // 0.01 = 1% of operations get delays
    
    bool inject_numa_imbalance;      // Simulate NUMA issues
    bool inject_gpu_hangs;           // Simulate driver hangs
    double gpu_hang_rate;            // 0.001 = 0.1% of GPU operations hang
    
    bool inject_io_errors;           // Simulate I/O failures
    double io_error_rate;            // 0.01 = 1% of I/O operations fail
    
    // Random seed for reproducibility
    uint32_t random_seed;            // 0 = use time-based seed
} lgx_chaos_config_t;

// Memory pool statistics
typedef struct lgx_pool_stats {
    size_t size_class;
    size_t total_allocated;
    size_t current_free;
    size_t slab_count;
} lgx_pool_stats_t;

// Performance characteristics structure
typedef struct lgx_performance_characteristics {
    size_t struct_size;
    
    // Algorithmic guarantees (provable)
    const char* alloc_complexity;           // "O(1) for cache hit, O(log n) for cache miss"
    size_t max_memory_overhead;             // Provable bound
    
    // Measured initialization time (statistical, not proven)
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

// Memory statistics
typedef struct lgx_memory_stats {
    size_t struct_size;
    uint64_t total_allocated;
    uint64_t total_deallocated;
    uint64_t current_allocated;
    uint64_t peak_allocated;
    uint64_t allocation_count;      // For compatibility with tests
    uint64_t deallocation_count;    // For compatibility with tests
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    // Pool statistics
    size_t pool_count;
    lgx_pool_stats_t pool_stats[16]; // Up to 16 size classes
} lgx_memory_stats_t;

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

// Allocation usage tracking
typedef struct lgx_allocation_usage {
    size_t struct_size;
    uint64_t access_count;
    lgx_access_pattern_t observed_pattern;  // What we actually saw
    double pattern_confidence;               // 0.0 to 1.0
    lgx_lifetime_t observed_lifetime;
    uint64_t actual_lifetime_ms;            // Measured lifetime
    uint64_t allocation_timestamp_ns;       // When allocated
    uint64_t first_access_timestamp_ns;     // When first accessed
    uint64_t last_access_timestamp_ns;      // When last accessed
    bool intent_mismatch_detected;          // True if intent != observed
} lgx_allocation_usage_t;

// Telemetry overflow policy
typedef enum lgx_telemetry_overflow {
    LGX_TEL_DROP_OLDEST,    // Drop oldest events when buffer full
    LGX_TEL_DROP_NEWEST,    // Drop newest events when buffer full
    LGX_TEL_SAMPLE,         // Adaptive sampling when buffer full
} lgx_telemetry_overflow_t;

// Privacy policy for telemetry
typedef struct lgx_privacy_policy {
    size_t struct_size;
    bool collect_frame_times;
    bool collect_allocation_sizes;
    bool collect_cpu_model;
    bool collect_gpu_model;
    bool collect_kernel_version;
    bool add_noise;              // Add differential privacy noise
    double noise_stddev;         // Standard deviation for noise
    bool aggregate_only;         // Only collect aggregates, not raw events
} lgx_privacy_policy_t;

// Telemetry configuration
typedef struct lgx_telemetry_config {
    size_t struct_size;
    bool enabled;
    size_t ring_buffer_size;
    lgx_telemetry_overflow_t overflow_policy;
    bool adaptive_sampling;
    double min_sample_rate;
    lgx_privacy_policy_t privacy_policy;
} lgx_telemetry_config_t;

#ifdef __cplusplus
}
#endif

#endif // LGX_TYPES_H