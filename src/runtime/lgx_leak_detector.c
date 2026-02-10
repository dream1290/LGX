/**
 * LGX Memory Leak Detector - Task 14.3.4
 * 
 * Implements anomaly detection for memory leaks using trend analysis.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>

// Configuration
#define LGX_LEAK_DETECTOR_WINDOW_SIZE 60  // Track last 60 samples
#define LGX_LEAK_DETECTOR_GROWTH_THRESHOLD 1.05  // 5% growth per sample is suspicious
#define LGX_LEAK_DETECTOR_SUSTAINED_GROWTH_COUNT 10  // 10 consecutive growths = leak

// Memory sample
typedef struct {
    uint64_t timestamp_ns;
    size_t total_bytes;
    size_t frame_arena_bytes;
    size_t gpu_pool_bytes;
    size_t persistent_heap_bytes;
} lgx_memory_sample_t;

// Leak detector state
typedef struct {
    pthread_mutex_t mutex;
    bool initialized;
    
    // Sample history (circular buffer)
    lgx_memory_sample_t samples[LGX_LEAK_DETECTOR_WINDOW_SIZE];
    int sample_count;
    int sample_index;  // Next write position
    
    // Leak detection state
    bool leak_detected;
    int consecutive_growth_count;
    float growth_rate;  // Average growth rate
    const char* suspected_allocator;  // Which allocator is leaking
    
    // Statistics
    uint64_t total_samples;
    uint64_t leak_alerts;
    
    // Alert callback
    lgx_leak_alert_callback_t alert_callback;
    void* alert_callback_user_data;
} lgx_leak_detector_t;

static lgx_leak_detector_t g_leak_detector = {0};

/**
 * Initialize leak detector
 */
lgx_result_t lgx_leak_detector_init(void) {
    if (g_leak_detector.initialized) {
        return LGX_ERROR_ALREADY_INITIALIZED;
    }
    
    if (pthread_mutex_init(&g_leak_detector.mutex, NULL) != 0) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    memset(&g_leak_detector, 0, sizeof(lgx_leak_detector_t));
    g_leak_detector.initialized = true;
    
    return LGX_SUCCESS;
}

/**
 * Shutdown leak detector
 */
lgx_result_t lgx_leak_detector_shutdown(void) {
    if (!g_leak_detector.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_destroy(&g_leak_detector.mutex);
    g_leak_detector.initialized = false;
    
    return LGX_SUCCESS;
}

/**
 * Add a memory sample for analysis
 */
lgx_result_t lgx_leak_detector_add_sample(size_t total_bytes,
                                          size_t frame_arena_bytes,
                                          size_t gpu_pool_bytes,
                                          size_t persistent_heap_bytes) {
    if (!g_leak_detector.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_leak_detector.mutex);
    
    // Create new sample
    lgx_memory_sample_t sample;
    sample.timestamp_ns = lgx_time_now_ns();
    sample.total_bytes = total_bytes;
    sample.frame_arena_bytes = frame_arena_bytes;
    sample.gpu_pool_bytes = gpu_pool_bytes;
    sample.persistent_heap_bytes = persistent_heap_bytes;
    
    // Add to circular buffer
    g_leak_detector.samples[g_leak_detector.sample_index] = sample;
    g_leak_detector.sample_index = (g_leak_detector.sample_index + 1) % LGX_LEAK_DETECTOR_WINDOW_SIZE;
    
    if (g_leak_detector.sample_count < LGX_LEAK_DETECTOR_WINDOW_SIZE) {
        g_leak_detector.sample_count++;
    }
    
    g_leak_detector.total_samples++;
    
    // Analyze for leaks if we have enough samples
    if (g_leak_detector.sample_count >= 2) {
        // Get previous sample
        int prev_index = (g_leak_detector.sample_index - 2 + LGX_LEAK_DETECTOR_WINDOW_SIZE) 
                        % LGX_LEAK_DETECTOR_WINDOW_SIZE;
        lgx_memory_sample_t* prev = &g_leak_detector.samples[prev_index];
        
        // Check for growth
        if (sample.total_bytes > prev->total_bytes) {
            float growth = (float)sample.total_bytes / (float)prev->total_bytes;
            
            if (growth >= LGX_LEAK_DETECTOR_GROWTH_THRESHOLD) {
                g_leak_detector.consecutive_growth_count++;
                g_leak_detector.growth_rate = growth;
                
                // Identify which allocator is growing
                size_t frame_growth = sample.frame_arena_bytes - prev->frame_arena_bytes;
                size_t gpu_growth = sample.gpu_pool_bytes - prev->gpu_pool_bytes;
                size_t heap_growth = sample.persistent_heap_bytes - prev->persistent_heap_bytes;
                
                if (frame_growth > gpu_growth && frame_growth > heap_growth) {
                    g_leak_detector.suspected_allocator = "frame_arena";
                } else if (gpu_growth > heap_growth) {
                    g_leak_detector.suspected_allocator = "gpu_pool";
                } else {
                    g_leak_detector.suspected_allocator = "persistent_heap";
                }
                
                // Check if we've detected a leak
                if (g_leak_detector.consecutive_growth_count >= LGX_LEAK_DETECTOR_SUSTAINED_GROWTH_COUNT) {
                    if (!g_leak_detector.leak_detected) {
                        g_leak_detector.leak_detected = true;
                        g_leak_detector.leak_alerts++;
                        
                        // Trigger alert callback if registered
                        if (g_leak_detector.alert_callback) {
                            lgx_leak_alert_callback_t callback = g_leak_detector.alert_callback;
                            void* user_data = g_leak_detector.alert_callback_user_data;
                            
                            lgx_leak_alert_t alert;
                            alert.growth_rate = g_leak_detector.growth_rate;
                            alert.suspected_allocator = g_leak_detector.suspected_allocator;
                            alert.current_bytes = sample.total_bytes;
                            alert.consecutive_growth_count = g_leak_detector.consecutive_growth_count;
                            
                            pthread_mutex_unlock(&g_leak_detector.mutex);
                            callback(&alert, user_data);
                            pthread_mutex_lock(&g_leak_detector.mutex);
                        }
                    }
                }
            } else {
                // Growth is normal, reset counter
                g_leak_detector.consecutive_growth_count = 0;
                g_leak_detector.leak_detected = false;
            }
        } else {
            // Memory decreased or stayed same, reset counter
            g_leak_detector.consecutive_growth_count = 0;
            g_leak_detector.leak_detected = false;
        }
    }
    
    pthread_mutex_unlock(&g_leak_detector.mutex);
    return LGX_SUCCESS;
}

/**
 * Check if a leak is currently detected
 */
bool lgx_leak_detector_is_leak_detected(void) {
    if (!g_leak_detector.initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_leak_detector.mutex);
    bool detected = g_leak_detector.leak_detected;
    pthread_mutex_unlock(&g_leak_detector.mutex);
    
    return detected;
}

/**
 * Get leak detection statistics
 */
lgx_result_t lgx_leak_detector_get_stats(lgx_leak_detector_stats_t* stats) {
    if (!stats) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_leak_detector.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_leak_detector.mutex);
    
    stats->struct_size = sizeof(lgx_leak_detector_stats_t);
    stats->total_samples = g_leak_detector.total_samples;
    stats->leak_alerts = g_leak_detector.leak_alerts;
    stats->leak_detected = g_leak_detector.leak_detected;
    stats->consecutive_growth_count = g_leak_detector.consecutive_growth_count;
    stats->growth_rate = g_leak_detector.growth_rate;
    stats->suspected_allocator = g_leak_detector.suspected_allocator;
    
    // Get current memory usage from latest sample
    if (g_leak_detector.sample_count > 0) {
        int latest_index = (g_leak_detector.sample_index - 1 + LGX_LEAK_DETECTOR_WINDOW_SIZE) 
                          % LGX_LEAK_DETECTOR_WINDOW_SIZE;
        lgx_memory_sample_t* latest = &g_leak_detector.samples[latest_index];
        
        stats->current_total_bytes = latest->total_bytes;
        stats->current_frame_arena_bytes = latest->frame_arena_bytes;
        stats->current_gpu_pool_bytes = latest->gpu_pool_bytes;
        stats->current_persistent_heap_bytes = latest->persistent_heap_bytes;
    } else {
        stats->current_total_bytes = 0;
        stats->current_frame_arena_bytes = 0;
        stats->current_gpu_pool_bytes = 0;
        stats->current_persistent_heap_bytes = 0;
    }
    
    pthread_mutex_unlock(&g_leak_detector.mutex);
    return LGX_SUCCESS;
}

/**
 * Set leak alert callback
 */
void lgx_leak_detector_set_alert_callback(lgx_leak_alert_callback_t callback, void* user_data) {
    if (!g_leak_detector.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_leak_detector.mutex);
    g_leak_detector.alert_callback = callback;
    g_leak_detector.alert_callback_user_data = user_data;
    pthread_mutex_unlock(&g_leak_detector.mutex);
}

/**
 * Reset leak detection state
 */
lgx_result_t lgx_leak_detector_reset(void) {
    if (!g_leak_detector.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_leak_detector.mutex);
    
    g_leak_detector.sample_count = 0;
    g_leak_detector.sample_index = 0;
    g_leak_detector.leak_detected = false;
    g_leak_detector.consecutive_growth_count = 0;
    g_leak_detector.growth_rate = 0.0f;
    g_leak_detector.suspected_allocator = NULL;
    
    pthread_mutex_unlock(&g_leak_detector.mutex);
    return LGX_SUCCESS;
}
