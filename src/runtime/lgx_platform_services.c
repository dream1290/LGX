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
#include <errno.h>

// Default log file size limit: 100MB
#define LGX_LOG_MAX_SIZE (100 * 1024 * 1024)
#define LGX_LOG_MAX_ROTATIONS 5

// Platform services state
struct lgx_platform_services {
    pthread_mutex_t log_mutex;
    FILE* log_file;
    lgx_log_level_t min_log_level;
    uint32_t subsystem_filter;  // Bitmask of enabled subsystems (0 = all enabled)
    
    // Configuration
    const char* log_path;
    size_t log_max_size;        // Maximum log file size before rotation
    size_t log_current_size;    // Current log file size
    bool log_rotation_enabled;  // Whether log rotation is enabled
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
    ps->subsystem_filter = 0;  // 0 = all subsystems enabled
    ps->log_file = NULL;
    ps->log_max_size = LGX_LOG_MAX_SIZE;
    ps->log_current_size = 0;
    ps->log_rotation_enabled = true;
    
    // Open log file if specified
    if (ps->log_path) {
        ps->log_file = fopen(ps->log_path, "a");
        // If file open fails, continue with stderr logging
        if (ps->log_file) {
            // Get current file size
            fseek(ps->log_file, 0, SEEK_END);
            ps->log_current_size = ftell(ps->log_file);
        }
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
    
    // Chaos testing: inject I/O error
    if (lgx_chaos_should_fail_io()) {
        return -1;
    }
    
    // Chaos testing: inject latency spike
    lgx_chaos_inject_latency();
    
    return open(path, flags);
}

ssize_t lgx_platform_services_fs_read(lgx_platform_services_t* services,
                                     int fd, void* buffer, size_t size) {
    if (!services || fd < 0 || !buffer) {
        return -1;
    }
    
    // Chaos testing: inject I/O error
    if (lgx_chaos_should_fail_io()) {
        return -1;
    }
    
    // Chaos testing: inject latency spike
    lgx_chaos_inject_latency();
    
    return read(fd, buffer, size);
}

ssize_t lgx_platform_services_fs_write(lgx_platform_services_t* services,
                                      int fd, const void* buffer, size_t size) {
    if (!services || fd < 0 || !buffer) {
        return -1;
    }
    
    // Chaos testing: inject I/O error
    if (lgx_chaos_should_fail_io()) {
        return -1;
    }
    
    // Chaos testing: inject latency spike
    lgx_chaos_inject_latency();
    
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

/**
 * Rotate log file when size limit is reached
 */
static void rotate_log_file(lgx_platform_services_t* services) {
    if (!services || !services->log_path || !services->log_rotation_enabled) {
        return;
    }
    
    // Close current log file
    if (services->log_file) {
        fclose(services->log_file);
        services->log_file = NULL;
    }
    
    // Rotate existing log files
    // log.4 -> deleted
    // log.3 -> log.4
    // log.2 -> log.3
    // log.1 -> log.2
    // log -> log.1
    char old_path[512];
    char new_path[512];
    
    // Delete oldest log file
    snprintf(old_path, sizeof(old_path), "%s.%d", services->log_path, LGX_LOG_MAX_ROTATIONS - 1);
    unlink(old_path);  // Ignore errors
    
    // Rotate log files
    for (int i = LGX_LOG_MAX_ROTATIONS - 2; i >= 1; i--) {
        snprintf(old_path, sizeof(old_path), "%s.%d", services->log_path, i);
        snprintf(new_path, sizeof(new_path), "%s.%d", services->log_path, i + 1);
        rename(old_path, new_path);  // Ignore errors
    }
    
    // Rename current log to .1
    snprintf(new_path, sizeof(new_path), "%s.1", services->log_path);
    rename(services->log_path, new_path);  // Ignore errors
    
    // Open new log file
    services->log_file = fopen(services->log_path, "w");
    services->log_current_size = 0;
}

void lgx_log_impl(lgx_platform_services_t* services, lgx_log_level_t level, 
                  const char* format, va_list args) {
    if (!services || level < services->min_log_level) {
        return;
    }
    
    pthread_mutex_lock(&services->log_mutex);
    
    // Check if log rotation is needed
    if (services->log_file && services->log_rotation_enabled && 
        services->log_current_size >= services->log_max_size) {
        rotate_log_file(services);
    }
    
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
    int written = fprintf(output, "[%s.%03ld] [%s] ", 
            timestamp, ts.tv_nsec / 1000000, log_level_to_string(level));
    
    // Format message
    char message[4096];
    vsnprintf(message, sizeof(message), format, args);
    written += fprintf(output, "%s\n", message);
    
    fflush(output);
    
    // Update log file size
    if (services->log_file && written > 0) {
        services->log_current_size += written;
    }
    
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

/**
 * Subsystem name mapping
 */
const char* lgx_subsystem_to_string(lgx_log_subsystem_t subsystem) {
    switch (subsystem) {
        case LGX_SUBSYSTEM_CORE:       return "CORE";
        case LGX_SUBSYSTEM_MEMORY:     return "MEMORY";
        case LGX_SUBSYSTEM_GPU:        return "GPU";
        case LGX_SUBSYSTEM_FILESYSTEM: return "FS";
        case LGX_SUBSYSTEM_TELEMETRY:  return "TELEMETRY";
        case LGX_SUBSYSTEM_LIFECYCLE:  return "LIFECYCLE";
        case LGX_SUBSYSTEM_HARDWARE:   return "HARDWARE";
        case LGX_SUBSYSTEM_SECURITY:   return "SECURITY";
        default: return "UNKNOWN";
    }
}

/**
 * Structured logging with subsystem filtering
 */
void lgx_log_tagged(lgx_log_subsystem_t subsystem, lgx_log_level_t level, 
                    const char* format, ...) {
    if (!g_platform_services) {
        return;
    }
    
    // Check log level filter
    if (level < g_platform_services->min_log_level) {
        return;
    }
    
    // Check subsystem filter (0 = all enabled, otherwise check bitmask)
    if (g_platform_services->subsystem_filter != 0) {
        uint32_t subsystem_bit = (1U << subsystem);
        if ((g_platform_services->subsystem_filter & subsystem_bit) == 0) {
            return;  // Subsystem filtered out
        }
    }
    
    pthread_mutex_lock(&g_platform_services->log_mutex);
    
    // Check if log rotation is needed
    if (g_platform_services->log_file && g_platform_services->log_rotation_enabled && 
        g_platform_services->log_current_size >= g_platform_services->log_max_size) {
        rotate_log_file(g_platform_services);
    }
    
    // Get current time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Format timestamp
    char timestamp[32];
    struct tm* tm_info = localtime(&ts.tv_sec);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Choose output stream
    FILE* output = g_platform_services->log_file ? g_platform_services->log_file : stderr;
    
    // Write log entry with subsystem tag
    int written = fprintf(output, "[%s.%03ld] [%s] [%s] ", 
            timestamp, ts.tv_nsec / 1000000, 
            log_level_to_string(level),
            lgx_subsystem_to_string(subsystem));
    
    // Format message
    char message[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    written += fprintf(output, "%s\n", message);
    fflush(output);
    
    // Update log file size
    if (g_platform_services->log_file && written > 0) {
        g_platform_services->log_current_size += written;
    }
    
    pthread_mutex_unlock(&g_platform_services->log_mutex);
}

/**
 * Set subsystem filter
 */
void lgx_set_subsystem_filter(uint32_t subsystem_mask) {
    if (g_platform_services) {
        g_platform_services->subsystem_filter = subsystem_mask;
    }
}

/**
 * Get subsystem filter
 */
uint32_t lgx_get_subsystem_filter(void) {
    if (g_platform_services) {
        return g_platform_services->subsystem_filter;
    }
    return 0;
}

/**
 * Set log file maximum size
 */
void lgx_set_log_max_size(size_t max_size_bytes) {
    if (g_platform_services) {
        pthread_mutex_lock(&g_platform_services->log_mutex);
        g_platform_services->log_max_size = max_size_bytes;
        pthread_mutex_unlock(&g_platform_services->log_mutex);
    }
}

/**
 * Enable or disable log rotation
 */
void lgx_set_log_rotation_enabled(bool enabled) {
    if (g_platform_services) {
        pthread_mutex_lock(&g_platform_services->log_mutex);
        g_platform_services->log_rotation_enabled = enabled;
        pthread_mutex_unlock(&g_platform_services->log_mutex);
    }
}

/**
 * Get current log file size
 */
size_t lgx_get_log_current_size(void) {
    if (g_platform_services) {
        pthread_mutex_lock(&g_platform_services->log_mutex);
        size_t size = g_platform_services->log_current_size;
        pthread_mutex_unlock(&g_platform_services->log_mutex);
        return size;
    }
    return 0;
}

/**
 * Filesystem API Implementation
 */

// File handle structure
struct lgx_file {
    FILE* fp;
    char path[256];
    bool is_open;
};

/**
 * Open a file
 */
lgx_result_t lgx_fs_open(const char* path, const char* mode, lgx_file_t** file) {
    if (!path || !mode || !file) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Chaos testing: inject I/O error
    if (lgx_chaos_should_fail_io()) {
        return LGX_ERROR_IO_ERROR;
    }
    
    // Allocate file handle
    lgx_file_t* f = calloc(1, sizeof(lgx_file_t));
    if (!f) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Open file
    f->fp = fopen(path, mode);
    if (!f->fp) {
        free(f);
        return LGX_ERROR_IO_ERROR;
    }
    
    // Store path and mark as open
    strncpy(f->path, path, sizeof(f->path) - 1);
    f->path[sizeof(f->path) - 1] = '\0';
    f->is_open = true;
    
    *file = f;
    return LGX_SUCCESS;
}

/**
 * Read from a file
 */
lgx_result_t lgx_fs_read(lgx_file_t* file, void* buffer, size_t size, size_t* bytes_read) {
    if (!file || !buffer || !bytes_read) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!file->is_open || !file->fp) {
        return LGX_ERROR_INVALID_STATE;
    }
    
    // Chaos testing: inject I/O error
    if (lgx_chaos_should_fail_io()) {
        return LGX_ERROR_IO_ERROR;
    }
    
    // Read from file
    size_t read = fread(buffer, 1, size, file->fp);
    *bytes_read = read;
    
    // Check for errors
    if (read < size && ferror(file->fp)) {
        return LGX_ERROR_IO_ERROR;
    }
    
    return LGX_SUCCESS;
}

/**
 * Write to a file
 */
lgx_result_t lgx_fs_write(lgx_file_t* file, const void* buffer, size_t size) {
    if (!file || !buffer) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!file->is_open || !file->fp) {
        return LGX_ERROR_INVALID_STATE;
    }
    
    // Chaos testing: inject I/O error
    if (lgx_chaos_should_fail_io()) {
        return LGX_ERROR_IO_ERROR;
    }
    
    // Write to file
    size_t written = fwrite(buffer, 1, size, file->fp);
    
    // Check for errors
    if (written < size) {
        return LGX_ERROR_IO_ERROR;
    }
    
    // Flush to ensure data is written
    fflush(file->fp);
    
    return LGX_SUCCESS;
}

/**
 * Close a file
 */
lgx_result_t lgx_fs_close(lgx_file_t* file) {
    if (!file) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!file->is_open) {
        return LGX_ERROR_INVALID_STATE;
    }
    
    // Close file
    if (file->fp) {
        fclose(file->fp);
        file->fp = NULL;
    }
    
    file->is_open = false;
    free(file);
    
    return LGX_SUCCESS;
}
