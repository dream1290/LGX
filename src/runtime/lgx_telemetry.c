/**
 * LGX Telemetry - Phase 1 Implementation
 * 
 * Collects and exports telemetry data with privacy guarantees.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

// Ring buffer for telemetry events
#define TELEMETRY_RING_BUFFER_SIZE 10000

typedef struct {
    uint64_t timestamp_ns;
    enum {
        TEL_EVENT_FRAME_TIME,
        TEL_EVENT_MEMORY_USAGE,
        TEL_EVENT_ALLOCATION,
        TEL_EVENT_FRAME_SPIKE,
        TEL_EVENT_ALLOCATION_FAILURE
    } type;
    union {
        struct { float frame_time_ms; } frame;
        struct { size_t memory_mb; } memory;
        struct { size_t size; } allocation;
        struct { float spike_ms; const char* cause; } spike;
        struct { size_t requested_size; } failure;
    } data;
} telemetry_event_t;

// Telemetry state
struct lgx_telemetry {
    pthread_mutex_t mutex;
    bool enabled;
    bool user_consent;
    lgx_observability_level_t observability_level;
    
    // Privacy policy
    lgx_privacy_policy_t privacy_policy;
    
    // Adaptive sampling
    bool adaptive_sampling;
    double current_sample_rate;
    double min_sample_rate;
    size_t ring_buffer_size;
    lgx_telemetry_overflow_t overflow_policy;
    
    // Ring buffer for events
    telemetry_event_t* ring_buffer;
    size_t ring_buffer_head;
    size_t ring_buffer_tail;
    size_t ring_buffer_count;
    uint64_t dropped_events;
    
    // Telemetry data (aggregated)
    uint64_t frame_count;
    double total_frame_time;
    double max_frame_time;
    double min_frame_time;
    
    size_t memory_usage_samples;
    size_t total_memory_usage;
    size_t peak_memory_usage;
    
    uint64_t allocation_count;
    uint64_t crash_count;
    
    // Correlation tracking
    uint64_t frame_spike_count;
    uint64_t allocation_burst_count;
};

/**
 * Initialize telemetry
 */
lgx_result_t lgx_telemetry_init(lgx_telemetry_t** telemetry,
                               const lgx_runtime_config_t* config) {
    if (!telemetry || !config) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_telemetry_t* tel = calloc(1, sizeof(lgx_telemetry_t));
    if (!tel) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    if (pthread_mutex_init(&tel->mutex, NULL) != 0) {
        free(tel);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    tel->enabled = false;
    tel->user_consent = false;
    tel->observability_level = LGX_OBS_NORMAL;  // Default to normal
    
    // Initialize privacy policy (default: privacy-first)
    tel->privacy_policy.struct_size = sizeof(lgx_privacy_policy_t);
    tel->privacy_policy.collect_frame_times = true;
    tel->privacy_policy.collect_allocation_sizes = true;
    tel->privacy_policy.collect_cpu_model = false;  // Disabled by default (fingerprinting risk)
    tel->privacy_policy.collect_gpu_model = false;  // Disabled by default (fingerprinting risk)
    tel->privacy_policy.collect_kernel_version = false;  // Disabled by default
    tel->privacy_policy.add_noise = false;  // No noise by default
    tel->privacy_policy.noise_stddev = 0.05;  // 5% noise if enabled
    tel->privacy_policy.aggregate_only = true;  // Only aggregates by default
    
    // Initialize adaptive sampling
    tel->adaptive_sampling = true;
    tel->current_sample_rate = 1.0;  // 100% initially
    tel->min_sample_rate = 0.01;  // 1% minimum
    tel->ring_buffer_size = TELEMETRY_RING_BUFFER_SIZE;
    tel->overflow_policy = LGX_TEL_SAMPLE;  // Adaptive sampling on overflow
    
    // Allocate ring buffer
    tel->ring_buffer = calloc(tel->ring_buffer_size, sizeof(telemetry_event_t));
    if (!tel->ring_buffer) {
        pthread_mutex_destroy(&tel->mutex);
        free(tel);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    tel->ring_buffer_head = 0;
    tel->ring_buffer_tail = 0;
    tel->ring_buffer_count = 0;
    tel->dropped_events = 0;
    
    // Initialize counters
    tel->frame_count = 0;
    tel->total_frame_time = 0.0;
    tel->max_frame_time = 0.0;
    tel->min_frame_time = 1000000.0;  // Large initial value
    tel->memory_usage_samples = 0;
    tel->total_memory_usage = 0;
    tel->peak_memory_usage = 0;
    tel->allocation_count = 0;
    tel->crash_count = 0;
    tel->frame_spike_count = 0;
    tel->allocation_burst_count = 0;
    
    *telemetry = tel;
    
    lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                  "Telemetry initialized with privacy-first defaults");
    
    return LGX_SUCCESS;
}

/**
 * Shutdown telemetry
 */
lgx_result_t lgx_telemetry_shutdown(lgx_telemetry_t* telemetry) {
    if (!telemetry) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&telemetry->mutex);
    
    // Log final statistics
    if (telemetry->enabled) {
        lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                      "Telemetry shutdown: %lu frames, %lu events, %lu dropped",
                      telemetry->frame_count,
                      telemetry->ring_buffer_count,
                      telemetry->dropped_events);
    }
    
    // Free ring buffer
    if (telemetry->ring_buffer) {
        free(telemetry->ring_buffer);
    }
    
    pthread_mutex_unlock(&telemetry->mutex);
    pthread_mutex_destroy(&telemetry->mutex);
    free(telemetry);
    return LGX_SUCCESS;
}

/**
 * Enable telemetry with user consent
 */
lgx_result_t lgx_telemetry_enable(lgx_telemetry_t* telemetry, bool user_consent) {
    if (!telemetry) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&telemetry->mutex);
    
    telemetry->user_consent = user_consent;
    telemetry->enabled = user_consent;
    
    if (user_consent) {
        lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                      "Telemetry ENABLED with user consent");
        lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                      "Privacy policy: frame_times=%d, allocations=%d, cpu_model=%d, gpu_model=%d",
                      telemetry->privacy_policy.collect_frame_times,
                      telemetry->privacy_policy.collect_allocation_sizes,
                      telemetry->privacy_policy.collect_cpu_model,
                      telemetry->privacy_policy.collect_gpu_model);
    } else {
        lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                      "Telemetry DISABLED (no user consent)");
    }
    
    pthread_mutex_unlock(&telemetry->mutex);
    return LGX_SUCCESS;
}

/**
 * Configure telemetry with privacy policy
 */
lgx_result_t lgx_telemetry_configure(const lgx_telemetry_config_t* config) {
    if (!config) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->telemetry) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    lgx_telemetry_t* tel = runtime->telemetry;
    
    pthread_mutex_lock(&tel->mutex);
    
    // Update configuration
    tel->enabled = config->enabled;
    tel->ring_buffer_size = config->ring_buffer_size;
    tel->overflow_policy = config->overflow_policy;
    tel->adaptive_sampling = config->adaptive_sampling;
    tel->min_sample_rate = config->min_sample_rate;
    tel->privacy_policy = config->privacy_policy;
    
    lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                  "Telemetry configured: enabled=%d, adaptive_sampling=%d, min_rate=%.2f%%",
                  tel->enabled, tel->adaptive_sampling, tel->min_sample_rate * 100.0);
    
    pthread_mutex_unlock(&tel->mutex);
    return LGX_SUCCESS;
}

/**
 * Get privacy policy (for user transparency)
 */
lgx_privacy_policy_t lgx_telemetry_get_privacy_policy(void) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->telemetry) {
        lgx_privacy_policy_t empty = {0};
        empty.struct_size = sizeof(lgx_privacy_policy_t);
        return empty;
    }
    
    pthread_mutex_lock(&runtime->telemetry->mutex);
    lgx_privacy_policy_t policy = runtime->telemetry->privacy_policy;
    pthread_mutex_unlock(&runtime->telemetry->mutex);
    
    return policy;
}

/**
 * Should sample this event? (Adaptive sampling)
 */
static bool should_sample_event(lgx_telemetry_t* tel) {
    if (!tel->adaptive_sampling) {
        return true;  // Always sample if adaptive sampling disabled
    }
    
    // Check buffer fullness
    double fullness = (double)tel->ring_buffer_count / tel->ring_buffer_size;
    
    // Adjust sample rate based on fullness
    if (fullness > 0.90) {
        // Buffer >90% full: reduce to minimum sample rate
        tel->current_sample_rate = tel->min_sample_rate;
    } else if (fullness > 0.75) {
        // Buffer >75% full: reduce to 10%
        tel->current_sample_rate = 0.10;
    } else if (fullness > 0.50) {
        // Buffer >50% full: reduce to 50%
        tel->current_sample_rate = 0.50;
    } else {
        // Buffer <50% full: sample everything
        tel->current_sample_rate = 1.0;
    }
    
    // Random sampling based on current rate
    double r = (double)rand() / RAND_MAX;
    return r < tel->current_sample_rate;
}

/**
 * Add event to ring buffer
 */
static lgx_result_t add_telemetry_event(lgx_telemetry_t* tel, const telemetry_event_t* event) {
    // Check if we should sample this event
    if (!should_sample_event(tel)) {
        tel->dropped_events++;
        return LGX_SUCCESS;  // Not an error, just dropped for sampling
    }
    
    // Check buffer fullness
    if (tel->ring_buffer_count >= tel->ring_buffer_size) {
        // Buffer full: handle overflow
        switch (tel->overflow_policy) {
            case LGX_TEL_DROP_OLDEST:
                // Drop oldest event (ring buffer behavior)
                tel->ring_buffer_tail = (tel->ring_buffer_tail + 1) % tel->ring_buffer_size;
                tel->ring_buffer_count--;
                tel->dropped_events++;
                break;
                
            case LGX_TEL_DROP_NEWEST:
                // Drop this event
                tel->dropped_events++;
                return LGX_SUCCESS;
                
            case LGX_TEL_SAMPLE:
                // Already handled by should_sample_event()
                tel->dropped_events++;
                return LGX_SUCCESS;
        }
    }
    
    // Add event to buffer
    tel->ring_buffer[tel->ring_buffer_head] = *event;
    tel->ring_buffer_head = (tel->ring_buffer_head + 1) % tel->ring_buffer_size;
    tel->ring_buffer_count++;
    
    return LGX_SUCCESS;
}

/**
 * Record frame time with spike detection
 */
lgx_result_t lgx_telemetry_record_frame_time(lgx_telemetry_t* telemetry, float frame_time_ms) {
    if (!telemetry) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&telemetry->mutex);
    
    if (telemetry->enabled && telemetry->privacy_policy.collect_frame_times) {
        telemetry->frame_count++;
        telemetry->total_frame_time += frame_time_ms;
        
        if (frame_time_ms > telemetry->max_frame_time) {
            telemetry->max_frame_time = frame_time_ms;
        }
        
        if (frame_time_ms < telemetry->min_frame_time) {
            telemetry->min_frame_time = frame_time_ms;
        }
        
        // Detect frame spike (>2x average)
        if (telemetry->frame_count > 100) {  // Need baseline
            double avg_frame_time = telemetry->total_frame_time / telemetry->frame_count;
            if (frame_time_ms > avg_frame_time * 2.0) {
                // Frame spike detected
                telemetry->frame_spike_count++;
                
                // Add spike event to ring buffer
                telemetry_event_t event = {
                    .timestamp_ns = lgx_time_now_ns(),
                    .type = TEL_EVENT_FRAME_SPIKE,
                    .data.spike = { .spike_ms = frame_time_ms, .cause = "unknown" }
                };
                add_telemetry_event(telemetry, &event);
                
                lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_DEBUG,
                              "Frame spike detected: %.2f ms (avg: %.2f ms)",
                              frame_time_ms, avg_frame_time);
            }
        }
        
        // Add frame time event
        telemetry_event_t event = {
            .timestamp_ns = lgx_time_now_ns(),
            .type = TEL_EVENT_FRAME_TIME,
            .data.frame = { .frame_time_ms = frame_time_ms }
        };
        add_telemetry_event(telemetry, &event);
    }
    
    pthread_mutex_unlock(&telemetry->mutex);
    return LGX_SUCCESS;
}

/**
 * Record memory usage
 */
lgx_result_t lgx_telemetry_record_memory_usage(lgx_telemetry_t* telemetry, 
                                              size_t memory_usage_mb, size_t pool_usage_mb) {
    if (!telemetry) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Suppress unused parameter warning
    (void)pool_usage_mb;
    
    pthread_mutex_lock(&telemetry->mutex);
    
    if (telemetry->enabled) {
        telemetry->memory_usage_samples++;
        telemetry->total_memory_usage += memory_usage_mb;
        
        if (memory_usage_mb > telemetry->peak_memory_usage) {
            telemetry->peak_memory_usage = memory_usage_mb;
        }
        
        // Add memory event
        telemetry_event_t event = {
            .timestamp_ns = lgx_time_now_ns(),
            .type = TEL_EVENT_MEMORY_USAGE,
            .data.memory = { .memory_mb = memory_usage_mb }
        };
        add_telemetry_event(telemetry, &event);
    }
    
    pthread_mutex_unlock(&telemetry->mutex);
    return LGX_SUCCESS;
}

/**
 * Record allocation (for correlation analysis)
 */
lgx_result_t lgx_telemetry_record_allocation(size_t size) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->telemetry) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    lgx_telemetry_t* tel = runtime->telemetry;
    
    pthread_mutex_lock(&tel->mutex);
    
    if (tel->enabled && tel->privacy_policy.collect_allocation_sizes) {
        tel->allocation_count++;
        
        // Add allocation event
        telemetry_event_t event = {
            .timestamp_ns = lgx_time_now_ns(),
            .type = TEL_EVENT_ALLOCATION,
            .data.allocation = { .size = size }
        };
        add_telemetry_event(tel, &event);
    }
    
    pthread_mutex_unlock(&tel->mutex);
    return LGX_SUCCESS;
}

/**
 * Record allocation failure (for correlation analysis)
 */
lgx_result_t lgx_telemetry_record_allocation_failure(size_t requested_size) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->telemetry) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    lgx_telemetry_t* tel = runtime->telemetry;
    
    pthread_mutex_lock(&tel->mutex);
    
    if (tel->enabled) {
        // Add failure event
        telemetry_event_t event = {
            .timestamp_ns = lgx_time_now_ns(),
            .type = TEL_EVENT_ALLOCATION_FAILURE,
            .data.failure = { .requested_size = requested_size }
        };
        add_telemetry_event(tel, &event);
        
        lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_DEBUG,
                      "Allocation failure recorded: %zu bytes", requested_size);
    }
    
    pthread_mutex_unlock(&tel->mutex);
    return LGX_SUCCESS;
}

/**
 * Export telemetry data
 */
lgx_result_t lgx_telemetry_export(lgx_telemetry_t* telemetry, const char* output_path) {
    if (!telemetry || !output_path) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&telemetry->mutex);
    
    if (!telemetry->enabled) {
        pthread_mutex_unlock(&telemetry->mutex);
        return LGX_ERROR_NOT_SUPPORTED;
    }
    
    FILE* file = fopen(output_path, "w");
    if (!file) {
        pthread_mutex_unlock(&telemetry->mutex);
        return LGX_ERROR_IO_ERROR;
    }
    
    // Export as JSON
    fprintf(file, "{\n");
    fprintf(file, "  \"lgx_version\": \"1.0.0\",\n");
    fprintf(file, "  \"frame_times\": {\n");
    
    if (telemetry->frame_count > 0) {
        double avg_frame_time = telemetry->total_frame_time / telemetry->frame_count;
        fprintf(file, "    \"average_ms\": %.2f,\n", avg_frame_time);
        fprintf(file, "    \"max_ms\": %.2f,\n", telemetry->max_frame_time);
        fprintf(file, "    \"frame_count\": %lu\n", telemetry->frame_count);
    } else {
        fprintf(file, "    \"average_ms\": 0.0,\n");
        fprintf(file, "    \"max_ms\": 0.0,\n");
        fprintf(file, "    \"frame_count\": 0\n");
    }
    
    fprintf(file, "  },\n");
    fprintf(file, "  \"memory\": {\n");
    
    if (telemetry->memory_usage_samples > 0) {
        size_t avg_memory = telemetry->total_memory_usage / telemetry->memory_usage_samples;
        fprintf(file, "    \"average_mb\": %zu,\n", avg_memory);
        fprintf(file, "    \"peak_mb\": %zu,\n", telemetry->peak_memory_usage);
        fprintf(file, "    \"samples\": %zu\n", telemetry->memory_usage_samples);
    } else {
        fprintf(file, "    \"average_mb\": 0,\n");
        fprintf(file, "    \"peak_mb\": 0,\n");
        fprintf(file, "    \"samples\": 0\n");
    }
    
    fprintf(file, "  },\n");
    fprintf(file, "  \"allocations\": {\n");
    fprintf(file, "    \"total\": %lu\n", telemetry->allocation_count);
    fprintf(file, "  },\n");
    fprintf(file, "  \"crashes\": %lu\n", telemetry->crash_count);
    fprintf(file, "}\n");
    
    fclose(file);
    
    pthread_mutex_unlock(&telemetry->mutex);
    return LGX_SUCCESS;
}

/**
 * Set observability level
 */
void lgx_set_observability_level(lgx_observability_level_t level) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->telemetry) {
        return;
    }
    
    pthread_mutex_lock(&runtime->telemetry->mutex);
    runtime->telemetry->observability_level = level;
    pthread_mutex_unlock(&runtime->telemetry->mutex);
}

/**
 * Get observability level
 */
lgx_observability_level_t lgx_get_observability_level(void) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->telemetry) {
        return LGX_OBS_NONE;
    }
    
    pthread_mutex_lock(&runtime->telemetry->mutex);
    lgx_observability_level_t level = runtime->telemetry->observability_level;
    pthread_mutex_unlock(&runtime->telemetry->mutex);
    
    return level;
}

/**
 * Check if observability level allows operation
 */
bool lgx_observability_allows(lgx_observability_level_t required_level) {
    lgx_observability_level_t current = lgx_get_observability_level();
    return current >= required_level;
}

/**
 * Perform correlation analysis on telemetry data
 * Analyzes relationships between events (e.g., allocation bursts causing frame spikes)
 */
typedef struct {
    const char* correlation_type;
    double confidence;  // 0.0 to 1.0
    const char* description;
    const char* recommendation;
} correlation_result_t;

static void analyze_correlations(lgx_telemetry_t* tel, 
                                 correlation_result_t* results, 
                                 size_t* result_count,
                                 size_t max_results) {
    *result_count = 0;
    
    if (tel->ring_buffer_count < 100) {
        return;  // Not enough data for correlation
    }
    
    // Analyze allocation bursts vs frame spikes
    // Look for allocation events within 16ms window before frame spikes
    size_t allocation_before_spike = 0;
    size_t total_spikes = 0;
    
    for (size_t i = 0; i < tel->ring_buffer_count && i < tel->ring_buffer_size; i++) {
        size_t idx = (tel->ring_buffer_tail + i) % tel->ring_buffer_size;
        telemetry_event_t* event = &tel->ring_buffer[idx];
        
        if (event->type == TEL_EVENT_FRAME_SPIKE) {
            total_spikes++;
            
            // Look back 16ms for allocation bursts
            uint64_t spike_time = event->timestamp_ns;
            uint64_t window_start = spike_time - 16000000;  // 16ms in ns
            
            size_t alloc_count_in_window = 0;
            
            // Scan backwards from spike
            for (size_t j = 0; j < i && j < 100; j++) {
                size_t back_idx = (tel->ring_buffer_tail + i - j - 1) % tel->ring_buffer_size;
                telemetry_event_t* back_event = &tel->ring_buffer[back_idx];
                
                if (back_event->timestamp_ns < window_start) {
                    break;  // Outside window
                }
                
                if (back_event->type == TEL_EVENT_ALLOCATION) {
                    alloc_count_in_window++;
                }
            }
            
            // Threshold: >10 allocations in 16ms window = burst
            if (alloc_count_in_window > 10) {
                allocation_before_spike++;
            }
        }
    }
    
    // Calculate correlation confidence
    if (total_spikes > 0 && *result_count < max_results) {
        double correlation = (double)allocation_before_spike / total_spikes;
        
        if (correlation > 0.5) {  // >50% correlation
            results[*result_count].correlation_type = "allocation_burst_frame_spike";
            results[*result_count].confidence = correlation;
            results[*result_count].description = 
                "Frame-time spikes correlate with allocation bursts";
            results[*result_count].recommendation = 
                "Pre-allocate memory or use frame arena for temporary allocations";
            (*result_count)++;
        }
    }
    
    // Analyze memory growth trends (potential leak detection)
    if (tel->memory_usage_samples > 100 && *result_count < max_results) {
        // Simple linear regression to detect upward trend
        double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
        size_t n = 0;
        
        for (size_t i = 0; i < tel->ring_buffer_count && i < tel->ring_buffer_size; i++) {
            size_t idx = (tel->ring_buffer_tail + i) % tel->ring_buffer_size;
            telemetry_event_t* event = &tel->ring_buffer[idx];
            
            if (event->type == TEL_EVENT_MEMORY_USAGE) {
                double x = (double)n;
                double y = (double)event->data.memory.memory_mb;
                
                sum_x += x;
                sum_y += y;
                sum_xy += x * y;
                sum_x2 += x * x;
                n++;
            }
        }
        
        if (n > 10) {
            // Calculate slope (memory growth rate)
            double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
            
            // If slope > 0.1 MB per sample, potential leak
            if (slope > 0.1) {
                results[*result_count].correlation_type = "memory_growth_trend";
                results[*result_count].confidence = 0.7;  // Medium confidence
                results[*result_count].description = 
                    "Memory usage increasing over time (potential leak)";
                results[*result_count].recommendation = 
                    "Check for memory leaks, enable allocation tracking";
                (*result_count)++;
            }
        }
    }
}

/**
 * Export telemetry data with correlation analysis
 */
lgx_result_t lgx_telemetry_export_collected_data(const char* output_path) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->telemetry) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    if (!output_path) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_telemetry_t* tel = runtime->telemetry;
    
    pthread_mutex_lock(&tel->mutex);
    
    if (!tel->enabled) {
        pthread_mutex_unlock(&tel->mutex);
        return LGX_ERROR_NOT_SUPPORTED;
    }
    
    FILE* file = fopen(output_path, "w");
    if (!file) {
        pthread_mutex_unlock(&tel->mutex);
        return LGX_ERROR_IO_ERROR;
    }
    
    // Perform correlation analysis
    correlation_result_t correlations[10];
    size_t correlation_count = 0;
    analyze_correlations(tel, correlations, &correlation_count, 10);
    
    // Export as JSON with rich context
    fprintf(file, "{\n");
    fprintf(file, "  \"lgx_version\": \"1.0.0\",\n");
    fprintf(file, "  \"export_timestamp\": %lu,\n", lgx_time_now_ns());
    fprintf(file, "  \"user_consent\": %s,\n", tel->user_consent ? "true" : "false");
    
    // Privacy policy
    fprintf(file, "  \"privacy_policy\": {\n");
    fprintf(file, "    \"collect_frame_times\": %s,\n", 
            tel->privacy_policy.collect_frame_times ? "true" : "false");
    fprintf(file, "    \"collect_allocation_sizes\": %s,\n",
            tel->privacy_policy.collect_allocation_sizes ? "true" : "false");
    fprintf(file, "    \"collect_cpu_model\": %s,\n",
            tel->privacy_policy.collect_cpu_model ? "true" : "false");
    fprintf(file, "    \"collect_gpu_model\": %s,\n",
            tel->privacy_policy.collect_gpu_model ? "true" : "false");
    fprintf(file, "    \"aggregate_only\": %s\n",
            tel->privacy_policy.aggregate_only ? "true" : "false");
    fprintf(file, "  },\n");
    
    // Sampling statistics
    fprintf(file, "  \"sampling\": {\n");
    fprintf(file, "    \"adaptive_sampling\": %s,\n",
            tel->adaptive_sampling ? "true" : "false");
    fprintf(file, "    \"current_sample_rate\": %.4f,\n", tel->current_sample_rate);
    fprintf(file, "    \"events_collected\": %zu,\n", tel->ring_buffer_count);
    fprintf(file, "    \"events_dropped\": %lu\n", tel->dropped_events);
    fprintf(file, "  },\n");
    
    // Frame times
    fprintf(file, "  \"frame_times\": {\n");
    if (tel->frame_count > 0) {
        double avg_frame_time = tel->total_frame_time / tel->frame_count;
        fprintf(file, "    \"average_ms\": %.2f,\n", avg_frame_time);
        fprintf(file, "    \"min_ms\": %.2f,\n", tel->min_frame_time);
        fprintf(file, "    \"max_ms\": %.2f,\n", tel->max_frame_time);
        fprintf(file, "    \"frame_count\": %lu,\n", tel->frame_count);
        fprintf(file, "    \"spike_count\": %lu\n", tel->frame_spike_count);
    } else {
        fprintf(file, "    \"average_ms\": 0.0,\n");
        fprintf(file, "    \"min_ms\": 0.0,\n");
        fprintf(file, "    \"max_ms\": 0.0,\n");
        fprintf(file, "    \"frame_count\": 0,\n");
        fprintf(file, "    \"spike_count\": 0\n");
    }
    fprintf(file, "  },\n");
    
    // Memory
    fprintf(file, "  \"memory\": {\n");
    if (tel->memory_usage_samples > 0) {
        size_t avg_memory = tel->total_memory_usage / tel->memory_usage_samples;
        fprintf(file, "    \"average_mb\": %zu,\n", avg_memory);
        fprintf(file, "    \"peak_mb\": %zu,\n", tel->peak_memory_usage);
        fprintf(file, "    \"samples\": %zu\n", tel->memory_usage_samples);
    } else {
        fprintf(file, "    \"average_mb\": 0,\n");
        fprintf(file, "    \"peak_mb\": 0,\n");
        fprintf(file, "    \"samples\": 0\n");
    }
    fprintf(file, "  },\n");
    
    // Allocations
    fprintf(file, "  \"allocations\": {\n");
    fprintf(file, "    \"total\": %lu\n", tel->allocation_count);
    fprintf(file, "  },\n");
    
    // Correlation analysis
    fprintf(file, "  \"correlations\": [\n");
    for (size_t i = 0; i < correlation_count; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"type\": \"%s\",\n", correlations[i].correlation_type);
        fprintf(file, "      \"confidence\": %.2f,\n", correlations[i].confidence);
        fprintf(file, "      \"description\": \"%s\",\n", correlations[i].description);
        fprintf(file, "      \"recommendation\": \"%s\"\n", correlations[i].recommendation);
        fprintf(file, "    }%s\n", (i < correlation_count - 1) ? "," : "");
    }
    fprintf(file, "  ],\n");
    
    // Crashes
    fprintf(file, "  \"crashes\": %lu\n", tel->crash_count);
    fprintf(file, "}\n");
    
    fclose(file);
    
    lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                  "Telemetry data exported to %s (%zu events, %zu correlations)",
                  output_path, tel->ring_buffer_count, correlation_count);
    
    pthread_mutex_unlock(&tel->mutex);
    return LGX_SUCCESS;
}
