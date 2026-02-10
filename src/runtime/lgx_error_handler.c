/**
 * LGX Error Handler - Phase 1 Implementation
 * 
 * Manages error reporting and recovery guidance.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

// Thread-local error context
static __thread lgx_error_context_ex_t g_last_error = {0};
static __thread bool g_error_set = false;

// Global error callback (works even before runtime init)
static lgx_error_callback_t g_global_callback = NULL;
static void* g_global_callback_user_data = NULL;
static pthread_mutex_t g_global_callback_mutex = PTHREAD_MUTEX_INITIALIZER;

// Error metadata mapping
typedef struct {
    lgx_result_t code;
    lgx_error_severity_t severity;
    lgx_recovery_action_t recovery;
    const char* recovery_steps;
    bool recoverable;
} lgx_error_metadata_t;

static const lgx_error_metadata_t g_error_metadata[] = {
    {
        LGX_SUCCESS,
        LGX_SEV_WARNING,
        LGX_RECOVER_RETRY,
        "No error occurred",
        true
    },
    {
        LGX_ERROR_INVALID_PARAM,
        LGX_SEV_ERROR,
        LGX_RECOVER_ABORT,
        "Check function parameters and ensure they are valid. Verify pointers are non-NULL and values are in valid ranges.",
        true
    },
    {
        LGX_ERROR_NOT_INITIALIZED,
        LGX_SEV_ERROR,
        LGX_RECOVER_RETRY,
        "Call lgx_runtime_init() before using any other runtime functions.",
        true
    },
    {
        LGX_ERROR_ALREADY_INITIALIZED,
        LGX_SEV_WARNING,
        LGX_RECOVER_RETRY,
        "Runtime is already initialized. Call lgx_runtime_shutdown() first if you need to reinitialize.",
        true
    },
    {
        LGX_ERROR_INCOMPATIBLE_VERSION,
        LGX_SEV_FATAL,
        LGX_RECOVER_SHUTDOWN,
        "Runtime version is incompatible with application. Update runtime library or rebuild application with matching headers.",
        false
    },
    {
        LGX_ERROR_OUT_OF_MEMORY,
        LGX_SEV_ERROR,
        LGX_RECOVER_DEGRADE,
        "System is out of memory. Free unused allocations, reduce memory pool sizes, or enable graceful degradation mode.",
        true
    },
    {
        LGX_ERROR_IO_ERROR,
        LGX_SEV_ERROR,
        LGX_RECOVER_RETRY,
        "I/O operation failed. Check file permissions, disk space, and filesystem health. Retry operation after resolving issues.",
        true
    },
    {
        LGX_ERROR_NOT_SUPPORTED,
        LGX_SEV_WARNING,
        LGX_RECOVER_DEGRADE,
        "Feature not supported on this hardware/platform. Runtime will use software fallback if available.",
        true
    },
    {
        LGX_ERROR_LIBRARY_VERSION_MISMATCH,
        LGX_SEV_FATAL,
        LGX_RECOVER_SHUTDOWN,
        "Dependent library version mismatch detected. Update system libraries or use pinned library versions.",
        false
    },
    {
        LGX_ERROR_GPU_UNAVAILABLE,
        LGX_SEV_WARNING,
        LGX_RECOVER_DEGRADE,
        "GPU acceleration unavailable. Runtime will use CPU fallback. Check GPU drivers and Vulkan installation.",
        true
    },
    {
        LGX_ERROR_RESOURCE_LIMIT_EXCEEDED,
        LGX_SEV_ERROR,
        LGX_RECOVER_DEGRADE,
        "Resource limit exceeded. Reduce allocation rate, increase pool sizes, or enable resource throttling.",
        true
    },
    {
        LGX_ERROR_HARDWARE_DEGRADED,
        LGX_SEV_WARNING,
        LGX_RECOVER_DEGRADE,
        "Hardware features degraded. Enable huge pages, configure NUMA, or update GPU drivers for optimal performance.",
        true
    },
    {
        LGX_ERROR_INTENT_VALIDATION_FAILED,
        LGX_SEV_WARNING,
        LGX_RECOVER_RETRY,
        "Allocation intent validation failed. Review allocation patterns and adjust intent hints for better performance.",
        true
    },
    {
        LGX_ERROR_SYSTEM_ERROR,
        LGX_SEV_ERROR,
        LGX_RECOVER_RETRY,
        "System error occurred. Check system logs (dmesg, journalctl) for details. May require system administrator intervention.",
        true
    },
    {
        LGX_ERROR_INVALID_STATE,
        LGX_SEV_ERROR,
        LGX_RECOVER_ABORT,
        "Runtime is in invalid state for this operation. Ensure proper initialization and operation sequencing.",
        true
    },
    {
        LGX_ERROR_TIMEOUT,
        LGX_SEV_ERROR,
        LGX_RECOVER_RETRY,
        "Operation timed out. Retry with longer timeout, reduce system load, or check for deadlocks.",
        true
    },
    {
        LGX_ERROR_NOT_FOUND,
        LGX_SEV_ERROR,
        LGX_RECOVER_ABORT,
        "Requested resource not found. Verify resource exists and is accessible.",
        true
    }
};

static const size_t g_error_metadata_count = sizeof(g_error_metadata) / sizeof(g_error_metadata[0]);

/**
 * Get error metadata for a given error code
 */
static const lgx_error_metadata_t* get_error_metadata(lgx_result_t error) {
    for (size_t i = 0; i < g_error_metadata_count; i++) {
        if (g_error_metadata[i].code == error) {
            return &g_error_metadata[i];
        }
    }
    
    // Return default metadata for unknown errors
    static const lgx_error_metadata_t default_metadata = {
        LGX_ERROR_SYSTEM_ERROR,
        LGX_SEV_ERROR,
        LGX_RECOVER_ABORT,
        "Unknown error occurred. Check logs for details.",
        true
    };
    return &default_metadata;
}

// Error handler state
struct lgx_error_handler {
    pthread_mutex_t mutex;
    lgx_error_callback_t callback;
    void* callback_user_data;
};

/**
 * Initialize error handler
 */
lgx_result_t lgx_error_handler_init(lgx_error_handler_t** handler) {
    if (!handler) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_error_handler_t* eh = calloc(1, sizeof(lgx_error_handler_t));
    if (!eh) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    if (pthread_mutex_init(&eh->mutex, NULL) != 0) {
        free(eh);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    eh->callback = NULL;
    eh->callback_user_data = NULL;
    
    *handler = eh;
    return LGX_SUCCESS;
}

/**
 * Shutdown error handler
 */
lgx_result_t lgx_error_handler_shutdown(lgx_error_handler_t* handler) {
    if (!handler) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_destroy(&handler->mutex);
    free(handler);
    return LGX_SUCCESS;
}

/**
 * Set error context
 */
lgx_result_t lgx_error_handler_set_error(lgx_error_handler_t* handler,
                                        lgx_result_t error,
                                        const char* function,
                                        const char* file,
                                        int line) {
    // Get error metadata
    const lgx_error_metadata_t* metadata = get_error_metadata(error);
    
    // Set thread-local error context
    g_last_error.base.struct_size = sizeof(lgx_error_context_t);
    g_last_error.base.error_code = error;
    g_last_error.base.error_message = lgx_result_to_string(error);
    g_last_error.base.function_name = function;
    g_last_error.base.file_name = file;
    g_last_error.base.line_number = line;
    g_last_error.base.timestamp_ns = lgx_time_now_ns();
    
    // Set extended error context from metadata
    g_last_error.severity = metadata->severity;
    g_last_error.suggested_action = metadata->recovery;
    g_last_error.recovery_steps = metadata->recovery_steps;
    g_last_error.recoverable = metadata->recoverable;
    g_last_error.context_data = NULL;
    
    g_error_set = true;
    
    // Call registered callback if handler is available
    if (handler) {
        pthread_mutex_lock(&handler->mutex);
        if (handler->callback) {
            handler->callback(&g_last_error, handler->callback_user_data);
        }
        pthread_mutex_unlock(&handler->mutex);
    }
    
    // Also call global callback if set
    pthread_mutex_lock(&g_global_callback_mutex);
    if (g_global_callback) {
        g_global_callback(&g_last_error, g_global_callback_user_data);
    }
    pthread_mutex_unlock(&g_global_callback_mutex);
    
    return LGX_SUCCESS;
}

/**
 * Get last error
 */
lgx_result_t lgx_error_handler_get_last_error(lgx_error_handler_t* handler) {
    if (!handler) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_error_set) {
        return LGX_SUCCESS;
    }
    
    return g_last_error.base.error_code;
}

/**
 * Get last error with details
 */
lgx_result_t lgx_error_handler_get_last_error_ex(lgx_error_handler_t* handler,
                                                char* message_buffer, size_t message_size,
                                                char* recovery_buffer, size_t recovery_size) {
    if (!handler) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_error_set) {
        if (message_buffer && message_size > 0) {
            message_buffer[0] = '\0';
        }
        if (recovery_buffer && recovery_size > 0) {
            recovery_buffer[0] = '\0';
        }
        return LGX_SUCCESS;
    }
    
    if (message_buffer && message_size > 0) {
        strncpy(message_buffer, g_last_error.base.error_message, message_size - 1);
        message_buffer[message_size - 1] = '\0';
    }
    
    if (recovery_buffer && recovery_size > 0) {
        strncpy(recovery_buffer, g_last_error.recovery_steps, recovery_size - 1);
        recovery_buffer[recovery_size - 1] = '\0';
    }
    
    return g_last_error.base.error_code;
}

/**
 * Public API implementations
 */
lgx_error_context_t lgx_get_last_error(void) {
    if (!g_error_set) {
        lgx_error_context_t empty = {0};
        empty.struct_size = sizeof(lgx_error_context_t);
        empty.error_code = LGX_SUCCESS;
        empty.error_message = ""; // Always provide a valid string, even if empty
        empty.function_name = "";
        empty.file_name = "";
        return empty;
    }
    
    return g_last_error.base;
}

lgx_error_context_ex_t lgx_get_last_error_ex(void) {
    if (!g_error_set) {
        lgx_error_context_ex_t empty = {0};
        empty.base.struct_size = sizeof(lgx_error_context_t);
        empty.base.error_code = LGX_SUCCESS;
        return empty;
    }
    
    return g_last_error;
}

void lgx_clear_last_error(void) {
    g_error_set = false;
    memset(&g_last_error, 0, sizeof(g_last_error));
}

void lgx_set_error_handler(lgx_error_callback_t callback, void* user_data) {
    // Set global callback (works even before runtime init)
    pthread_mutex_lock(&g_global_callback_mutex);
    g_global_callback = callback;
    g_global_callback_user_data = user_data;
    pthread_mutex_unlock(&g_global_callback_mutex);
    
    // Also set runtime-specific callback if runtime is initialized
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (runtime && runtime->error_handler) {
        pthread_mutex_lock(&runtime->error_handler->mutex);
        runtime->error_handler->callback = callback;
        runtime->error_handler->callback_user_data = user_data;
        pthread_mutex_unlock(&runtime->error_handler->mutex);
    }
}

const char* lgx_result_to_string(lgx_result_t result) {
    switch (result) {
        case LGX_SUCCESS: return "Success";
        case LGX_ERROR_INVALID_PARAM: return "Invalid parameter";
        case LGX_ERROR_NOT_INITIALIZED: return "Runtime not initialized";
        case LGX_ERROR_ALREADY_INITIALIZED: return "Runtime already initialized";
        case LGX_ERROR_INCOMPATIBLE_VERSION: return "Incompatible version";
        case LGX_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case LGX_ERROR_IO_ERROR: return "I/O error";
        case LGX_ERROR_NOT_SUPPORTED: return "Operation not supported";
        case LGX_ERROR_LIBRARY_VERSION_MISMATCH: return "Library version mismatch";
        case LGX_ERROR_GPU_UNAVAILABLE: return "GPU unavailable";
        case LGX_ERROR_RESOURCE_LIMIT_EXCEEDED: return "Resource limit exceeded";
        case LGX_ERROR_HARDWARE_DEGRADED: return "Hardware degraded";
        case LGX_ERROR_INTENT_VALIDATION_FAILED: return "Intent validation failed";
        case LGX_ERROR_SYSTEM_ERROR: return "System error";
        case LGX_ERROR_INVALID_STATE: return "Invalid state";
        case LGX_ERROR_TIMEOUT: return "Timeout";
        case LGX_ERROR_NOT_FOUND: return "Not found";
        default: return "Unknown error";
    }
}