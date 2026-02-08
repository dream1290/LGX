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
#include <pthread.h>

// Health monitor state
struct lgx_health_monitor {
    pthread_mutex_t mutex;
    lgx_runtime_state_t* runtime_state;
    
    // Health status
    lgx_health_status_t last_status;
    uint64_t last_check_time;
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
    
    pthread_mutex_lock(&monitor->mutex);
    
    // Initialize health status
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
    *status = monitor->last_status;
    
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