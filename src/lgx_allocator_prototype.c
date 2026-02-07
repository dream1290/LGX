/**
 * LGX Allocator Prototype - Compatibility Layer
 * 
 * DEPRECATED: This compatibility layer is deprecated and will be removed in Phase 2.
 * Use the specialized allocators instead:
 * - lgx_frame_alloc() for per-frame temporary allocations
 * - lgx_heap_alloc() for persistent allocations
 * - lgx_gpu_alloc() for GPU memory
 * 
 * These functions delegate to the Phase 1 runtime for backward compatibility only.
 */

// Suppress deprecation warnings in this file (we're implementing the deprecated API)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "lgx_allocator_prototype.h"
#include "lgx_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global state for prototype compatibility
static bool g_prototype_initialized = false;
static bool g_deprecation_warning_shown = false;

/**
 * Show deprecation warning (once per process)
 */
static void show_deprecation_warning(void) {
    if (g_deprecation_warning_shown) {
        return;
    }
    
    fprintf(stderr, 
        "[LGX WARNING] Prototype allocator API is DEPRECATED and will be removed in Phase 2.\n"
        "  Please migrate to specialized allocators:\n"
        "  - lgx_frame_alloc() for per-frame temporary allocations\n"
        "  - lgx_heap_alloc() for persistent allocations\n"
        "  - lgx_gpu_alloc() for GPU memory\n"
        "  See documentation for migration guide.\n\n");
    
    g_deprecation_warning_shown = true;
}

/**
 * Initialize the prototype allocator (compatibility)
 * DEPRECATED: Use lgx_runtime_init() directly instead.
 */
void lgx_allocator_prototype_init(void) {
    show_deprecation_warning();
    
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
 * DEPRECATED: Use lgx_runtime_shutdown() directly instead.
 */
void lgx_allocator_prototype_cleanup(void) {
    show_deprecation_warning();
    if (!g_prototype_initialized) {
        return;
    }
    
    lgx_runtime_shutdown();
    g_prototype_initialized = false;
}

/**
 * Allocate memory using prototype interface (compatibility)
 * DEPRECATED: Use lgx_frame_alloc(), lgx_heap_alloc(), or lgx_gpu_alloc() instead.
 */
void* lgx_alloc_prototype(size_t size) {
    show_deprecation_warning();
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
 * DEPRECATED: Use lgx_frame_reset(), lgx_heap_free(), or lgx_gpu_free() instead.
 */
void lgx_free_prototype(void* ptr) {
    show_deprecation_warning();
    if (!g_prototype_initialized || !ptr) {
        return;
    }
    
    lgx_free(ptr);
}

/**
 * Get cache statistics (compatibility)
 * DEPRECATED: Use lgx_memory_stats() instead.
 */
void lgx_get_cache_stats(uint64_t* hits, uint64_t* misses) {
    show_deprecation_warning();
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


// Restore warnings
#pragma GCC diagnostic pop
