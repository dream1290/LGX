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
    hm->last_status.cpu_usage_percent = 0.0;
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
 * Perform health check
 */
lgx_result_t lgx_health_monitor_check(lgx_health_monitor_t* monitor,
                                     lgx_health_status_t* status) {
    if (!monitor || !status) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    // Update health status
    monitor->last_status.struct_size = sizeof(lgx_health_status_t);
    monitor->last_status.overall_health = LGX_HEALTH_GOOD;
    monitor->last_status.degraded_features = 0;
    
    // Check hardware adapter health
    if (monitor->runtime_state->hardware_adapter) {
        lgx_hardware_status_t hw_status = lgx_hardware_adapter_get_status(
            monitor->runtime_state->hardware_adapter);
        
        if (hw_status.achieved_tier == LGX_HW_TIER_DEGRADED) {
            monitor->last_status.overall_health = LGX_HEALTH_CRITICAL;
            monitor->last_status.degraded_features |= (1 << 0); // Hardware degraded
        } else if (hw_status.achieved_tier == LGX_HW_TIER_COMPATIBLE) {
            monitor->last_status.overall_health = LGX_HEALTH_WARNING;
            monitor->last_status.degraded_features |= (1 << 1); // Hardware compatible
        }
    }
    
    // Check memory manager health
    if (monitor->runtime_state->memory_manager) {
        // TODO: Get actual memory usage from memory manager
        monitor->last_status.memory_usage_mb = 150; // Placeholder
    }
    
    // TODO: Check other subsystems
    
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
        status.cpu_usage_percent = 0.0;
        status.degraded_features = 0xFFFFFFFF;
        return status;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    status = monitor->last_status;
    pthread_mutex_unlock(&monitor->mutex);
    
    return status;
}