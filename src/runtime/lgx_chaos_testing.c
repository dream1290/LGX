/**
 * LGX Chaos Testing Framework
 * 
 * Provides failure injection and chaos testing capabilities for validating
 * runtime resilience under adverse conditions.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

// Chaos testing state
typedef struct {
    pthread_mutex_t mutex;
    bool enabled;
    lgx_chaos_config_t config;
    
    // Statistics
    uint64_t total_operations;
    uint64_t injected_failures;
    uint64_t injected_latency_spikes;
    
    // Random state
    unsigned int random_state;
} chaos_state_t;

static chaos_state_t g_chaos_state = {0};
static bool g_chaos_initialized = false;

/**
 * Initialize chaos testing (internal)
 */
static lgx_result_t chaos_init(void) {
    if (g_chaos_initialized) {
        return LGX_SUCCESS;
    }
    
    if (pthread_mutex_init(&g_chaos_state.mutex, NULL) != 0) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    g_chaos_state.enabled = false;
    g_chaos_state.total_operations = 0;
    g_chaos_state.injected_failures = 0;
    g_chaos_state.injected_latency_spikes = 0;
    
    // Initialize random state
    g_chaos_state.random_state = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    
    g_chaos_initialized = true;
    return LGX_SUCCESS;
}

/**
 * Thread-safe random number generator
 */
static double chaos_random(void) {
    pthread_mutex_lock(&g_chaos_state.mutex);
    int r = rand_r(&g_chaos_state.random_state);
    pthread_mutex_unlock(&g_chaos_state.mutex);
    return (double)r / (double)RAND_MAX;
}

/**
 * Enable chaos testing
 */
lgx_result_t lgx_runtime_enable_chaos_testing(const lgx_chaos_config_t* config) {
    if (!config) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Initialize if needed
    if (!g_chaos_initialized) {
        lgx_result_t result = chaos_init();
        if (result != LGX_SUCCESS) {
            return result;
        }
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    
    // Copy configuration
    g_chaos_state.config = *config;
    g_chaos_state.config.struct_size = sizeof(lgx_chaos_config_t);
    
    // Set random seed if provided
    if (config->random_seed != 0) {
        g_chaos_state.random_state = config->random_seed;
    }
    
    // Reset statistics
    g_chaos_state.total_operations = 0;
    g_chaos_state.injected_failures = 0;
    g_chaos_state.injected_latency_spikes = 0;
    
    g_chaos_state.enabled = true;
    
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_WARN, 
                  "Chaos testing ENABLED - failure_rate=%.2f%%, latency_spike_rate=%.2f%%",
                  config->failure_rate * 100.0, config->latency_spike_rate * 100.0);
    
    return LGX_SUCCESS;
}

/**
 * Disable chaos testing
 */
lgx_result_t lgx_runtime_disable_chaos_testing(void) {
    if (!g_chaos_initialized) {
        return LGX_SUCCESS;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    
    if (g_chaos_state.enabled) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                      "Chaos testing DISABLED - injected %lu failures, %lu latency spikes in %lu operations",
                      g_chaos_state.injected_failures,
                      g_chaos_state.injected_latency_spikes,
                      g_chaos_state.total_operations);
    }
    
    g_chaos_state.enabled = false;
    
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    return LGX_SUCCESS;
}

/**
 * Check if chaos testing is enabled
 */
bool lgx_runtime_is_chaos_testing_enabled(void) {
    if (!g_chaos_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    bool enabled = g_chaos_state.enabled;
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    return enabled;
}

/**
 * Get chaos configuration
 */
lgx_chaos_config_t lgx_runtime_get_chaos_config(void) {
    lgx_chaos_config_t config = {0};
    
    if (!g_chaos_initialized) {
        config.struct_size = sizeof(lgx_chaos_config_t);
        return config;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    config = g_chaos_state.config;
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    return config;
}

/**
 * Should inject allocation failure?
 */
bool lgx_chaos_should_fail_allocation(void) {
    if (!g_chaos_initialized || !g_chaos_state.enabled) {
        return false;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    
    if (!g_chaos_state.config.inject_memory_pressure) {
        pthread_mutex_unlock(&g_chaos_state.mutex);
        return false;
    }
    
    g_chaos_state.total_operations++;
    
    double r = chaos_random();
    bool should_fail = (r < g_chaos_state.config.failure_rate);
    
    if (should_fail) {
        g_chaos_state.injected_failures++;
        lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_DEBUG,
                      "Chaos: Injecting allocation failure (%lu/%lu)",
                      g_chaos_state.injected_failures,
                      g_chaos_state.total_operations);
    }
    
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    return should_fail;
}

/**
 * Should inject latency spike?
 */
bool lgx_chaos_should_inject_latency(uint64_t* latency_ns) {
    if (!g_chaos_initialized || !g_chaos_state.enabled) {
        return false;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    
    if (!g_chaos_state.config.inject_latency_spikes) {
        pthread_mutex_unlock(&g_chaos_state.mutex);
        return false;
    }
    
    double r = chaos_random();
    bool should_inject = (r < g_chaos_state.config.latency_spike_rate);
    
    if (should_inject) {
        // Random latency between 0 and max_latency_spike_ns
        double latency_factor = chaos_random();
        *latency_ns = (uint64_t)(latency_factor * g_chaos_state.config.max_latency_spike_ns);
        
        g_chaos_state.injected_latency_spikes++;
        
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_DEBUG,
                      "Chaos: Injecting latency spike of %lu ns (%lu/%lu)",
                      *latency_ns,
                      g_chaos_state.injected_latency_spikes,
                      g_chaos_state.total_operations);
    }
    
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    return should_inject;
}

/**
 * Inject latency spike (sleep)
 */
void lgx_chaos_inject_latency(void) {
    uint64_t latency_ns = 0;
    
    if (lgx_chaos_should_inject_latency(&latency_ns)) {
        struct timespec ts;
        ts.tv_sec = latency_ns / 1000000000ULL;
        ts.tv_nsec = latency_ns % 1000000000ULL;
        nanosleep(&ts, NULL);
    }
}

/**
 * Should inject GPU hang?
 */
bool lgx_chaos_should_hang_gpu(void) {
    if (!g_chaos_initialized || !g_chaos_state.enabled) {
        return false;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    
    if (!g_chaos_state.config.inject_gpu_hangs) {
        pthread_mutex_unlock(&g_chaos_state.mutex);
        return false;
    }
    
    double r = chaos_random();
    bool should_hang = (r < g_chaos_state.config.gpu_hang_rate);
    
    if (should_hang) {
        lgx_log_tagged(LGX_SUBSYSTEM_GPU, LGX_LOG_WARN,
                      "Chaos: Simulating GPU hang");
    }
    
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    return should_hang;
}

/**
 * Should inject I/O error?
 */
bool lgx_chaos_should_fail_io(void) {
    if (!g_chaos_initialized || !g_chaos_state.enabled) {
        return false;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    
    if (!g_chaos_state.config.inject_io_errors) {
        pthread_mutex_unlock(&g_chaos_state.mutex);
        return false;
    }
    
    double r = chaos_random();
    bool should_fail = (r < g_chaos_state.config.io_error_rate);
    
    if (should_fail) {
        lgx_log_tagged(LGX_SUBSYSTEM_FILESYSTEM, LGX_LOG_DEBUG,
                      "Chaos: Injecting I/O error");
    }
    
    pthread_mutex_unlock(&g_chaos_state.mutex);
    
    return should_fail;
}

/**
 * Get chaos testing statistics
 */
void lgx_chaos_get_stats(uint64_t* total_ops, uint64_t* failures, uint64_t* latency_spikes) {
    if (!g_chaos_initialized) {
        if (total_ops) *total_ops = 0;
        if (failures) *failures = 0;
        if (latency_spikes) *latency_spikes = 0;
        return;
    }
    
    pthread_mutex_lock(&g_chaos_state.mutex);
    
    if (total_ops) *total_ops = g_chaos_state.total_operations;
    if (failures) *failures = g_chaos_state.injected_failures;
    if (latency_spikes) *latency_spikes = g_chaos_state.injected_latency_spikes;
    
    pthread_mutex_unlock(&g_chaos_state.mutex);
}
