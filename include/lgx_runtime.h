#ifndef LGX_RUNTIME_H
#define LGX_RUNTIME_H

#include "lgx_types.h"
#include "lgx_version.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Core API functions
lgx_runtime_config_t* lgx_config_create(void);
void lgx_config_set_log_path(lgx_runtime_config_t* config, const char* path);
void lgx_config_set_memory_pool_size(lgx_runtime_config_t* config, size_t size);
void lgx_config_set_flags(lgx_runtime_config_t* config, uint32_t flags);
void lgx_config_destroy(lgx_runtime_config_t* config);

// Initialization and shutdown
lgx_result_t lgx_runtime_init(const lgx_runtime_config_t* config);
lgx_result_t lgx_runtime_shutdown(void);

// Version and compatibility
lgx_version_t lgx_runtime_get_version(void);
lgx_result_t lgx_runtime_check_compatibility(const lgx_version_t* required_version);

// Memory management
void* lgx_alloc(size_t size);
void* lgx_alloc_aligned(size_t size, size_t alignment);
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent);
void* lgx_alloc_with_intent_ex(const void* intent, size_t intent_type_id);
void lgx_free(void* ptr);
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats);

// Intent validation and learning
lgx_result_t lgx_alloc_get_usage_stats(void* ptr, lgx_allocation_usage_t* usage);
lgx_result_t lgx_alloc_validate_intent(void* ptr);  // Manually trigger validation

// Convenience macros for common allocation patterns
// These make intent-based allocation easier to use

// Convenience allocation functions (inline for performance)
static inline void* lgx_alloc_frame(size_t size) {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = size,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    return lgx_alloc_with_intent(&intent);
}

static inline void* lgx_alloc_level(size_t size) {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = size,
        .access_pattern = LGX_ACCESS_RANDOM,
        .lifetime = LGX_LIFETIME_LEVEL,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };
    return lgx_alloc_with_intent(&intent);
}

static inline void* lgx_alloc_persistent(size_t size) {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = size,
        .access_pattern = LGX_ACCESS_RANDOM,
        .lifetime = LGX_LIFETIME_SESSION,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };
    return lgx_alloc_with_intent(&intent);
}

static inline void* lgx_alloc_gpu_shared(size_t size) {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = size,
        .access_pattern = LGX_ACCESS_WRITE_ONCE,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_GPU_SHARED,
        .validation_policy = LGX_INTENT_TRUST
    };
    return lgx_alloc_with_intent(&intent);
}

// Performance measurement and assessment
lgx_result_t lgx_runtime_get_performance_characteristics(
    lgx_performance_characteristics_t* chars
);

lgx_performance_targets_t lgx_runtime_get_performance_targets(void);
lgx_result_t lgx_runtime_assess_performance(lgx_performance_assessment_t* assessment);
const char* lgx_performance_tier_to_string(lgx_performance_tier_t tier);

// Timing services
uint64_t lgx_time_now_ns(void);  // Nanoseconds since epoch
void lgx_time_sleep_ms(uint32_t milliseconds);

// Hardware adaptation and capability detection
bool lgx_runtime_has_capability(lgx_capability_t cap);
lgx_result_t lgx_runtime_query_capabilities(uint32_t* capabilities);
lgx_hardware_status_t lgx_runtime_get_hardware_status(void);

// Health check API
lgx_result_t lgx_runtime_health_check(lgx_health_status_t* status);

// Error handling
const char* lgx_result_to_string(lgx_result_t result);

// Enhanced error handling with recovery guidance
typedef struct lgx_error_context {
    size_t struct_size;
    lgx_result_t error_code;
    const char* error_message;
    const char* function_name;
    const char* file_name;
    int line_number;
    uint64_t timestamp_ns;
} lgx_error_context_t;

typedef struct lgx_error_context_ex {
    lgx_error_context_t base;
    lgx_error_severity_t severity;
    lgx_recovery_action_t suggested_action;
    const char* recovery_steps;
    bool recoverable;
    void* context_data;
} lgx_error_context_ex_t;

typedef void (*lgx_error_callback_t)(const lgx_error_context_ex_t* context, void* user_data);

lgx_error_context_t lgx_get_last_error(void);
lgx_error_context_ex_t lgx_get_last_error_ex(void);
void lgx_clear_last_error(void);
void lgx_set_error_handler(lgx_error_callback_t callback, void* user_data);

// Platform services - Filesystem
lgx_result_t lgx_fs_open(const char* path, const char* mode, lgx_file_t** file);
lgx_result_t lgx_fs_read(lgx_file_t* file, void* buffer, size_t size, size_t* bytes_read);
lgx_result_t lgx_fs_write(lgx_file_t* file, const void* buffer, size_t size);
lgx_result_t lgx_fs_close(lgx_file_t* file);

// Platform services - Logging
void lgx_log(lgx_log_level_t level, const char* format, ...);
void lgx_set_log_filter(lgx_log_level_t min_level);

// Structured logging with subsystem filtering
void lgx_log_tagged(lgx_log_subsystem_t subsystem, lgx_log_level_t level, 
                    const char* format, ...);
void lgx_set_subsystem_filter(uint32_t subsystem_mask);  // Bitmask of enabled subsystems
uint32_t lgx_get_subsystem_filter(void);
const char* lgx_subsystem_to_string(lgx_log_subsystem_t subsystem);

// Lifecycle management
lgx_result_t lgx_runtime_suspend(void);
lgx_result_t lgx_runtime_resume(void);

// Performance counters
uint64_t lgx_get_counter(lgx_counter_t counter);
void lgx_reset_counters(void);
lgx_custom_counter_t lgx_register_counter(const char* name);
void lgx_increment_counter(lgx_custom_counter_t counter);
void lgx_add_to_counter(lgx_custom_counter_t counter, uint64_t value);

// Observability
void lgx_set_observability_level(lgx_observability_level_t level);
lgx_observability_level_t lgx_get_observability_level(void);

// Intent allocator management (Task 3.4)
lgx_result_t lgx_intent_allocator_init(void);
lgx_result_t lgx_intent_allocator_shutdown(void);

// Intent statistics structure
typedef struct {
    uint64_t frame_allocations;
    uint64_t gpu_allocations;
    uint64_t persistent_allocations;
    uint64_t unknown_allocations;
    uint64_t intent_mismatches;
    uint64_t intent_validations;
    uint64_t total_intent_allocations;
    uint64_t total_intent_frees;
} lgx_intent_stats_t;

lgx_result_t lgx_intent_get_stats(lgx_intent_stats_t* stats);

#ifdef __cplusplus
}
#endif

// Telemetry with privacy framework
lgx_result_t lgx_telemetry_configure(const lgx_telemetry_config_t* config);
lgx_result_t lgx_telemetry_set_enabled(bool opt_in);
lgx_privacy_policy_t lgx_telemetry_get_privacy_policy(void);
lgx_result_t lgx_telemetry_export_collected_data(const char* output_path);

// Telemetry recording (internal use)
lgx_result_t lgx_telemetry_record_allocation(size_t size);
lgx_result_t lgx_telemetry_record_allocation_failure(size_t requested_size);

// Chaos testing framework
lgx_result_t lgx_runtime_enable_chaos_testing(const lgx_chaos_config_t* config);
lgx_result_t lgx_runtime_disable_chaos_testing(void);
bool lgx_runtime_is_chaos_testing_enabled(void);
lgx_chaos_config_t lgx_runtime_get_chaos_config(void);

// Trace event system
lgx_result_t lgx_trace_init(void);
lgx_result_t lgx_trace_shutdown(void);
lgx_result_t lgx_trace_enable(bool enabled);
bool lgx_trace_is_enabled(void);

void lgx_trace_begin(const char* name, uint64_t data);
void lgx_trace_end(const char* name);
void lgx_trace_instant(const char* name, uint64_t data);

lgx_result_t lgx_trace_export(const char* output_path);
lgx_result_t lgx_trace_clear(void);

// Trace statistics
typedef struct {
    uint64_t total_events;
    uint64_t begin_events;
    uint64_t end_events;
    uint64_t instant_events;
    uint64_t dropped_events;
    size_t buffer_capacity;
    size_t buffer_used;
} lgx_trace_stats_t;

lgx_result_t lgx_trace_get_stats(lgx_trace_stats_t* stats);

// Integration with external profiling tools
lgx_result_t lgx_trace_enable_perf_integration(bool enabled);
lgx_result_t lgx_trace_enable_valgrind_integration(bool enabled);
lgx_result_t lgx_trace_enable_tracy_integration(bool enabled);

// Convenience macros for scoped tracing
#define LGX_TRACE_SCOPE(name) \
    lgx_trace_begin(name, 0); \
    __attribute__((cleanup(lgx_trace_scope_cleanup))) int __trace_scope_##__LINE__ = 0; \
    (void)__trace_scope_##__LINE__

static inline void lgx_trace_scope_cleanup(int* dummy) {
    (void)dummy;
    lgx_trace_end("");
}

#ifdef __cplusplus
}
#endif

#endif // LGX_RUNTIME_H
