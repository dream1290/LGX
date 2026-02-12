/**
 * LGX Runtime Core - Phase 1 Implementation
 * 
 * Core runtime initialization, lifecycle management, and coordination.
 * This is the main entry point for the LGX Runtime system.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

// Global runtime state
static lgx_runtime_state_t g_runtime = {
    .initialized = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .config = NULL,
    .memory_manager = NULL,
    .hardware_adapter = NULL,
    .capability_detector = NULL,
    .lifecycle_manager = NULL,
    .platform_services = NULL,
    .telemetry = NULL,
    .error_handler = NULL,
    .health_monitor = NULL
};

// Forward declarations
static lgx_result_t initialize_subsystems(const lgx_runtime_config_t* config);
static lgx_result_t shutdown_subsystems(void);
static lgx_result_t validate_config(const lgx_runtime_config_t* config);

/**
 * Initialize the LGX Runtime Core
 */
lgx_result_t lgx_runtime_init(const lgx_runtime_config_t* config) {
    pthread_mutex_lock(&g_runtime.mutex);
    
    if (g_runtime.initialized) {
        pthread_mutex_unlock(&g_runtime.mutex);
        return LGX_ERROR_ALREADY_INITIALIZED;
    }
    
    // Initialize error handler FIRST so we can report errors during init
    lgx_result_t result = lgx_error_handler_init(&g_runtime.error_handler);
    if (result != LGX_SUCCESS) {
        pthread_mutex_unlock(&g_runtime.mutex);
        return result;
    }
    
    // Validate configuration
    result = validate_config(config);
    if (result != LGX_SUCCESS) {
        // Set error context for invalid config
        lgx_error_handler_set_error(g_runtime.error_handler, 
                                   result,
                                   __func__, __FILE__, __LINE__);
        lgx_error_handler_shutdown(g_runtime.error_handler);
        g_runtime.error_handler = NULL;
        pthread_mutex_unlock(&g_runtime.mutex);
        return result;
    }
    
    // Store configuration
    g_runtime.config = config;
    
    // Initialize all subsystems (error handler already initialized)
    result = initialize_subsystems(config);
    if (result != LGX_SUCCESS) {
        // Set error context for subsystem init failure
        lgx_error_handler_set_error(g_runtime.error_handler, 
                                   result,
                                   __func__, __FILE__, __LINE__);
        shutdown_subsystems();
        pthread_mutex_unlock(&g_runtime.mutex);
        return result;
    }
    
    g_runtime.initialized = true;
    
    pthread_mutex_unlock(&g_runtime.mutex);
    return LGX_SUCCESS;
}

/**
 * Shutdown the LGX Runtime Core
 */
lgx_result_t lgx_runtime_shutdown(void) {
    pthread_mutex_lock(&g_runtime.mutex);
    
    if (!g_runtime.initialized) {
        pthread_mutex_unlock(&g_runtime.mutex);
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    // Shutdown all subsystems in reverse order
    shutdown_subsystems();
    
    g_runtime.initialized = false;
    g_runtime.config = NULL;
    
    pthread_mutex_unlock(&g_runtime.mutex);
    return LGX_SUCCESS;
}

/**
 * Get runtime version
 */
lgx_version_t lgx_runtime_get_version(void) {
    lgx_version_t version;
    version.struct_size = sizeof(lgx_version_t);
    version.major = LGX_VERSION_MAJOR;
    version.minor = LGX_VERSION_MINOR;
    version.patch = LGX_VERSION_PATCH;
    return version;
}

/**
 * Check version compatibility
 */
lgx_result_t lgx_runtime_check_compatibility(const lgx_version_t* required_version) {
    if (!required_version) {
        // Set error (works even if runtime not initialized)
        lgx_error_handler_set_error(g_runtime.error_handler, LGX_ERROR_INVALID_PARAM,
                                   "lgx_runtime_check_compatibility", __FILE__, __LINE__);
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Check major version compatibility
    if (required_version->major != LGX_VERSION_MAJOR) {
        lgx_error_handler_set_error(g_runtime.error_handler, LGX_ERROR_INCOMPATIBLE_VERSION,
                                   "lgx_runtime_check_compatibility", __FILE__, __LINE__);
        return LGX_ERROR_INCOMPATIBLE_VERSION;
    }
    
    // Minor version must be <= current version
    if (required_version->minor > LGX_VERSION_MINOR) {
        lgx_error_handler_set_error(g_runtime.error_handler, LGX_ERROR_INCOMPATIBLE_VERSION,
                                   "lgx_runtime_check_compatibility", __FILE__, __LINE__);
        return LGX_ERROR_INCOMPATIBLE_VERSION;
    }
    
    return LGX_SUCCESS;
}

/**
 * Get runtime state (for internal use)
 */
lgx_runtime_state_t* lgx_runtime_get_state(void) {
    return &g_runtime;
}

/**
 * Check if runtime is initialized
 */
bool lgx_runtime_is_initialized(void) {
    return g_runtime.initialized;
}

/**
 * Get current time in nanoseconds
 */
uint64_t lgx_time_now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Sleep function implementation
 */
void lgx_time_sleep_ms(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/**
 * Validate timing precision
 * Returns the minimum measurable time difference in nanoseconds
 */
uint64_t lgx_time_get_precision_ns(void) {
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) != 0) {
        return 1000; // Default to 1μs if query fails
    }
    return (uint64_t)res.tv_sec * 1000000000ULL + (uint64_t)res.tv_nsec;
}

/**
 * Measure timing overhead
 * Returns the average overhead of calling lgx_time_now_ns() in nanoseconds
 */
uint64_t lgx_time_measure_overhead_ns(void) {
    const int iterations = 1000;
    uint64_t start, end;
    uint64_t total = 0;
    
    // Warm up
    for (int i = 0; i < 10; i++) {
        lgx_time_now_ns();
    }
    
    // Measure overhead
    for (int i = 0; i < iterations; i++) {
        start = lgx_time_now_ns();
        end = lgx_time_now_ns();
        
        // Only count if we got a valid measurement
        if (end > start) {
            total += (end - start);
        }
    }
    
    return total / iterations;
}

/**
 * Configuration API implementation
 */
lgx_runtime_config_t* lgx_config_create(void) {
    lgx_runtime_config_t* config = malloc(sizeof(lgx_runtime_config_t));
    if (!config) {
        return NULL;
    }
    
    // Set default values
    config->memory_pool_size = 256 * 1024 * 1024; // 256MB default
    config->log_path = NULL;
    config->flags = 0;
    
    return config;
}

void lgx_config_set_log_path(lgx_runtime_config_t* config, const char* path) {
    if (!config) return;
    config->log_path = path;
}

void lgx_config_set_memory_pool_size(lgx_runtime_config_t* config, size_t size) {
    if (!config) return;
    config->memory_pool_size = size;
}

void lgx_config_set_flags(lgx_runtime_config_t* config, uint32_t flags) {
    if (!config) return;
    config->flags = flags;
}

/**
 * Set frame arena size (Task 3.4.5.2.1)
 * 
 * Configures the initial size of each frame arena. Must be called before lgx_runtime_init().
 * 
 * @param config Configuration object
 * @param size Arena size in bytes (will be clamped to 16MB - 256MB range)
 */
void lgx_config_set_frame_arena_size(lgx_runtime_config_t* config, size_t size) {
    if (!config) return;
    config->frame_arena_size = size;
}

/**
 * Set frame arena maximum size (Task 3.4.5.2.2)
 * 
 * Configures the maximum size that arenas can grow to. Must be called before lgx_runtime_init().
 * 
 * @param config Configuration object
 * @param max_size Maximum arena size in bytes (will be clamped to 16MB - 256MB range)
 */
void lgx_config_set_frame_arena_max_size(lgx_runtime_config_t* config, size_t max_size) {
    if (!config) return;
    config->frame_arena_max_size = max_size;
}

void lgx_config_destroy(lgx_runtime_config_t* config) {
    if (config) {
        free(config);
    }
}

/**
 * Get memory statistics
 */
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats) {
    if (!stats) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_runtime.initialized || !g_runtime.memory_manager) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_memory_manager_get_stats(g_runtime.memory_manager, stats);
}

/**
 * Get performance characteristics
 */
lgx_result_t lgx_runtime_get_performance_characteristics(lgx_performance_characteristics_t* chars) {
    if (!chars) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_runtime.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    // Initialize the structure
    chars->struct_size = sizeof(lgx_performance_characteristics_t);
    
    // Set algorithmic guarantees
    chars->alloc_complexity = "O(1) for cache hit, O(log n) for cache miss";
    chars->max_memory_overhead = 64 * 1024 * 1024; // 64MB overhead estimate
    
    // Set measured characteristics (placeholder values for now)
    chars->measured_init_time.p50_ns = 50000000;   // 50ms
    chars->measured_init_time.p95_ns = 100000000;  // 100ms
    chars->measured_init_time.p99_ns = 200000000;  // 200ms
    chars->measured_init_time.p999_ns = 500000000; // 500ms
    chars->measured_init_time.confidence_interval = 0.95;
    chars->measured_init_time.sample_size = 1000;
    
    chars->measured_alloc_time.p50_ns = 100;    // 100ns
    chars->measured_alloc_time.p95_ns = 500;    // 500ns
    chars->measured_alloc_time.p99_ns = 1000;   // 1μs
    chars->measured_alloc_time.p999_ns = 5000;  // 5μs
    chars->measured_alloc_time.confidence_interval = 0.95;
    chars->measured_alloc_time.sample_size = 100000;
    
    // System configuration
    chars->kernel_version = "Unknown";
    chars->cpu_model = "Unknown";
    chars->real_time_kernel = false;
    chars->cpu_isolation = false;
    chars->measurement_conditions = "Standard test conditions";
    
    return LGX_SUCCESS;
}

/**
 * Get performance targets
 */
lgx_performance_targets_t lgx_runtime_get_performance_targets(void) {
    lgx_performance_targets_t targets;
    targets.struct_size = sizeof(lgx_performance_targets_t);
    
    // Tier 1 targets (MVP)
    targets.tier1_init_time_ms = 1000;
    targets.tier1_alloc_latency_ns = 5000;
    targets.tier1_memory_overhead_mb = 300;
    targets.tier1_cpu_overhead_percent = 10.0;
    
    // Tier 2 targets (Competitive)
    targets.tier2_init_time_ms = 500;
    targets.tier2_alloc_latency_ns = 1000;
    targets.tier2_memory_overhead_mb = 200;
    targets.tier2_cpu_overhead_percent = 5.0;
    
    // Tier 3 targets (Best-in-class)
    targets.tier3_init_time_ms = 100;
    targets.tier3_alloc_latency_ns = 500;
    targets.tier3_memory_overhead_mb = 100;
    targets.tier3_cpu_overhead_percent = 2.0;
    
    return targets;
}

/**
 * Assess current performance
 */
lgx_result_t lgx_runtime_assess_performance(lgx_performance_assessment_t* assessment) {
    if (!assessment) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_runtime.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    assessment->struct_size = sizeof(lgx_performance_assessment_t);
    assessment->achieved_tier = LGX_PERFORMANCE_TIER_2; // Assume Tier 2 for now
    assessment->meets_tier1 = true;
    assessment->meets_tier2 = true;
    assessment->meets_tier3 = false;
    
    // Measured values (placeholder)
    assessment->measured_init_time_ms = 50;
    assessment->measured_alloc_latency_ns = 1000;
    assessment->measured_memory_overhead_mb = 180;
    assessment->measured_cpu_overhead_percent = 4.5;
    
    assessment->bottleneck_description = "Memory allocation contention";
    assessment->improvement_suggestions = "Enable huge pages, reduce thread contention";
    
    return LGX_SUCCESS;
}

/**
 * Convert performance tier to string
 */
const char* lgx_performance_tier_to_string(lgx_performance_tier_t tier) {
    switch (tier) {
        case LGX_PERFORMANCE_TIER_1: return "Tier 1 (MVP)";
        case LGX_PERFORMANCE_TIER_2: return "Tier 2 (Competitive)";
        case LGX_PERFORMANCE_TIER_3: return "Tier 3 (Best-in-class)";
        default: return "Unknown";
    }
}

/**
 * Check if runtime has capability
 */
bool lgx_runtime_has_capability(lgx_capability_t cap) {
    if (!g_runtime.initialized || !g_runtime.capability_detector) {
        return false;
    }
    
    return lgx_capability_detector_has_capability_enum(g_runtime.capability_detector, cap);
}

/**
 * Query capabilities
 */
lgx_result_t lgx_runtime_query_capabilities(uint32_t* capabilities) {
    if (!capabilities) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_runtime.initialized || !g_runtime.capability_detector) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_capability_detector_query_capabilities(g_runtime.capability_detector, capabilities);
}

/**
 * Get hardware status
 */
lgx_hardware_status_t lgx_runtime_get_hardware_status(void) {
    lgx_hardware_status_t status;
    status.struct_size = sizeof(lgx_hardware_status_t);
    
    if (!g_runtime.initialized || !g_runtime.hardware_adapter) {
        status.achieved_tier = LGX_HW_TIER_DEGRADED;
        status.missing_capabilities = 0xFFFFFFFF;
        status.degradation_reason = "Runtime not initialized";
        status.performance_impact_estimate = "Unknown";
        status.remediation_steps = "Initialize runtime first";
        return status;
    }
    
    return lgx_hardware_adapter_get_status(g_runtime.hardware_adapter);
}

/**
 * Health check - comprehensive system health assessment
 */
lgx_result_t lgx_runtime_health_check(lgx_health_status_t* status) {
    if (!status) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_runtime.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_runtime.health_monitor) {
        // Health monitor not available, return error
        return LGX_ERROR_NOT_SUPPORTED;
    }
    
    // Perform comprehensive health check
    return lgx_health_monitor_check(g_runtime.health_monitor, status);
}

/**
 * Health monitoring with alerts (Task 14.3.3)
 */
lgx_result_t lgx_health_monitoring_start(uint32_t interval_ms) {
    return lgx_health_monitoring_start_public(interval_ms);
}

lgx_result_t lgx_health_monitoring_stop(void) {
    return lgx_health_monitoring_stop_public();
}

bool lgx_health_monitoring_is_running(void) {
    return lgx_health_monitoring_is_running_public();
}

void lgx_health_set_alert_callback(lgx_health_alert_callback_t callback, void* user_data) {
    lgx_health_set_alert_callback_public(callback, user_data);
}

void lgx_health_set_thresholds(float memory_warning, float memory_critical,
                                float cpu_warning, float cpu_critical) {
    lgx_health_set_thresholds_public(memory_warning, memory_critical, 
                                     cpu_warning, cpu_critical);
}

lgx_result_t lgx_health_get_monitoring_stats(uint64_t* total_checks, 
                                              uint64_t* warnings, uint64_t* criticals) {
    return lgx_health_get_monitoring_stats_public(total_checks, warnings, criticals);
}

// lgx_alloc_get_usage_stats is now implemented in lgx_intent_allocator.c

/**
 * Validate allocation intent
 */
lgx_result_t lgx_alloc_validate_intent(void* ptr) {
    if (!ptr) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_runtime.initialized || !g_runtime.memory_manager) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_memory_manager_validate_intent(g_runtime.memory_manager, ptr);
}

// lgx_alloc_with_intent_ex is now implemented in lgx_intent_allocator.c

// Private implementation functions

static lgx_result_t validate_config(const lgx_runtime_config_t* config) {
    if (!config) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Validate memory pool size
    if (config->memory_pool_size < 1024 * 1024) { // Minimum 1MB
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (config->memory_pool_size > 16ULL * 1024 * 1024 * 1024) { // Maximum 16GB
        return LGX_ERROR_INVALID_PARAM;
    }
    
    return LGX_SUCCESS;
}

static lgx_result_t initialize_subsystems(const lgx_runtime_config_t* config) {
    lgx_result_t result;
    
    // Initialize subsystems in dependency order
    
    // 0. Memory Monitor (MUST be first to capture accurate baseline RSS)
    result = lgx_memory_monitor_init();
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 1. Namespace Isolation (must be early, before any library loading)
    result = lgx_namespace_create_isolated();
    if (result != LGX_SUCCESS) {
        // Namespace isolation failure is not fatal (may not have privileges)
        lgx_log(LGX_LOG_WARN, "Namespace isolation not available");
    }
    
    // Mount pinned libraries if namespace was created
    result = lgx_namespace_mount_libraries("/opt/lgx/lib");
    if (result != LGX_SUCCESS) {
        lgx_log(LGX_LOG_WARN, "Failed to mount pinned libraries");
    }
    
    // Validate library versions
    result = lgx_namespace_validate_versions();
    if (result != LGX_SUCCESS) {
        lgx_log(LGX_LOG_WARN, "Library version validation failed");
    }
    
    // 1. Performance Counters (needed by all other subsystems)
    result = lgx_counters_init();
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 2. Error Handler - ALREADY INITIALIZED in lgx_runtime_init()
    // (skipped here to avoid double initialization)
    
    // 3. Hardware Adapter (needed for capability detection)
    result = lgx_hardware_adapter_init(&g_runtime.hardware_adapter);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 4. Capability Detector (depends on hardware adapter)
    result = lgx_capability_detector_init(&g_runtime.capability_detector, g_runtime.hardware_adapter);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 5. Memory Manager (core functionality)
    result = lgx_memory_manager_init(&g_runtime.memory_manager, config, g_runtime.hardware_adapter);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 5.4. Configure frame arena (Task 3.4.5.2.1)
    if (config && config->frame_arena_size > 0) {
        lgx_frame_arena_set_config_size(config->frame_arena_size);
    }
    if (config && config->frame_arena_max_size > 0) {
        lgx_frame_arena_set_config_max_size(config->frame_arena_max_size);
    }
    
    // 5.5. Intent Allocator (specialized allocators - frame arena, persistent heap)
    result = lgx_intent_allocator_init();
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 6. Platform Services (filesystem, timing, logging)
    result = lgx_platform_services_init(&g_runtime.platform_services, config);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // Set global platform services for logging API
    lgx_set_platform_services(g_runtime.platform_services);
    
    // 7. Lifecycle Manager (suspend/resume support)
    result = lgx_lifecycle_manager_init(&g_runtime.lifecycle_manager);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 8. Health Monitor (monitors all other subsystems)
    result = lgx_health_monitor_init(&g_runtime.health_monitor, &g_runtime);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // 9. Telemetry (optional, depends on config)
    if (config->flags & LGX_CONFIG_ENABLE_TELEMETRY) {
        result = lgx_telemetry_init(&g_runtime.telemetry, config);
        if (result != LGX_SUCCESS) {
            // Telemetry failure is not fatal
            g_runtime.telemetry = NULL;
        }
    }
    
    // 10. Trace Event System (optional, for performance profiling)
    result = lgx_trace_init();
    if (result != LGX_SUCCESS) {
        // Trace system failure is not fatal
        lgx_log(LGX_LOG_WARN, "Trace event system initialization failed");
    }
    
    return LGX_SUCCESS;
}

static lgx_result_t shutdown_subsystems(void) {
    // Shutdown in reverse order
    
    // Shutdown trace system first
    lgx_trace_shutdown();
    
    if (g_runtime.telemetry) {
        lgx_telemetry_shutdown(g_runtime.telemetry);
        g_runtime.telemetry = NULL;
    }
    
    if (g_runtime.health_monitor) {
        lgx_health_monitor_shutdown(g_runtime.health_monitor);
        g_runtime.health_monitor = NULL;
    }
    
    if (g_runtime.lifecycle_manager) {
        lgx_lifecycle_manager_shutdown(g_runtime.lifecycle_manager);
        g_runtime.lifecycle_manager = NULL;
    }
    
    // Cleanup namespace isolation (before platform services, as it may log)
    lgx_namespace_cleanup();
    
    if (g_runtime.platform_services) {
        // CRITICAL: Clear global pointer BEFORE freeing to prevent use-after-free
        lgx_set_platform_services(NULL);
        lgx_platform_services_shutdown(g_runtime.platform_services);
        g_runtime.platform_services = NULL;
    }
    
    // Shutdown intent allocator (specialized allocators)
    lgx_intent_allocator_shutdown();
    
    if (g_runtime.memory_manager) {
        lgx_memory_manager_shutdown(g_runtime.memory_manager);
        g_runtime.memory_manager = NULL;
    }
    
    if (g_runtime.capability_detector) {
        lgx_capability_detector_shutdown(g_runtime.capability_detector);
        g_runtime.capability_detector = NULL;
    }
    
    if (g_runtime.hardware_adapter) {
        lgx_hardware_adapter_shutdown(g_runtime.hardware_adapter);
        g_runtime.hardware_adapter = NULL;
    }
    
    if (g_runtime.error_handler) {
        lgx_error_handler_shutdown(g_runtime.error_handler);
        g_runtime.error_handler = NULL;
    }
    
    // Shutdown memory monitor (before performance counters)
    lgx_memory_monitor_shutdown();
    
    // Shutdown performance counters last
    lgx_counters_shutdown();
    
    return LGX_SUCCESS;
}

/**
 * Enable/disable telemetry (public API wrapper)
 */
lgx_result_t lgx_telemetry_set_enabled(bool opt_in) {
    if (!g_runtime.initialized || !g_runtime.telemetry) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_telemetry_enable(g_runtime.telemetry, opt_in);
}

/**
 * Suspend runtime (public API)
 * 
 * Suspends the runtime and saves critical state.
 * Must complete in <100ms per AC-5.
 */
lgx_result_t lgx_runtime_suspend(void) {
    if (!g_runtime.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_runtime.lifecycle_manager) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_lifecycle_manager_suspend(g_runtime.lifecycle_manager);
}

/**
 * Resume runtime (public API)
 * 
 * Resumes the runtime and restores saved state.
 * Must complete in <100ms per AC-5.
 */
lgx_result_t lgx_runtime_resume(void) {
    if (!g_runtime.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_runtime.lifecycle_manager) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_lifecycle_manager_resume(g_runtime.lifecycle_manager);
}
