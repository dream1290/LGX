/**
 * LGX Performance Counters - Enhanced Observability
 * 
 * Provides performance counter registry with custom counters.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

#define MAX_CUSTOM_COUNTERS 256

// Built-in counter storage
static _Atomic uint64_t g_builtin_counters[LGX_COUNTER_NUMA_MIGRATIONS + 1] = {0};

// Custom counter entry
typedef struct {
    char name[64];
    _Atomic uint64_t value;
    bool in_use;
} custom_counter_entry_t;

// Custom counter registry
typedef struct {
    pthread_mutex_t mutex;
    custom_counter_entry_t counters[MAX_CUSTOM_COUNTERS];
    uint32_t next_id;
} counter_registry_t;

static counter_registry_t g_registry = {0};
static bool g_registry_initialized = false;

/**
 * Initialize counter registry
 */
lgx_result_t lgx_counters_init(void) {
    if (g_registry_initialized) {
        return LGX_SUCCESS;
    }
    
    if (pthread_mutex_init(&g_registry.mutex, NULL) != 0) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    memset(g_registry.counters, 0, sizeof(g_registry.counters));
    g_registry.next_id = 0;
    
    g_registry_initialized = true;
    return LGX_SUCCESS;
}

/**
 * Shutdown counter registry
 */
lgx_result_t lgx_counters_shutdown(void) {
    if (!g_registry_initialized) {
        return LGX_SUCCESS;
    }
    
    pthread_mutex_destroy(&g_registry.mutex);
    g_registry_initialized = false;
    
    return LGX_SUCCESS;
}

/**
 * Get built-in counter value
 */
uint64_t lgx_get_counter(lgx_counter_t counter) {
    if (counter > LGX_COUNTER_NUMA_MIGRATIONS) {
        return 0;
    }
    
    return atomic_load(&g_builtin_counters[counter]);
}

/**
 * Increment built-in counter
 */
void lgx_increment_builtin_counter(lgx_counter_t counter) {
    if (counter > LGX_COUNTER_NUMA_MIGRATIONS) {
        return;
    }
    
    atomic_fetch_add(&g_builtin_counters[counter], 1);
}

/**
 * Add to built-in counter
 */
void lgx_add_to_builtin_counter(lgx_counter_t counter, uint64_t value) {
    if (counter > LGX_COUNTER_NUMA_MIGRATIONS) {
        return;
    }
    
    atomic_fetch_add(&g_builtin_counters[counter], value);
}

/**
 * Reset all counters
 */
void lgx_reset_counters(void) {
    // Reset built-in counters
    for (size_t i = 0; i <= LGX_COUNTER_NUMA_MIGRATIONS; i++) {
        atomic_store(&g_builtin_counters[i], 0);
    }
    
    // Reset custom counters
    if (!g_registry_initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_registry.mutex);
    for (size_t i = 0; i < MAX_CUSTOM_COUNTERS; i++) {
        if (g_registry.counters[i].in_use) {
            atomic_store(&g_registry.counters[i].value, 0);
        }
    }
    pthread_mutex_unlock(&g_registry.mutex);
}

/**
 * Register custom counter
 */
lgx_custom_counter_t lgx_register_counter(const char* name) {
    if (!name || !g_registry_initialized) {
        return (lgx_custom_counter_t)-1;
    }
    
    pthread_mutex_lock(&g_registry.mutex);
    
    // Check if counter already exists
    for (size_t i = 0; i < MAX_CUSTOM_COUNTERS; i++) {
        if (g_registry.counters[i].in_use &&
            strcmp(g_registry.counters[i].name, name) == 0) {
            pthread_mutex_unlock(&g_registry.mutex);
            return (lgx_custom_counter_t)i;
        }
    }
    
    // Find free slot
    for (size_t i = 0; i < MAX_CUSTOM_COUNTERS; i++) {
        if (!g_registry.counters[i].in_use) {
            strncpy(g_registry.counters[i].name, name, sizeof(g_registry.counters[i].name) - 1);
            g_registry.counters[i].name[sizeof(g_registry.counters[i].name) - 1] = '\0';
            atomic_store(&g_registry.counters[i].value, 0);
            g_registry.counters[i].in_use = true;
            
            pthread_mutex_unlock(&g_registry.mutex);
            return (lgx_custom_counter_t)i;
        }
    }
    
    pthread_mutex_unlock(&g_registry.mutex);
    return (lgx_custom_counter_t)-1;  // Registry full
}

/**
 * Increment custom counter
 */
void lgx_increment_counter(lgx_custom_counter_t counter) {
    if (counter >= MAX_CUSTOM_COUNTERS || !g_registry_initialized) {
        return;
    }
    
    if (g_registry.counters[counter].in_use) {
        atomic_fetch_add(&g_registry.counters[counter].value, 1);
    }
}

/**
 * Add to custom counter
 */
void lgx_add_to_counter(lgx_custom_counter_t counter, uint64_t value) {
    if (counter >= MAX_CUSTOM_COUNTERS || !g_registry_initialized) {
        return;
    }
    
    if (g_registry.counters[counter].in_use) {
        atomic_fetch_add(&g_registry.counters[counter].value, value);
    }
}

/**
 * Get custom counter value
 */
uint64_t lgx_get_custom_counter(lgx_custom_counter_t counter) {
    if (counter >= MAX_CUSTOM_COUNTERS || !g_registry_initialized) {
        return 0;
    }
    
    if (!g_registry.counters[counter].in_use) {
        return 0;
    }
    
    return atomic_load(&g_registry.counters[counter].value);
}

/**
 * Get custom counter name
 */
const char* lgx_get_custom_counter_name(lgx_custom_counter_t counter) {
    if (counter >= MAX_CUSTOM_COUNTERS || !g_registry_initialized) {
        return NULL;
    }
    
    if (!g_registry.counters[counter].in_use) {
        return NULL;
    }
    
    return g_registry.counters[counter].name;
}
