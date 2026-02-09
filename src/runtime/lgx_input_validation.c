/**
 * LGX Input Validation - Security Hardening
 * 
 * Provides comprehensive input validation for all API functions.
 * Prevents security vulnerabilities from invalid inputs.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

// Validation limits
#define LGX_MAX_STRING_LENGTH       4096
#define LGX_MAX_PATH_LENGTH         PATH_MAX
#define LGX_MAX_ALLOCATION_SIZE     (16ULL * 1024 * 1024 * 1024)  // 16 GB
#define LGX_MIN_ALIGNMENT           1
#define LGX_MAX_ALIGNMENT           (1024 * 1024)  // 1 MB
#define LGX_MAX_BUFFER_SIZE         (1ULL * 1024 * 1024 * 1024)   // 1 GB

/**
 * Validate pointer is not NULL
 */
bool lgx_validate_pointer(const void* ptr, const char* param_name) {
    if (ptr == NULL) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "NULL pointer for parameter: %s", param_name);
        return false;
    }
    return true;
}

/**
 * Validate size is within bounds
 */
bool lgx_validate_size(size_t size, size_t min_size, size_t max_size, 
                       const char* param_name) {
    if (size < min_size) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Size %zu below minimum %zu for parameter: %s",
                      size, min_size, param_name);
        return false;
    }
    
    if (size > max_size) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Size %zu exceeds maximum %zu for parameter: %s",
                      size, max_size, param_name);
        return false;
    }
    
    return true;
}

/**
 * Validate allocation size
 */
bool lgx_validate_allocation_size(size_t size) {
    if (size == 0) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Allocation size cannot be zero");
        return false;
    }
    
    if (size > LGX_MAX_ALLOCATION_SIZE) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Allocation size %zu exceeds maximum %zu",
                      size, LGX_MAX_ALLOCATION_SIZE);
        return false;
    }
    
    return true;
}

/**
 * Validate alignment is power of 2
 */
bool lgx_validate_alignment(size_t alignment) {
    if (alignment == 0) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Alignment cannot be zero");
        return false;
    }
    
    if (alignment < LGX_MIN_ALIGNMENT) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Alignment %zu below minimum %zu",
                      alignment, LGX_MIN_ALIGNMENT);
        return false;
    }
    
    if (alignment > LGX_MAX_ALIGNMENT) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Alignment %zu exceeds maximum %zu",
                      alignment, LGX_MAX_ALIGNMENT);
        return false;
    }
    
    // Check if power of 2
    if ((alignment & (alignment - 1)) != 0) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Alignment %zu is not a power of 2", alignment);
        return false;
    }
    
    return true;
}

/**
 * Validate string is not NULL and within length limits
 */
bool lgx_validate_string(const char* str, size_t max_length, const char* param_name) {
    if (!lgx_validate_pointer(str, param_name)) {
        return false;
    }
    
    // Check for null terminator within max_length
    size_t len = strnlen(str, max_length + 1);
    if (len > max_length) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "String length exceeds maximum %zu for parameter: %s",
                      max_length, param_name);
        return false;
    }
    
    return true;
}

/**
 * Validate and truncate string if needed
 */
bool lgx_validate_and_truncate_string(const char* src, char* dst, size_t dst_size,
                                     const char* param_name) {
    if (!lgx_validate_pointer(src, param_name)) {
        return false;
    }
    
    if (!lgx_validate_pointer(dst, "dst")) {
        return false;
    }
    
    if (dst_size == 0) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                     "Destination buffer size cannot be zero");
        return false;
    }
    
    // Copy with truncation
    size_t src_len = strnlen(src, dst_size);
    if (src_len >= dst_size) {
        // Truncate
        memcpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
        
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_WARN,
                      "String truncated from %zu to %zu bytes for parameter: %s",
                      src_len, dst_size - 1, param_name);
    } else {
        // Copy normally
        memcpy(dst, src, src_len + 1);  // Include null terminator
    }
    
    return true;
}

/**
 * Validate path string
 */
bool lgx_validate_path(const char* path, const char* param_name) {
    if (!lgx_validate_string(path, LGX_MAX_PATH_LENGTH, param_name)) {
        return false;
    }
    
    // Check for path traversal attempts
    if (strstr(path, "..") != NULL) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                     "Path contains '..' (path traversal attempt): %s", param_name);
        return false;
    }
    
    // Check for null bytes in path (security issue)
    size_t path_len = strlen(path);
    for (size_t i = 0; i < path_len; i++) {
        if (path[i] == '\0') {
            lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                         "Path contains null byte at position %zu: %s", i, param_name);
            return false;
        }
    }
    
    return true;
}

/**
 * Validate enum value is within range
 */
bool lgx_validate_enum(int value, int min_value, int max_value, const char* param_name) {
    if (value < min_value || value > max_value) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                     "Enum value %d out of range [%d, %d] for parameter: %s",
                     value, min_value, max_value, param_name);
        return false;
    }
    return true;
}

/**
 * Validate buffer and size
 */
bool lgx_validate_buffer(const void* buffer, size_t size, const char* param_name) {
    if (!lgx_validate_pointer(buffer, param_name)) {
        return false;
    }
    
    if (!lgx_validate_size(size, 1, LGX_MAX_BUFFER_SIZE, "size")) {
        return false;
    }
    
    return true;
}

/**
 * Validate struct size for forward compatibility
 */
bool lgx_validate_struct_size(size_t provided_size, size_t expected_size,
                              const char* struct_name) {
    if (provided_size < expected_size) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                     "Struct size %zu below expected %zu for: %s",
                     provided_size, expected_size, struct_name);
        return false;
    }
    
    // Allow larger sizes for forward compatibility
    if (provided_size > expected_size) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_DEBUG,
                      "Struct size %zu larger than expected %zu for: %s (forward compat)",
                      provided_size, expected_size, struct_name);
    }
    
    return true;
}

/**
 * Validate capability enum
 */
bool lgx_validate_capability(lgx_capability_t cap) {
    return lgx_validate_enum((int)cap, 0, LGX_CAP_MESH_SHADERS, "capability");
}

/**
 * Validate log level enum
 */
bool lgx_validate_log_level(lgx_log_level_t level) {
    return lgx_validate_enum((int)level, LGX_LOG_DEBUG, LGX_LOG_ERROR, "log_level");
}

/**
 * Validate access pattern enum
 */
bool lgx_validate_access_pattern(lgx_access_pattern_t pattern) {
    return lgx_validate_enum((int)pattern, LGX_ACCESS_SEQUENTIAL, LGX_ACCESS_UNKNOWN,
                            "access_pattern");
}

/**
 * Validate lifetime enum
 */
bool lgx_validate_lifetime(lgx_lifetime_t lifetime) {
    return lgx_validate_enum((int)lifetime, LGX_LIFETIME_FRAME, LGX_LIFETIME_UNKNOWN,
                            "lifetime");
}

/**
 * Validate performance hint enum
 */
bool lgx_validate_performance_hint(lgx_performance_hint_t hint) {
    return lgx_validate_enum((int)hint, LGX_HINT_CRITICAL_PATH, LGX_HINT_GPU_SHARED,
                            "performance_hint");
}

/**
 * Validate allocation intent structure
 */
bool lgx_validate_allocation_intent(const lgx_allocation_intent_base_t* intent) {
    if (!lgx_validate_pointer(intent, "intent")) {
        return false;
    }
    
    // Validate struct size
    if (!lgx_validate_struct_size(intent->struct_size, 
                                  sizeof(lgx_allocation_intent_base_t),
                                  "lgx_allocation_intent_base_t")) {
        return false;
    }
    
    // Validate size
    if (!lgx_validate_allocation_size(intent->size)) {
        return false;
    }
    
    // Validate enums
    if (!lgx_validate_access_pattern(intent->access_pattern)) {
        return false;
    }
    
    if (!lgx_validate_lifetime(intent->lifetime)) {
        return false;
    }
    
    if (!lgx_validate_performance_hint(intent->hint)) {
        return false;
    }
    
    return true;
}

/**
 * Validate runtime config
 */
bool lgx_validate_runtime_config(const lgx_runtime_config_t* config) {
    if (!lgx_validate_pointer(config, "config")) {
        return false;
    }
    
    // Validate memory pool size
    if (config->memory_pool_size > 0) {
        if (!lgx_validate_size(config->memory_pool_size, 
                              1024 * 1024,  // 1 MB minimum
                              16ULL * 1024 * 1024 * 1024,  // 16 GB maximum
                              "memory_pool_size")) {
            return false;
        }
    }
    
    // Validate log path if provided
    if (config->log_path != NULL) {
        if (!lgx_validate_path(config->log_path, "log_path")) {
            return false;
        }
    }
    
    return true;
}

/**
 * Get validation limits (for diagnostics)
 */
void lgx_validation_get_limits(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }
    
    snprintf(buffer, buffer_size,
             "Input Validation Limits:\n"
             "  Max string length: %d bytes\n"
             "  Max path length: %d bytes\n"
             "  Max allocation size: %llu bytes (%.2f GB)\n"
             "  Min alignment: %d bytes\n"
             "  Max alignment: %d bytes\n"
             "  Max buffer size: %llu bytes (%.2f GB)\n",
             LGX_MAX_STRING_LENGTH,
             LGX_MAX_PATH_LENGTH,
             (unsigned long long)LGX_MAX_ALLOCATION_SIZE,
             LGX_MAX_ALLOCATION_SIZE / (1024.0 * 1024.0 * 1024.0),
             LGX_MIN_ALIGNMENT,
             LGX_MAX_ALIGNMENT,
             (unsigned long long)LGX_MAX_BUFFER_SIZE,
             LGX_MAX_BUFFER_SIZE / (1024.0 * 1024.0 * 1024.0));
}
