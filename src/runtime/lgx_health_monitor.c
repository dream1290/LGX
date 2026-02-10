/**
 * LGX Health Monitor - Phase 1 Implementation
 * 
 * Monitors the health of all runtime subsystems.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <pthread.h>

// Health monitor state
struct lgx_health_monitor {
    pthread_mutex_t mutex;
    lgx_runtime_state_t* runtime_state;
    
    // Health status
    lgx_health_status_t last_status;
    uint64_t last_check_time;
    
    // Continuous monitoring (Task 14.3.3)
    bool monitoring_enabled;
    pthread_t monitoring_thread;
    bool monitoring_thread_running;
    uint32_t monitoring_interval_ms;  // How often to check health
    
    // Alert thresholds
    float memory_warning_threshold;    // % of limit (default: 0.8 = 80%)
    float memory_critical_threshold;   // % of limit (default: 0.95 = 95%)
    float cpu_warning_threshold;       // % overhead (default: 5.0%)
    float cpu_critical_threshold;      // % overhead (default: 10.0%)
    
    // Alert callback
    lgx_health_alert_callback_t alert_callback;
    void* alert_callback_user_data;
    
    // Statistics
    uint64_t total_checks;
    uint64_t warnings_triggered;
    uint64_t criticals_triggered;
};

/**
 * Initialize health monitor
 */
lgx_result_t lgx_health_monitor_init(lgx_health_monitor_t** monitor,
                                    lgx_runtime_state_t* runtime_state) {
    if (!monitor || !runtime_state) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_health_monitor_t* hm = calloc(1, sizeof(lgx_health_monitor_t));
    if (!hm) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    if (pthread_mutex_init(&hm->mutex, NULL) != 0) {
        free(hm);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    hm->runtime_state = runtime_state;
    
    // Initialize health status
    hm->last_status.struct_size = sizeof(lgx_health_status_t);
    hm->last_status.overall_health = LGX_HEALTH_GOOD;
    hm->last_status.memory_usage_mb = 0;
    hm->last_status.cpu_overhead_percent = 0.0;
    hm->last_status.degraded_features = 0;
    hm->last_check_time = 0;
    
    // Initialize monitoring (Task 14.3.3)
    hm->monitoring_enabled = false;
    hm->monitoring_thread_running = false;
    hm->monitoring_interval_ms = 1000;  // Default: check every 1 second
    
    // Set default alert thresholds
    hm->memory_warning_threshold = 0.8f;   // 80%
    hm->memory_critical_threshold = 0.95f; // 95%
    hm->cpu_warning_threshold = 5.0f;      // 5%
    hm->cpu_critical_threshold = 10.0f;    // 10%
    
    hm->alert_callback = NULL;
    hm->alert_callback_user_data = NULL;
    
    // Initialize statistics
    hm->total_checks = 0;
    hm->warnings_triggered = 0;
    hm->criticals_triggered = 0;
    
    *monitor = hm;
    return LGX_SUCCESS;
}

/**
 * Shutdown health monitor
 */
lgx_result_t lgx_health_monitor_shutdown(lgx_health_monitor_t* monitor) {
    if (!monitor) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Stop monitoring if running
    if (monitor->monitoring_enabled) {
        lgx_health_monitor_stop_monitoring(monitor);
    }
    
    pthread_mutex_destroy(&monitor->mutex);
    free(monitor);
    return LGX_SUCCESS;
}

/**
 * Perform comprehensive health check
 */
lgx_result_t lgx_health_monitor_check(lgx_health_monitor_t* monitor,
                                     lgx_health_status_t* status) {
    if (!monitor || !status) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Validate struct size for ABI compatibility
    // Allow smaller sizes (old versions) but not larger (future versions we don't know about)
    if (status->struct_size > sizeof(lgx_health_status_t)) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Minimum size must include at least the basic fields
    // (struct_size, overall_health, hardware_tier, huge_pages_active, gpu_responsive, memory_usage_mb, memory_limit_mb)
    size_t min_size = offsetof(lgx_health_status_t, memory_limit_mb) + sizeof(size_t);
    if (status->struct_size < min_size) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    // Save caller's struct size before we modify anything
    size_t caller_struct_size = status->struct_size;
    
    // Initialize health status - only zero out the size provided by caller
    memset(status, 0, caller_struct_size);
    status->struct_size = caller_struct_size;  // Restore after zeroing
    status->overall_health = LGX_HEALTH_GOOD;
    
    // Also initialize monitor->last_status for internal tracking
    memset(&monitor->last_status, 0, sizeof(lgx_health_status_t));
    monitor->last_status.struct_size = sizeof(lgx_health_status_t);
    monitor->last_status.overall_health = LGX_HEALTH_GOOD;
    monitor->last_status.degraded_features = 0;
    
    // Check hardware adapter health
    if (monitor->runtime_state->hardware_adapter) {
        lgx_hardware_status_t hw_status = lgx_hardware_adapter_get_status(
            monitor->runtime_state->hardware_adapter);
        
        monitor->last_status.hardware_tier = hw_status.achieved_tier;
        monitor->last_status.huge_pages_active = hw_status.huge_pages_available;
        monitor->last_status.numa_awareness_active = hw_status.numa_topology_detected;
        monitor->last_status.gpu_responsive = hw_status.gpu_acceleration_available;
        
        // Update overall health based on hardware tier
        if (hw_status.achieved_tier == LGX_HW_TIER_DEGRADED) {
            monitor->last_status.overall_health = LGX_HEALTH_CRITICAL;
            monitor->last_status.degraded_features |= (1 << 0); // Hardware degraded
            monitor->last_status.degradation_summary = "Hardware tier degraded";
        } else if (hw_status.achieved_tier == LGX_HW_TIER_COMPATIBLE) {
            if (monitor->last_status.overall_health == LGX_HEALTH_GOOD) {
                monitor->last_status.overall_health = LGX_HEALTH_WARNING;
            }
            monitor->last_status.degraded_features |= (1 << 1); // Hardware compatible
            monitor->last_status.degradation_summary = "Hardware tier compatible (some features emulated)";
        }
    }
    
    // Check memory manager health
    if (monitor->runtime_state->memory_manager) {
        // Get memory usage from counters
        uint64_t allocations = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
        uint64_t allocation_failures = lgx_get_counter(LGX_COUNTER_POOL_EXHAUSTIONS);
        (void)allocations;  // Suppress unused warning for now
        
        // Estimate memory usage (placeholder - should get from memory manager)
        monitor->last_status.memory_usage_mb = 150;
        monitor->last_status.memory_limit_mb = 16384; // 16GB default
        
        // Calculate allocation failure rate
        monitor->last_status.allocation_failures = (uint32_t)allocation_failures;
        if (allocations > 0) {
            double failure_rate = (double)allocation_failures / (double)allocations;
            monitor->last_status.high_allocation_failure_rate = (failure_rate > 0.01); // >1%
            
            if (failure_rate > 0.05) { // >5%
                monitor->last_status.overall_health = LGX_HEALTH_CRITICAL;
                monitor->last_status.degraded_features |= (1 << 2); // High failure rate
                monitor->last_status.degradation_summary = "High allocation failure rate";
            } else if (failure_rate > 0.01) { // >1%
                if (monitor->last_status.overall_health == LGX_HEALTH_GOOD) {
                    monitor->last_status.overall_health = LGX_HEALTH_WARNING;
                }
                monitor->last_status.degraded_features |= (1 << 3); // Elevated failure rate
            }
        }
        
        // Check memory usage
        double memory_usage_percent = (double)monitor->last_status.memory_usage_mb / 
                                     (double)monitor->last_status.memory_limit_mb;
        monitor->last_status.approaching_memory_limit = (memory_usage_percent > 0.8); // >80%
        
        if (memory_usage_percent > 0.95) { // >95%
            monitor->last_status.overall_health = LGX_HEALTH_CRITICAL;
            monitor->last_status.degraded_features |= (1 << 4); // Memory critical
            monitor->last_status.degradation_summary = "Memory usage critical (>95%)";
        } else if (memory_usage_percent > 0.8) { // >80%
            if (monitor->last_status.overall_health == LGX_HEALTH_GOOD) {
                monitor->last_status.overall_health = LGX_HEALTH_WARNING;
            }
            monitor->last_status.degraded_features |= (1 << 5); // Memory warning
        }
        
        // Get cache hit rate
        uint64_t cache_hits = lgx_get_counter(LGX_COUNTER_CACHE_HITS);
        uint64_t cache_misses = lgx_get_counter(LGX_COUNTER_CACHE_MISSES);
        uint64_t total_accesses = cache_hits + cache_misses;
        if (total_accesses > 0) {
            monitor->last_status.cache_hit_rate_percent = 
                (uint32_t)((cache_hits * 100) / total_accesses);
        }
    }
    
    // Estimate CPU overhead (placeholder - should measure actual overhead)
    monitor->last_status.cpu_overhead_percent = 2.5; // Placeholder
    monitor->last_status.high_cpu_overhead = (monitor->last_status.cpu_overhead_percent > 5.0);
    
    if (monitor->last_status.cpu_overhead_percent > 10.0) {
        monitor->last_status.overall_health = LGX_HEALTH_CRITICAL;
        monitor->last_status.degraded_features |= (1 << 6); // High CPU overhead
        monitor->last_status.degradation_summary = "High CPU overhead (>10%)";
    } else if (monitor->last_status.cpu_overhead_percent > 5.0) {
        if (monitor->last_status.overall_health == LGX_HEALTH_GOOD) {
            monitor->last_status.overall_health = LGX_HEALTH_WARNING;
        }
        monitor->last_status.degraded_features |= (1 << 7); // Elevated CPU overhead
    }
    
    // Set default degradation summary if none set
    if (!monitor->last_status.degradation_summary) {
        if (monitor->last_status.overall_health == LGX_HEALTH_GOOD) {
            monitor->last_status.degradation_summary = "All systems operating normally";
        } else if (monitor->last_status.overall_health == LGX_HEALTH_WARNING) {
            monitor->last_status.degradation_summary = "Some degradation detected";
        }
    }
    
    monitor->last_check_time = lgx_time_now_ns();
    
    // Copy results to caller's struct, but only copy the fields that exist in their struct
    // This ensures ABI compatibility with older struct versions
    memcpy(status, &monitor->last_status, caller_struct_size);
    status->struct_size = caller_struct_size;  // Preserve caller's struct size
    
    pthread_mutex_unlock(&monitor->mutex);
    return LGX_SUCCESS;
}

/**
 * Get last health status
 */
lgx_health_status_t lgx_health_monitor_get_status(lgx_health_monitor_t* monitor) {
    lgx_health_status_t status;
    
    if (!monitor) {
        status.struct_size = sizeof(lgx_health_status_t);
        status.overall_health = LGX_HEALTH_CRITICAL;
        status.memory_usage_mb = 0;
        status.cpu_overhead_percent = 0.0;
        status.degraded_features = 0xFFFFFFFF;
        return status;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    status = monitor->last_status;
    pthread_mutex_unlock(&monitor->mutex);
    
    return status;
}

/**
 * Monitoring thread function (Task 14.3.3)
 */
static void* health_monitoring_thread(void* arg) {
    lgx_health_monitor_t* monitor = (lgx_health_monitor_t*)arg;
    
    while (monitor->monitoring_thread_running) {
        // Perform health check
        lgx_health_status_t status;
        lgx_result_t result = lgx_health_monitor_check(monitor, &status);
        
        if (result == LGX_SUCCESS) {
            pthread_mutex_lock(&monitor->mutex);
            monitor->total_checks++;
            
            // Check if we should trigger an alert
            bool should_alert = false;
            
            if (status.overall_health == LGX_HEALTH_WARNING) {
                monitor->warnings_triggered++;
                should_alert = true;
            } else if (status.overall_health == LGX_HEALTH_CRITICAL) {
                monitor->criticals_triggered++;
                should_alert = true;
            }
            
            // Call alert callback if registered and alert triggered
            if (should_alert && monitor->alert_callback) {
                lgx_health_alert_callback_t callback = monitor->alert_callback;
                void* user_data = monitor->alert_callback_user_data;
                pthread_mutex_unlock(&monitor->mutex);
                
                // Call callback outside of lock to avoid deadlock
                callback(&status, user_data);
            } else {
                pthread_mutex_unlock(&monitor->mutex);
            }
        }
        
        // Sleep for the configured interval
        struct timespec sleep_time;
        sleep_time.tv_sec = monitor->monitoring_interval_ms / 1000;
        sleep_time.tv_nsec = (monitor->monitoring_interval_ms % 1000) * 1000000;
        nanosleep(&sleep_time, NULL);
    }
    
    return NULL;
}

/**
 * Start continuous health monitoring (Task 14.3.3)
 */
lgx_result_t lgx_health_monitor_start_monitoring(lgx_health_monitor_t* monitor, uint32_t interval_ms) {
    if (!monitor) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (interval_ms == 0) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    if (monitor->monitoring_enabled) {
        pthread_mutex_unlock(&monitor->mutex);
        return LGX_ERROR_ALREADY_INITIALIZED;
    }
    
    monitor->monitoring_interval_ms = interval_ms;
    monitor->monitoring_enabled = true;
    monitor->monitoring_thread_running = true;
    
    // Create monitoring thread
    int result = pthread_create(&monitor->monitoring_thread, NULL, 
                               health_monitoring_thread, monitor);
    
    if (result != 0) {
        monitor->monitoring_enabled = false;
        monitor->monitoring_thread_running = false;
        pthread_mutex_unlock(&monitor->mutex);
        return LGX_ERROR_SYSTEM_ERROR;
    }
    
    pthread_mutex_unlock(&monitor->mutex);
    return LGX_SUCCESS;
}

/**
 * Stop continuous health monitoring
 */
lgx_result_t lgx_health_monitor_stop_monitoring(lgx_health_monitor_t* monitor) {
    if (!monitor) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    if (!monitor->monitoring_enabled) {
        pthread_mutex_unlock(&monitor->mutex);
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    // Signal thread to stop
    monitor->monitoring_thread_running = false;
    pthread_mutex_unlock(&monitor->mutex);
    
    // Wait for thread to finish
    pthread_join(monitor->monitoring_thread, NULL);
    
    pthread_mutex_lock(&monitor->mutex);
    monitor->monitoring_enabled = false;
    pthread_mutex_unlock(&monitor->mutex);
    
    return LGX_SUCCESS;
}

/**
 * Check if monitoring is running
 */
bool lgx_health_monitor_is_monitoring_running(lgx_health_monitor_t* monitor) {
    if (!monitor) {
        return false;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    bool running = monitor->monitoring_enabled;
    pthread_mutex_unlock(&monitor->mutex);
    
    return running;
}

/**
 * Set alert callback
 */
void lgx_health_monitor_set_alert_callback(lgx_health_monitor_t* monitor,
                                           lgx_health_alert_callback_t callback,
                                           void* user_data) {
    if (!monitor) {
        return;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    monitor->alert_callback = callback;
    monitor->alert_callback_user_data = user_data;
    pthread_mutex_unlock(&monitor->mutex);
}

/**
 * Set alert thresholds
 */
void lgx_health_monitor_set_thresholds(lgx_health_monitor_t* monitor,
                                       float memory_warning, float memory_critical,
                                       float cpu_warning, float cpu_critical) {
    if (!monitor) {
        return;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    monitor->memory_warning_threshold = memory_warning;
    monitor->memory_critical_threshold = memory_critical;
    monitor->cpu_warning_threshold = cpu_warning;
    monitor->cpu_critical_threshold = cpu_critical;
    pthread_mutex_unlock(&monitor->mutex);
}

/**
 * Get monitoring statistics
 */
lgx_result_t lgx_health_monitor_get_monitoring_stats(lgx_health_monitor_t* monitor,
                                                     uint64_t* total_checks,
                                                     uint64_t* warnings,
                                                     uint64_t* criticals) {
    if (!monitor || !total_checks || !warnings || !criticals) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    *total_checks = monitor->total_checks;
    *warnings = monitor->warnings_triggered;
    *criticals = monitor->criticals_triggered;
    pthread_mutex_unlock(&monitor->mutex);
    
    return LGX_SUCCESS;
}

/**
 * Public API wrappers that use global runtime state
 */

lgx_result_t lgx_health_monitoring_start_public(uint32_t interval_ms) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->health_monitor) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_health_monitor_start_monitoring(runtime->health_monitor, interval_ms);
}

lgx_result_t lgx_health_monitoring_stop_public(void) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->health_monitor) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_health_monitor_stop_monitoring(runtime->health_monitor);
}

bool lgx_health_monitoring_is_running_public(void) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->health_monitor) {
        return false;
    }
    
    return lgx_health_monitor_is_monitoring_running(runtime->health_monitor);
}

void lgx_health_set_alert_callback_public(lgx_health_alert_callback_t callback, void* user_data) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->health_monitor) {
        return;
    }
    
    lgx_health_monitor_set_alert_callback(runtime->health_monitor, callback, user_data);
}

void lgx_health_set_thresholds_public(float memory_warning, float memory_critical,
                                      float cpu_warning, float cpu_critical) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->health_monitor) {
        return;
    }
    
    lgx_health_monitor_set_thresholds(runtime->health_monitor, memory_warning, memory_critical,
                                      cpu_warning, cpu_critical);
}

lgx_result_t lgx_health_get_monitoring_stats_public(uint64_t* total_checks,
                                                    uint64_t* warnings,
                                                    uint64_t* criticals) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->health_monitor) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    return lgx_health_monitor_get_monitoring_stats(runtime->health_monitor, total_checks, 
                                                   warnings, criticals);
}
