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
    if (!handler) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Set thread-local error context
    g_last_error.base.struct_size = sizeof(lgx_error_context_t);
    g_last_error.base.error_code = error;
    g_last_error.base.error_message = lgx_result_to_string(error);
    g_last_error.base.function_name = function;
    g_last_error.base.file_name = file;
    g_last_error.base.line_number = line;
    g_last_error.base.timestamp_ns = lgx_time_now_ns();
    
    // Set extended error context
    g_last_error.severity = LGX_SEV_ERROR;
    g_last_error.suggested_action = LGX_RECOVER_ABORT;
    g_last_error.recovery_steps = "Check error code and retry";
    g_last_error.recoverable = true;
    g_last_error.context_data = NULL;
    
    g_error_set = true;
    
    // Call registered callback if any
    pthread_mutex_lock(&handler->mutex);
    if (handler->callback) {
        handler->callback(&g_last_error, handler->callback_user_data);
    }
    pthread_mutex_unlock(&handler->mutex);
    
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
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->error_handler) {
        return;
    }
    
    pthread_mutex_lock(&runtime->error_handler->mutex);
    runtime->error_handler->callback = callback;
    runtime->error_handler->callback_user_data = user_data;
    pthread_mutex_unlock(&runtime->error_handler->mutex);
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