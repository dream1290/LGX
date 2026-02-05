/**
 * LGX Platform Services - Phase 1 Implementation
 * 
 * Provides platform abstraction for filesystem, timing, and logging services.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

// Platform services state
struct lgx_platform_services {
    pthread_mutex_t log_mutex;
    FILE* log_file;
    lgx_log_level_t min_log_level;
    
    // Configuration
    const char* log_path;
};

/**
 * Initialize platform services
 */
lgx_result_t lgx_platform_services_init(lgx_platform_services_t** services,
                                       const lgx_runtime_config_t* config) {
    if (!services || !config) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_platform_services_t* ps = calloc(1, sizeof(lgx_platform_services_t));
    if (!ps) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    if (pthread_mutex_init(&ps->log_mutex, NULL) != 0) {
        free(ps);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    ps->log_path = config->log_path;
    ps->min_log_level = LGX_LOG_INFO;
    ps->log_file = NULL;
    
    // Open log file if specified
    if (ps->log_path) {
        ps->log_file = fopen(ps->log_path, "a");
        // If file open fails, continue with stderr logging
    }
    
    *services = ps;
    return LGX_SUCCESS;
}

/**
 * Shutdown platform services
 */
lgx_result_t lgx_platform_services_shutdown(lgx_platform_services_t* services) {
    if (!services) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&services->log_mutex);
    
    if (services->log_file) {
        fclose(services->log_file);
    }
    
    pthread_mutex_unlock(&services->log_mutex);
    pthread_mutex_destroy(&services->log_mutex);
    
    free(services);
    return LGX_SUCCESS;
}

/**
 * Filesystem operations
 */
int lgx_platform_services_fs_open(lgx_platform_services_t* services, 
                                  const char* path, int flags) {
    if (!services || !path) {
        return -1;
    }
    
    return open(path, flags);
}

ssize_t lgx_platform_services_fs_read(lgx_platform_services_t* services,
                                     int fd, void* buffer, size_t size) {
    if (!services || fd < 0 || !buffer) {
        return -1;
    }
    
    return read(fd, buffer, size);
}

ssize_t lgx_platform_services_fs_write(lgx_platform_services_t* services,
                                      int fd, const void* buffer, size_t size) {
    if (!services || fd < 0 || !buffer) {
        return -1;
    }
    
    return write(fd, buffer, size);
}

int lgx_platform_services_fs_close(lgx_platform_services_t* services, int fd) {
    if (!services || fd < 0) {
        return -1;
    }
    
    return close(fd);
}

/**
 * Timing operations
 */
uint64_t lgx_platform_services_time_now_ns(lgx_platform_services_t* services) {
    if (!services) {
        return 0;
    }
    
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

lgx_result_t lgx_platform_services_time_sleep_ms(lgx_platform_services_t* services,
                                                 uint32_t milliseconds) {
    if (!services) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    
    if (nanosleep(&ts, NULL) != 0) {
        return LGX_ERROR_SYSTEM_ERROR;
    }
    
    return LGX_SUCCESS;
}

/**
 * Logging implementation
 */
static const char* log_level_to_string(lgx_log_level_t level) {
    switch (level) {
        case LGX_LOG_DEBUG: return "DEBUG";
        case LGX_LOG_INFO:  return "INFO";
        case LGX_LOG_WARN:  return "WARN";
        case LGX_LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void lgx_log_impl(lgx_platform_services_t* services, lgx_log_level_t level, 
                  const char* format, va_list args) {
    if (!services || level < services->min_log_level) {
        return;
    }
    
    pthread_mutex_lock(&services->log_mutex);
    
    // Get current time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Format timestamp
    char timestamp[32];
    struct tm* tm_info = localtime(&ts.tv_sec);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Choose output stream
    FILE* output = services->log_file ? services->log_file : stderr;
    
    // Write log entry
    fprintf(output, "[%s.%03ld] [%s] ", 
            timestamp, ts.tv_nsec / 1000000, log_level_to_string(level));
    vfprintf(output, format, args);
    fprintf(output, "\n");
    fflush(output);
    
    pthread_mutex_unlock(&services->log_mutex);
}

// Global platform services instance for public API
static lgx_platform_services_t* g_platform_services = NULL;

void lgx_set_platform_services(lgx_platform_services_t* services) {
    g_platform_services = services;
}

/**
 * Public logging API
 */
void lgx_log(lgx_log_level_t level, const char* format, ...) {
    if (!g_platform_services) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    lgx_log_impl(g_platform_services, level, format, args);
    va_end(args);
}

void lgx_set_log_filter(lgx_log_level_t min_level) {
    if (g_platform_services) {
        g_platform_services->min_log_level = min_level;
    }
}