#ifndef LGX_ALLOCATOR_PROTOTYPE_H
#define LGX_ALLOCATOR_PROTOTYPE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Prototype allocator functions for CSF-1 validation
void* lgx_alloc_prototype(size_t size);
void lgx_free_prototype(void* ptr);
void lgx_get_cache_stats(uint64_t* hits, uint64_t* misses);
void lgx_allocator_prototype_init(void);
void lgx_allocator_prototype_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // LGX_ALLOCATOR_PROTOTYPE_H