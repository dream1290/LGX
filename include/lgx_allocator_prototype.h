/**
 * LGX Allocator Prototype - DEPRECATED Compatibility Layer
 * 
 * DEPRECATED: This API is deprecated and will be removed in Phase 2.
 * Please migrate to specialized allocators:
 * - lgx_frame_alloc() for per-frame temporary allocations
 * - lgx_heap_alloc() for persistent allocations  
 * - lgx_gpu_alloc() for GPU memory
 */

#ifndef LGX_ALLOCATOR_PROTOTYPE_H
#define LGX_ALLOCATOR_PROTOTYPE_H

#include <stddef.h>
#include <stdint.h>
#include "lgx_version.h"  // For LGX_DEPRECATED

#ifdef __cplusplus
extern "C" {
#endif

// DEPRECATED: Prototype allocator functions for CSF-1 validation
LGX_DEPRECATED void* lgx_alloc_prototype(size_t size);
LGX_DEPRECATED void lgx_free_prototype(void* ptr);
LGX_DEPRECATED void lgx_get_cache_stats(uint64_t* hits, uint64_t* misses);
LGX_DEPRECATED void lgx_allocator_prototype_init(void);
LGX_DEPRECATED void lgx_allocator_prototype_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // LGX_ALLOCATOR_PROTOTYPE_H