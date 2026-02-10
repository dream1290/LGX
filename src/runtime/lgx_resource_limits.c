/**
 * LGX Runtime Core - Resource Limits
 * 
 * Implements resource limits for production hardening:
 * - Maximum memory limit (16GB default)
 * - Maximum file handles (1024 default)
 * - Log file size limits with rotation
 * - Allocation rate limiting
 */

#define _GNU_SOURCE
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <errno.h>
#include <stdatomic.h>

// Default limits
#define DEFAULT_MAX_MEMORY_BYTES (16ULL * 1024 * 1024 * 1024)  // 16GB
#define DEFAULT_MAX_FILE_HANDLES 1024
#define DEFAULT_MAX_LOG_SIZE_BYTES (100 * 1024 * 1024)  // 100MB
#define DEFAULT_MAX_ALLOCATIONS_PER_SEC (1000000)  // 1M/sec

// Rate limiting window
#define RATE_LIMIT_WINDOW_NS (1000000000)  // 1 second in nanoseconds

// Resource limits state
typedef struct {
    bool initialized;
    pthread_mutex_t mutex;
    
    // Memory limits
    size_t max_memory_bytes;
    atomic_size_t current_memory_bytes;
    atomic_size_t peak_memory_bytes;
    
    // File handle limits
    size_t max_file_handles;
    atomic_size_t current_file_handles;
    
    // Log file limits
    size_t max_log_size_bytes;
    atomic_size_t current_log_size_bytes;
    char log_file_path[256];
    int log_rotation_count;
    
    // Allocation rate limiting
    size_t max_allocations_per_sec;
    atomic_uint_fast64_t allocation_count;
    atomic_uint_fast64_t last_reset_time_ns;
    atomic_uint_fast64_t rate_limit_violations;
    
    // Statistics
    atomic_uint_fast64_t memory_limit_hits;
    atomic_uint_fast64_t file_handle_limit_hits;
    atomic_uint_fast64_t log_rotation_count_total;
} lgx_resource_limits_t;

static lgx_resource_limits_t g_limits = {
    .initialized = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

// Get current time in nanoseconds
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Initialize resource limits
 */
lgx_result_t lgx_resource_limits_init(const lgx_resource_limits_config_t* config) {
    pthread_mutex_lock(&g_limits.mutex);
    
    if (g_limits.initialized) {
        pthread_mutex_unlock(&g_limits.mutex);
        return LGX_ERROR_ALREADY_INITIALIZED;
    }
    
    // Set limits from config or use defaults
    g_limits.max_memory_bytes = config && config->max_memory_bytes > 0 
        ? config->max_memory_bytes 
        : DEFAULT_MAX_MEMORY_BYTES;
    
    g_limits.max_file_handles = config && config->max_file_handles > 0
        ? config->max_file_handles
        : DEFAULT_MAX_FILE_HANDLES;
    
    g_limits.max_log_size_bytes = config && config->max_log_size_bytes > 0
        ? config->max_log_size_bytes
        : DEFAULT_MAX_LOG_SIZE_BYTES;
    
    g_limits.max_allocations_per_sec = config && config->max_allocations_per_sec > 0
        ? config->max_allocations_per_sec
        : DEFAULT_MAX_ALLOCATIONS_PER_SEC;
    
    // Initialize atomic counters
    atomic_store(&g_limits.current_memory_bytes, 0);
    atomic_store(&g_limits.peak_memory_bytes, 0);
    atomic_store(&g_limits.current_file_handles, 0);
    atomic_store(&g_limits.current_log_size_bytes, 0);
    atomic_store(&g_limits.allocation_count, 0);
    atomic_store(&g_limits.last_reset_time_ns, get_time_ns());
    atomic_store(&g_limits.rate_limit_violations, 0);
    atomic_store(&g_limits.memory_limit_hits, 0);
    atomic_store(&g_limits.file_handle_limit_hits, 0);
    atomic_store(&g_limits.log_rotation_count_total, 0);
    
    // Set log file path
    if (config && config->log_file_path) {
        strncpy(g_limits.log_file_path, config->log_file_path, sizeof(g_limits.log_file_path) - 1);
        g_limits.log_file_path[sizeof(g_limits.log_file_path) - 1] = '\0';
    } else {
        strncpy(g_limits.log_file_path, "/tmp/lgx_runtime.log", sizeof(g_limits.log_file_path) - 1);
    }
    
    g_limits.log_rotation_count = 0;
    
    // Set system resource limits (rlimit) - only if reasonable and we have permission
    // Note: Very restrictive limits are not set at system level to avoid
    // breaking the runtime itself. We still track them at application level.
    struct rlimit rlim;
    
    // Set memory limit (RLIMIT_AS - address space) - only if > 100MB
    // Skip when running under AddressSanitizer as it conflicts with ASAN's memory management
#ifndef __SANITIZE_ADDRESS__
    if (g_limits.max_memory_bytes > 100 * 1024 * 1024) {
        rlim.rlim_cur = g_limits.max_memory_bytes;
        rlim.rlim_max = g_limits.max_memory_bytes;
        if (setrlimit(RLIMIT_AS, &rlim) != 0) {
            // Non-fatal, just log warning
            fprintf(stderr, "Warning: Failed to set RLIMIT_AS: %s\n", strerror(errno));
        }
    }
#endif
    
    // Set file descriptor limit (RLIMIT_NOFILE) - only if > 256
    // (lower values can break the runtime with AddressSanitizer)
    if (g_limits.max_file_handles > 256) {
        rlim.rlim_cur = g_limits.max_file_handles;
        rlim.rlim_max = g_limits.max_file_handles;
        if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
            // Non-fatal, just log warning
            fprintf(stderr, "Warning: Failed to set RLIMIT_NOFILE: %s\n", strerror(errno));
        }
    }
    
    g_limits.initialized = true;
    
    pthread_mutex_unlock(&g_limits.mutex);
    return LGX_SUCCESS;
}

/**
 * Shutdown resource limits
 */
lgx_result_t lgx_resource_limits_shutdown(void) {
    pthread_mutex_lock(&g_limits.mutex);
    
    if (!g_limits.initialized) {
        pthread_mutex_unlock(&g_limits.mutex);
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    g_limits.initialized = false;
    
    pthread_mutex_unlock(&g_limits.mutex);
    return LGX_SUCCESS;
}

/**
 * Check if memory allocation is within limits
 */
bool lgx_resource_limits_check_memory(size_t size) {
    if (!g_limits.initialized) {
        return true;  // No limits enforced if not initialized
    }
    
    size_t current = atomic_load(&g_limits.current_memory_bytes);
    size_t new_total = current + size;
    
    if (new_total > g_limits.max_memory_bytes) {
        atomic_fetch_add(&g_limits.memory_limit_hits, 1);
        return false;
    }
    
    return true;
}

/**
 * Track memory allocation
 */
void lgx_resource_limits_track_allocation(size_t size) {
    if (!g_limits.initialized) {
        return;
    }
    
    size_t new_total = atomic_fetch_add(&g_limits.current_memory_bytes, size) + size;
    
    // Update peak
    size_t current_peak = atomic_load(&g_limits.peak_memory_bytes);
    while (new_total > current_peak) {
        if (atomic_compare_exchange_weak(&g_limits.peak_memory_bytes, &current_peak, new_total)) {
            break;
        }
    }
}

/**
 * Track memory deallocation
 */
void lgx_resource_limits_track_deallocation(size_t size) {
    if (!g_limits.initialized) {
        return;
    }
    
    atomic_fetch_sub(&g_limits.current_memory_bytes, size);
}

/**
 * Check allocation rate limit
 */
bool lgx_resource_limits_check_allocation_rate(void) {
    if (!g_limits.initialized) {
        return true;
    }
    
    uint64_t current_time = get_time_ns();
    uint64_t last_reset = atomic_load(&g_limits.last_reset_time_ns);
    
    // Check if we need to reset the window
    if (current_time - last_reset >= RATE_LIMIT_WINDOW_NS) {
        // Try to reset the window
        if (atomic_compare_exchange_strong(&g_limits.last_reset_time_ns, &last_reset, current_time)) {
            atomic_store(&g_limits.allocation_count, 0);
        }
    }
    
    // Increment allocation count
    uint64_t count = atomic_fetch_add(&g_limits.allocation_count, 1) + 1;
    
    if (count > g_limits.max_allocations_per_sec) {
        atomic_fetch_add(&g_limits.rate_limit_violations, 1);
        return false;
    }
    
    return true;
}

/**
 * Check if file handle is within limits
 */
bool lgx_resource_limits_check_file_handle(void) {
    if (!g_limits.initialized) {
        return true;
    }
    
    size_t current = atomic_load(&g_limits.current_file_handles);
    
    if (current >= g_limits.max_file_handles) {
        atomic_fetch_add(&g_limits.file_handle_limit_hits, 1);
        return false;
    }
    
    return true;
}

/**
 * Track file handle open
 */
void lgx_resource_limits_track_file_open(void) {
    if (!g_limits.initialized) {
        return;
    }
    
    atomic_fetch_add(&g_limits.current_file_handles, 1);
}

/**
 * Track file handle close
 */
void lgx_resource_limits_track_file_close(void) {
    if (!g_limits.initialized) {
        return;
    }
    
    atomic_fetch_sub(&g_limits.current_file_handles, 1);
}

/**
 * Check if log file needs rotation
 */
bool lgx_resource_limits_check_log_rotation(size_t bytes_to_write) {
    if (!g_limits.initialized) {
        return false;
    }
    
    size_t current = atomic_load(&g_limits.current_log_size_bytes);
    return (current + bytes_to_write) > g_limits.max_log_size_bytes;
}

/**
 * Rotate log file
 */
lgx_result_t lgx_resource_limits_rotate_log(void) {
    if (!g_limits.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_limits.mutex);
    
    // Generate rotated log file name
    char rotated_path[512];
    snprintf(rotated_path, sizeof(rotated_path), "%s.%d", 
             g_limits.log_file_path, g_limits.log_rotation_count);
    
    // Rename current log file
    if (rename(g_limits.log_file_path, rotated_path) != 0) {
        pthread_mutex_unlock(&g_limits.mutex);
        return LGX_ERROR_IO_ERROR;
    }
    
    // Reset log size counter
    atomic_store(&g_limits.current_log_size_bytes, 0);
    
    // Increment rotation count
    g_limits.log_rotation_count++;
    atomic_fetch_add(&g_limits.log_rotation_count_total, 1);
    
    // Keep only last 5 rotated logs
    if (g_limits.log_rotation_count > 5) {
        char old_path[512];
        snprintf(old_path, sizeof(old_path), "%s.%d", 
                 g_limits.log_file_path, g_limits.log_rotation_count - 5);
        unlink(old_path);  // Ignore errors
    }
    
    pthread_mutex_unlock(&g_limits.mutex);
    return LGX_SUCCESS;
}

/**
 * Track log write
 */
void lgx_resource_limits_track_log_write(size_t bytes) {
    if (!g_limits.initialized) {
        return;
    }
    
    atomic_fetch_add(&g_limits.current_log_size_bytes, bytes);
}

/**
 * Get resource limits statistics
 */
lgx_result_t lgx_resource_limits_get_stats(lgx_resource_limits_stats_t* stats) {
    if (!stats) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_limits.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    stats->struct_size = sizeof(lgx_resource_limits_stats_t);
    
    // Memory stats
    stats->current_memory_bytes = atomic_load(&g_limits.current_memory_bytes);
    stats->peak_memory_bytes = atomic_load(&g_limits.peak_memory_bytes);
    stats->max_memory_bytes = g_limits.max_memory_bytes;
    stats->memory_limit_hits = atomic_load(&g_limits.memory_limit_hits);
    
    // File handle stats
    stats->current_file_handles = atomic_load(&g_limits.current_file_handles);
    stats->max_file_handles = g_limits.max_file_handles;
    stats->file_handle_limit_hits = atomic_load(&g_limits.file_handle_limit_hits);
    
    // Log stats
    stats->current_log_size_bytes = atomic_load(&g_limits.current_log_size_bytes);
    stats->max_log_size_bytes = g_limits.max_log_size_bytes;
    stats->log_rotation_count = atomic_load(&g_limits.log_rotation_count_total);
    
    // Rate limiting stats
    stats->allocation_count = atomic_load(&g_limits.allocation_count);
    stats->max_allocations_per_sec = g_limits.max_allocations_per_sec;
    stats->rate_limit_violations = atomic_load(&g_limits.rate_limit_violations);
    
    return LGX_SUCCESS;
}

/**
 * Reset resource limits statistics
 */
lgx_result_t lgx_resource_limits_reset_stats(void) {
    if (!g_limits.initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    atomic_store(&g_limits.peak_memory_bytes, atomic_load(&g_limits.current_memory_bytes));
    atomic_store(&g_limits.memory_limit_hits, 0);
    atomic_store(&g_limits.file_handle_limit_hits, 0);
    atomic_store(&g_limits.rate_limit_violations, 0);
    
    return LGX_SUCCESS;
}
