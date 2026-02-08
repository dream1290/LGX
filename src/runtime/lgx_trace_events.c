/**
 * LGX Trace Event System - Performance Profiling
 * 
 * Provides lightweight tracing for performance analysis with integration
 * hooks for external tools (perf, Valgrind, Tracy).
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

// Maximum trace event name length
#define MAX_TRACE_NAME_LENGTH 64

// Ring buffer size (must be power of 2)
#define TRACE_RING_BUFFER_SIZE 65536

// Trace event types
typedef enum {
    TRACE_EVENT_BEGIN,
    TRACE_EVENT_END,
    TRACE_EVENT_INSTANT,
} trace_event_type_t;

// Trace event structure
typedef struct {
    trace_event_type_t type;
    char name[MAX_TRACE_NAME_LENGTH];
    uint64_t timestamp_ns;
    uint32_t thread_id;
    uint64_t data;  // Optional data (e.g., allocation size)
} trace_event_t;

// Ring buffer for trace events
typedef struct {
    trace_event_t* events;
    _Atomic uint64_t write_index;
    _Atomic uint64_t read_index;
    size_t capacity;
    _Atomic uint64_t dropped_events;
} trace_ring_buffer_t;

// Trace system state
typedef struct {
    pthread_mutex_t mutex;
    bool enabled;
    trace_ring_buffer_t ring_buffer;
    
    // Integration hooks
    bool perf_enabled;
    bool valgrind_enabled;
    bool tracy_enabled;
    
    // Statistics
    _Atomic uint64_t total_events;
    _Atomic uint64_t begin_events;
    _Atomic uint64_t end_events;
    _Atomic uint64_t instant_events;
} trace_system_t;

static trace_system_t g_trace_system = {0};
static bool g_trace_initialized = false;

// Thread-local storage for nested trace tracking
static __thread trace_event_t* tls_trace_stack = NULL;
static __thread size_t tls_trace_stack_size = 0;
static __thread size_t tls_trace_stack_capacity = 0;

/**
 * Get current thread ID
 */
static uint32_t get_thread_id(void) {
    return (uint32_t)pthread_self();
}

/**
 * Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Initialize trace ring buffer
 */
static lgx_result_t init_ring_buffer(trace_ring_buffer_t* buffer, size_t capacity) {
    buffer->events = calloc(capacity, sizeof(trace_event_t));
    if (!buffer->events) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    atomic_store(&buffer->write_index, 0);
    atomic_store(&buffer->read_index, 0);
    buffer->capacity = capacity;
    atomic_store(&buffer->dropped_events, 0);
    
    return LGX_SUCCESS;
}

/**
 * Add event to ring buffer (lock-free)
 */
static void add_trace_event(trace_ring_buffer_t* buffer, const trace_event_t* event) {
    uint64_t write_idx = atomic_fetch_add(&buffer->write_index, 1);
    uint64_t read_idx = atomic_load(&buffer->read_index);
    
    // Check if buffer is full
    if (write_idx - read_idx >= buffer->capacity) {
        atomic_fetch_add(&buffer->dropped_events, 1);
        return;
    }
    
    // Write event to ring buffer
    size_t slot = write_idx % buffer->capacity;
    buffer->events[slot] = *event;
}

/**
 * Initialize trace system
 */
lgx_result_t lgx_trace_init(void) {
    if (g_trace_initialized) {
        return LGX_SUCCESS;
    }
    
    if (pthread_mutex_init(&g_trace_system.mutex, NULL) != 0) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    lgx_result_t result = init_ring_buffer(&g_trace_system.ring_buffer, TRACE_RING_BUFFER_SIZE);
    if (result != LGX_SUCCESS) {
        pthread_mutex_destroy(&g_trace_system.mutex);
        return result;
    }
    
    g_trace_system.enabled = false;
    g_trace_system.perf_enabled = false;
    g_trace_system.valgrind_enabled = false;
    g_trace_system.tracy_enabled = false;
    
    atomic_store(&g_trace_system.total_events, 0);
    atomic_store(&g_trace_system.begin_events, 0);
    atomic_store(&g_trace_system.end_events, 0);
    atomic_store(&g_trace_system.instant_events, 0);
    
    g_trace_initialized = true;
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "Trace system initialized (buffer size: %zu events)", TRACE_RING_BUFFER_SIZE);
    
    return LGX_SUCCESS;
}

/**
 * Shutdown trace system
 */
lgx_result_t lgx_trace_shutdown(void) {
    if (!g_trace_initialized) {
        return LGX_SUCCESS;
    }
    
    pthread_mutex_lock(&g_trace_system.mutex);
    
    if (g_trace_system.ring_buffer.events) {
        free(g_trace_system.ring_buffer.events);
        g_trace_system.ring_buffer.events = NULL;
    }
    
    pthread_mutex_unlock(&g_trace_system.mutex);
    pthread_mutex_destroy(&g_trace_system.mutex);
    
    // Free thread-local trace stack
    if (tls_trace_stack) {
        free(tls_trace_stack);
        tls_trace_stack = NULL;
        tls_trace_stack_size = 0;
        tls_trace_stack_capacity = 0;
    }
    
    g_trace_initialized = false;
    
    return LGX_SUCCESS;
}

/**
 * Enable/disable tracing
 */
lgx_result_t lgx_trace_enable(bool enabled) {
    if (!g_trace_initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_trace_system.mutex);
    g_trace_system.enabled = enabled;
    pthread_mutex_unlock(&g_trace_system.mutex);
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "Tracing %s", enabled ? "ENABLED" : "DISABLED");
    
    return LGX_SUCCESS;
}

/**
 * Check if tracing is enabled
 */
bool lgx_trace_is_enabled(void) {
    return g_trace_initialized && g_trace_system.enabled;
}

/**
 * Begin trace event
 */
void lgx_trace_begin(const char* name, uint64_t data) {
    if (!lgx_trace_is_enabled() || !name) {
        return;
    }
    
    trace_event_t event = {
        .type = TRACE_EVENT_BEGIN,
        .timestamp_ns = get_timestamp_ns(),
        .thread_id = get_thread_id(),
        .data = data
    };
    
    strncpy(event.name, name, MAX_TRACE_NAME_LENGTH - 1);
    event.name[MAX_TRACE_NAME_LENGTH - 1] = '\0';
    
    add_trace_event(&g_trace_system.ring_buffer, &event);
    
    atomic_fetch_add(&g_trace_system.total_events, 1);
    atomic_fetch_add(&g_trace_system.begin_events, 1);
    
    // Integration hooks
    if (g_trace_system.tracy_enabled) {
        // Tracy integration would go here
        // TracyZoneBegin(name);
    }
}

/**
 * End trace event
 */
void lgx_trace_end(const char* name) {
    if (!lgx_trace_is_enabled() || !name) {
        return;
    }
    
    trace_event_t event = {
        .type = TRACE_EVENT_END,
        .timestamp_ns = get_timestamp_ns(),
        .thread_id = get_thread_id(),
        .data = 0
    };
    
    strncpy(event.name, name, MAX_TRACE_NAME_LENGTH - 1);
    event.name[MAX_TRACE_NAME_LENGTH - 1] = '\0';
    
    add_trace_event(&g_trace_system.ring_buffer, &event);
    
    atomic_fetch_add(&g_trace_system.total_events, 1);
    atomic_fetch_add(&g_trace_system.end_events, 1);
    
    // Integration hooks
    if (g_trace_system.tracy_enabled) {
        // Tracy integration would go here
        // TracyZoneEnd();
    }
}

/**
 * Instant trace event (single point in time)
 */
void lgx_trace_instant(const char* name, uint64_t data) {
    if (!lgx_trace_is_enabled() || !name) {
        return;
    }
    
    trace_event_t event = {
        .type = TRACE_EVENT_INSTANT,
        .timestamp_ns = get_timestamp_ns(),
        .thread_id = get_thread_id(),
        .data = data
    };
    
    strncpy(event.name, name, MAX_TRACE_NAME_LENGTH - 1);
    event.name[MAX_TRACE_NAME_LENGTH - 1] = '\0';
    
    add_trace_event(&g_trace_system.ring_buffer, &event);
    
    atomic_fetch_add(&g_trace_system.total_events, 1);
    atomic_fetch_add(&g_trace_system.instant_events, 1);
}

/**
 * Get trace statistics
 */
lgx_result_t lgx_trace_get_stats(lgx_trace_stats_t* stats) {
    if (!g_trace_initialized || !stats) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    stats->total_events = atomic_load(&g_trace_system.total_events);
    stats->begin_events = atomic_load(&g_trace_system.begin_events);
    stats->end_events = atomic_load(&g_trace_system.end_events);
    stats->instant_events = atomic_load(&g_trace_system.instant_events);
    stats->dropped_events = atomic_load(&g_trace_system.ring_buffer.dropped_events);
    stats->buffer_capacity = g_trace_system.ring_buffer.capacity;
    
    uint64_t write_idx = atomic_load(&g_trace_system.ring_buffer.write_index);
    uint64_t read_idx = atomic_load(&g_trace_system.ring_buffer.read_index);
    stats->buffer_used = (write_idx - read_idx) < stats->buffer_capacity ? 
                         (write_idx - read_idx) : stats->buffer_capacity;
    
    return LGX_SUCCESS;
}

/**
 * Clear trace buffer
 */
lgx_result_t lgx_trace_clear(void) {
    if (!g_trace_initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_trace_system.mutex);
    
    atomic_store(&g_trace_system.ring_buffer.write_index, 0);
    atomic_store(&g_trace_system.ring_buffer.read_index, 0);
    atomic_store(&g_trace_system.ring_buffer.dropped_events, 0);
    
    atomic_store(&g_trace_system.total_events, 0);
    atomic_store(&g_trace_system.begin_events, 0);
    atomic_store(&g_trace_system.end_events, 0);
    atomic_store(&g_trace_system.instant_events, 0);
    
    pthread_mutex_unlock(&g_trace_system.mutex);
    
    return LGX_SUCCESS;
}

/**
 * Export trace events to JSON
 */
lgx_result_t lgx_trace_export(const char* output_path) {
    if (!g_trace_initialized || !output_path) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    FILE* file = fopen(output_path, "w");
    if (!file) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Failed to open trace export file: %s", output_path);
        return LGX_ERROR_IO_ERROR;
    }
    
    pthread_mutex_lock(&g_trace_system.mutex);
    
    // Write JSON header
    fprintf(file, "{\n");
    fprintf(file, "  \"traceEvents\": [\n");
    
    // Export events from ring buffer
    uint64_t read_idx = atomic_load(&g_trace_system.ring_buffer.read_index);
    uint64_t write_idx = atomic_load(&g_trace_system.ring_buffer.write_index);
    uint64_t count = (write_idx - read_idx) < g_trace_system.ring_buffer.capacity ?
                     (write_idx - read_idx) : g_trace_system.ring_buffer.capacity;
    
    bool first = true;
    for (uint64_t i = 0; i < count; i++) {
        size_t slot = (read_idx + i) % g_trace_system.ring_buffer.capacity;
        const trace_event_t* event = &g_trace_system.ring_buffer.events[slot];
        
        if (!first) {
            fprintf(file, ",\n");
        }
        first = false;
        
        // Chrome Trace Event Format
        const char* phase;
        switch (event->type) {
            case TRACE_EVENT_BEGIN: phase = "B"; break;
            case TRACE_EVENT_END: phase = "E"; break;
            case TRACE_EVENT_INSTANT: phase = "i"; break;
            default: phase = "?"; break;
        }
        
        fprintf(file, "    {\n");
        fprintf(file, "      \"name\": \"%s\",\n", event->name);
        fprintf(file, "      \"ph\": \"%s\",\n", phase);
        fprintf(file, "      \"ts\": %lu,\n", event->timestamp_ns / 1000);  // Convert to microseconds
        fprintf(file, "      \"pid\": 1,\n");
        fprintf(file, "      \"tid\": %u", event->thread_id);
        
        if (event->data != 0) {
            fprintf(file, ",\n      \"args\": {\"data\": %lu}", event->data);
        }
        
        fprintf(file, "\n    }");
    }
    
    fprintf(file, "\n  ],\n");
    
    // Write metadata
    fprintf(file, "  \"metadata\": {\n");
    fprintf(file, "    \"total_events\": %lu,\n", atomic_load(&g_trace_system.total_events));
    fprintf(file, "    \"dropped_events\": %lu,\n", atomic_load(&g_trace_system.ring_buffer.dropped_events));
    fprintf(file, "    \"buffer_capacity\": %zu\n", g_trace_system.ring_buffer.capacity);
    fprintf(file, "  }\n");
    fprintf(file, "}\n");
    
    pthread_mutex_unlock(&g_trace_system.mutex);
    
    fclose(file);
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "Trace exported to %s (%lu events)", output_path, count);
    
    return LGX_SUCCESS;
}

/**
 * Enable integration with external profiling tools
 */
lgx_result_t lgx_trace_enable_perf_integration(bool enabled) {
    if (!g_trace_initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_trace_system.mutex);
    g_trace_system.perf_enabled = enabled;
    pthread_mutex_unlock(&g_trace_system.mutex);
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "Perf integration %s", enabled ? "ENABLED" : "DISABLED");
    
    return LGX_SUCCESS;
}

lgx_result_t lgx_trace_enable_valgrind_integration(bool enabled) {
    if (!g_trace_initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_trace_system.mutex);
    g_trace_system.valgrind_enabled = enabled;
    pthread_mutex_unlock(&g_trace_system.mutex);
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "Valgrind integration %s", enabled ? "ENABLED" : "DISABLED");
    
    return LGX_SUCCESS;
}

lgx_result_t lgx_trace_enable_tracy_integration(bool enabled) {
    if (!g_trace_initialized) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_trace_system.mutex);
    g_trace_system.tracy_enabled = enabled;
    pthread_mutex_unlock(&g_trace_system.mutex);
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "Tracy integration %s", enabled ? "ENABLED" : "DISABLED");
    
    return LGX_SUCCESS;
}
