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

// Telemetry state
struct lgx_telemetry {
    pthread_mutex_t mutex;
    bool enabled;
    bool user_consent;
    lgx_observability_level_t observability_level;
    
    // Telemetry data
    uint64_t frame_count;
    double total_frame_time;
    double max_frame_time;
    
    size_t memory_usage_samples;
    size_t total_memory_usage;
    size_t peak_memory_usage;
    
    uint64_t allocation_count;
    uint64_t crash_count;
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
    
    // Initialize counters
    tel->frame_count = 0;
    tel->total_frame_time = 0.0;
    tel->max_frame_time = 0.0;
    tel->memory_usage_samples = 0;
    tel->total_memory_usage = 0;
    tel->peak_memory_usage = 0;
    tel->allocation_count = 0;
    tel->crash_count = 0;
    
    *telemetry = tel;
    return LGX_SUCCESS;
}

/**
 * Shutdown telemetry
 */
lgx_result_t lgx_telemetry_shutdown(lgx_telemetry_t* telemetry) {
    if (!telemetry) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
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
    
    pthread_mutex_unlock(&telemetry->mutex);
    return LGX_SUCCESS;
}

/**
 * Record frame time
 */
lgx_result_t lgx_telemetry_record_frame_time(lgx_telemetry_t* telemetry, float frame_time_ms) {
    if (!telemetry) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&telemetry->mutex);
    
    if (telemetry->enabled) {
        telemetry->frame_count++;
        telemetry->total_frame_time += frame_time_ms;
        
        if (frame_time_ms > telemetry->max_frame_time) {
            telemetry->max_frame_time = frame_time_ms;
        }
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
    }
    
    pthread_mutex_unlock(&telemetry->mutex);
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
