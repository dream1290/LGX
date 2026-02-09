# Prototype Allocator Migration Guide

## Overview

The Phase 0 prototype allocator API (`lgx_alloc_prototype`, `lgx_free_prototype`) is **DEPRECATED** and will be removed in Phase 2. This guide helps you migrate to the specialized allocator APIs.

## Why Migrate?

The prototype allocator was a general-purpose malloc/free wrapper used for Phase 0 validation. Phase 1 introduces specialized allocators that are:

- **Faster**: Each allocator is optimized for its specific use case
- **More predictable**: Clear allocation patterns and lifetimes
- **Better instrumented**: Detailed per-allocator statistics
- **Hardware-aware**: Adapts to GPU, NUMA, huge pages automatically

## Migration Path

### 1. Identify Allocation Lifetime

**Question**: How long does the allocation live?

- **Per-frame (< 16ms)** → Use Frame Arena
- **Persistent (minutes/hours)** → Use Persistent Heap
- **GPU memory** → Use GPU Pool

### 2. Replace API Calls

#### Frame Arena (Per-Frame Temporary)

**Before (Deprecated):**
```c
void* temp_data = lgx_alloc_prototype(1024);
// ... use for current frame ...
lgx_free_prototype(temp_data);  // Manual free
```

**After (Recommended):**
```c
void* temp_data = lgx_frame_alloc(1024);
// ... use for current frame ...
// No manual free needed - reset at frame boundary
lgx_frame_reset();  // Called once per frame
```

**Benefits:**
- 10-100x faster allocation (bump pointer vs malloc)
- No fragmentation
- Automatic cleanup at frame boundary
- No need to track individual frees

#### Persistent Heap (Long-Lived)

**Before (Deprecated):**
```c
void* persistent = lgx_alloc_prototype(4096);
// ... use for entire session ...
lgx_free_prototype(persistent);
```

**After (Recommended):**
```c
void* persistent = lgx_heap_alloc(4096);
// ... use for entire session ...
lgx_heap_free(persistent);
```

**Benefits:**
- Buddy allocator reduces fragmentation
- Background defragmentation available
- Better statistics and health monitoring
- Huge page support for large allocations

#### GPU Memory

**Before (Deprecated):**
```c
// Not supported in prototype allocator
```

**After (Recommended):**
```c
lgx_gpu_allocation_t* gpu_mem = lgx_gpu_alloc(
    size, 
    alignment, 
    LGX_GPU_DEVICE_LOCAL
);
VkDeviceMemory memory = lgx_gpu_get_memory(gpu_mem);
VkDeviceSize offset = lgx_gpu_get_offset(gpu_mem);
// ... use GPU memory ...
lgx_gpu_free(gpu_mem);
```

**Benefits:**
- Hardware-aware allocation (ReBAR, device-local, host-visible)
- Buddy allocator for efficient GPU memory management
- Vulkan integration
- Automatic strategy selection

### 3. Update Initialization

**Before (Deprecated):**
```c
lgx_allocator_prototype_init();
// ... use allocators ...
lgx_allocator_prototype_cleanup();
```

**After (Recommended):**
```c
lgx_runtime_config_t* config = lgx_config_create();
lgx_runtime_init(config);
lgx_config_destroy(config);

// Initialize specialized allocators as needed
lgx_frame_arena_init();
lgx_persistent_heap_init();
lgx_gpu_pool_init(instance, physical_device, device);

// ... use allocators ...

// Shutdown
lgx_gpu_pool_shutdown();
lgx_persistent_heap_shutdown();
lgx_frame_arena_shutdown();
lgx_runtime_shutdown();
```

### 4. Update Statistics

**Before (Deprecated):**
```c
uint64_t hits, misses;
lgx_get_cache_stats(&hits, &misses);
```

**After (Recommended):**
```c
// Frame arena stats
frame_arena_stats_t frame_stats;
lgx_frame_get_stats(&frame_stats);
printf("Frame allocations: %lu\n", frame_stats.total_allocations);

// Persistent heap stats
lgx_heap_stats_t heap_stats;
lgx_heap_get_stats(&heap_stats);
printf("Heap fragmentation: %.2f%%\n", heap_stats.fragmentation_ratio * 100);

// Overall memory stats
lgx_memory_stats_t mem_stats;
lgx_memory_stats(&mem_stats);
printf("Total allocated: %zu MB\n", mem_stats.total_allocated_mb);
```

## Migration Checklist

- [ ] Identify allocation lifetimes (per-frame vs persistent vs GPU)
- [ ] Replace `lgx_alloc_prototype()` with appropriate specialized allocator
- [ ] Replace `lgx_free_prototype()` with appropriate free function
- [ ] Update initialization to use `lgx_runtime_init()` and specialized init functions
- [ ] Update statistics gathering to use specialized stats functions
- [ ] Test with deprecation warnings enabled
- [ ] Verify performance improvements (should see 2-10x speedup)

## Timeline

- **Phase 1 (Current)**: Prototype API marked deprecated, warnings shown
- **Phase 2 (Q2 2026)**: Prototype API removed entirely

## Need Help?

If you have questions about migration:
1. Check the API documentation in `include/lgx_runtime.h`
2. Review example code in `tests/phase0/test_frame_arena.c`
3. Contact the LGX team for migration assistance

## Performance Expectations

After migration, you should see:

- **Frame allocations**: 10-100x faster (< 10ns vs 100-1000ns)
- **Persistent allocations**: 2-5x faster, better fragmentation
- **GPU allocations**: Hardware-aware, optimal strategy selection
- **Overall**: Lower P99 latency, more predictable performance
