/**
 * LGX Allocator Prototype - Compatibility Layer
 * 
 * Provides compatibility functions for Phase 0 tests to work with Phase 1 implementation.
 * These functions delegate to the Phase 1 runtime.
 */

#include "lgx_allocator_prototype.h"
#include "lgx_runtime.h"
#include <stdlib.h>
#include <string.h>

// Global state for prototype compatibility
static bool g_prototype_initialized = false;

/**
 * Initialize the prototype allocator (compatibility)
 */
void lgx_allocator_prototype_init(void) {
    if (g_prototype_initialized) {
        return;
    }
    
    // Initialize the Phase 1 runtime with default config
    lgx_runtime_config_t* config = lgx_config_create();
    if (!config) {
        return;
    }
    
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    // Accept both success and already initialized as success
    if (result == LGX_SUCCESS || result == LGX_ERROR_ALREADY_INITIALIZED) {
        g_prototype_initialized = true;
    }
}

/**
 * Cleanup the prototype allocator (compatibility)
 */
void lgx_allocator_prototype_cleanup(void) {
    if (!g_prototype_initialized) {
        return;
    }
    
    lgx_runtime_shutdown();
    g_prototype_initialized = false;
}

/**
 * Allocate memory using prototype interface (compatibility)
 */
void* lgx_alloc_prototype(size_t size) {
    if (!g_prototype_initialized) {
        // Try to initialize if not already done
        lgx_allocator_prototype_init();
        if (!g_prototype_initialized) {
            return NULL;
        }
    }
    
    return lgx_alloc(size);
}

/**
 * Free memory using prototype interface (compatibility)
 */
void lgx_free_prototype(void* ptr) {
    if (!g_prototype_initialized || !ptr) {
        return;
    }
    
    lgx_free(ptr);
}

/**
 * Get cache statistics (compatibility)
 */
void lgx_get_cache_stats(uint64_t* hits, uint64_t* misses) {
    if (!hits || !misses || !g_prototype_initialized) {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        return;
    }
    
    // Get memory stats from Phase 1 runtime
    lgx_memory_stats_t mem_stats;
    lgx_result_t result = lgx_memory_stats(&mem_stats);
    if (result != LGX_SUCCESS) {
        *hits = 0;
        *misses = 0;
        return;
    }
    
    *hits = mem_stats.cache_hits;
    *misses = mem_stats.cache_misses;
}