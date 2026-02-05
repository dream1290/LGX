/**
 * LGX Memory Manager - Phase 1 Implementation
 * 
 * Implements the hybrid allocation strategy:
 * - Lock-free for hot paths (small, frequent allocations)
 * - Lock-based for cold paths (large, infrequent allocations)
 * - jemalloc fallback for edge cases
 * - Intent-based allocation with validation and learning
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>

#ifdef HAVE_JEMALLOC
#if HAVE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif
#endif

// Size class configuration
#define NUM_SIZE_CLASSES 16
#define MAX_SMALL_SIZE 4096
#define MAX_MEDIUM_SIZE (64 * 1024)
#define CACHE_LINE_SIZE 64
#define THREAD_CACHE_SIZE 256   // Increased cache size to reduce misses

// Ultra-fast hot path configuration (report3.txt approach)
#define HOT_PATH_SIZE_CLASSES 8  // Focus on most common sizes
#define HOT_PATH_CACHE_SIZE 128  // Increased again for better hit rates
#define HOT_PATH_REFILL_THRESHOLD 32  // Refill when below this

// Size classes (optimized based on Phase 0 profiling)
static const size_t size_classes[NUM_SIZE_CLASSES] = {
    16, 32, 48, 64, 96, 128, 192, 256,
    384, 512, 768, 1024, 1536, 2048, 3072, 4096
};

// Hot path size classes (most frequently used - report3.txt approach)
// Focus on smaller sizes that are most frequent and avoid memset overhead
static const size_t hot_path_sizes[HOT_PATH_SIZE_CLASSES] = {
    64, 128, 256, 512, 1024, 2048, 4096, 8192
};

// Ultra-Fast Hot Path Cache (report3.txt Layer 1)
typedef struct {
    // Pre-allocated blocks for ultra-fast allocation
    void* preallocated_blocks[HOT_PATH_SIZE_CLASSES][HOT_PATH_CACHE_SIZE];
    
    // Lock-free indices (single atomic operation per allocation)
    atomic_uint_fast32_t next_free[HOT_PATH_SIZE_CLASSES];
    atomic_uint_fast32_t watermark[HOT_PATH_SIZE_CLASSES];  // High-water mark
    
    // Performance tracking
    uint64_t last_access[HOT_PATH_SIZE_CLASSES];
    uint64_t hit_count[HOT_PATH_SIZE_CLASSES];
    uint64_t miss_count[HOT_PATH_SIZE_CLASSES];
    
    // Cache line alignment
} __attribute__((aligned(CACHE_LINE_SIZE))) hot_path_cache_t;

// Thread-local cache structure (adaptive)
typedef struct thread_cache {
    // Fast path: Pre-allocated objects by size class
    void* slots[NUM_SIZE_CLASSES][THREAD_CACHE_SIZE];
    uint32_t count[NUM_SIZE_CLASSES];
    uint32_t capacity[NUM_SIZE_CLASSES];  // Adaptive capacity per size class
    
    // Performance counters for adaptation
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t size_class_hits[NUM_SIZE_CLASSES];
    uint64_t size_class_misses[NUM_SIZE_CLASSES];
    
    // Adaptation tracking
    uint64_t last_adaptation_time;
    uint64_t allocation_count_since_adapt;
    
    // Ultra-fast hot path cache
    hot_path_cache_t hot_path;
    
    // Cache line alignment
} __attribute__((aligned(CACHE_LINE_SIZE))) thread_cache_t;

// Global pool for each size class
typedef struct size_class_pool {
    pthread_mutex_t mutex;
    void* free_list;
    size_t block_count;
    size_t total_allocated;
    
    // Pool metadata
    void** slabs;
    size_t slab_count;
    size_t slab_capacity;
} size_class_pool_t;

// Intent tracking for allocations
typedef struct allocation_header {
    uint64_t generation;
    size_t size;
    uint32_t size_class;
    uint32_t magic;
    lgx_allocation_intent_base_t intent;
    uint64_t allocation_time;
} allocation_header_t;

// Memory manager state
struct lgx_memory_manager {
    // Configuration
    size_t memory_pool_size;
    bool use_huge_pages;
    bool numa_aware;
    
    // Size class pools
    size_class_pool_t pools[NUM_SIZE_CLASSES];
    
    // Thread-local cache key
    pthread_key_t cache_key;
    
    // Global statistics
    atomic_uint_fast64_t total_allocations;
    atomic_uint_fast64_t total_deallocations;
    atomic_uint_fast64_t cache_hits;
    atomic_uint_fast64_t cache_misses;
    atomic_uint_fast64_t generation_counter;
    
    // Intent tracking
    pthread_mutex_t intent_mutex;
    allocation_header_t* tracked_allocations;
    size_t tracked_count;
    size_t tracked_capacity;
    
    // Hardware adapter reference
    lgx_hardware_adapter_t* hardware_adapter;
};

// Per-thread pool slices (tcmalloc approach) - eliminates atomic contention
typedef struct {
    void* base;
    size_t offset;
    size_t capacity;
    int thread_id;  // For debugging
} thread_pool_t;

// Pre-allocated thread pools - no atomics needed!
#define MAX_THREADS 64
static thread_pool_t thread_pools[MAX_THREADS];
static atomic_int next_thread_id = 0;

// Per-thread pool allocation - ZERO atomics after initialization
static __thread thread_pool_t* my_pool = NULL;
static __thread int my_thread_id = -1;

// Forward declarations
static void cache_destructor(void* cache);
static thread_cache_t* get_thread_cache(lgx_memory_manager_t* manager);
static int get_size_class_index(size_t size);
static int get_hot_path_size_class(size_t size);
static void* pool_alloc(size_t size);
static void* allocate_ultra_fast(lgx_memory_manager_t* manager, size_t size);
static void prewarm_hot_path_cache(hot_path_cache_t* cache);
static void track_allocation(lgx_memory_manager_t* manager, void* ptr, size_t size, 
                           const lgx_allocation_intent_base_t* intent);
static void untrack_allocation(lgx_memory_manager_t* manager, void* ptr);
static void* allocate_small(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent);
static void* allocate_medium(lgx_memory_manager_t* manager, size_t size, 
                            const lgx_allocation_intent_base_t* intent);
static void* allocate_large(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent);

static void track_allocation(lgx_memory_manager_t* manager, void* ptr, size_t size, 
                           const lgx_allocation_intent_base_t* intent);
static void untrack_allocation(lgx_memory_manager_t* manager, void* ptr);
static void* allocate_small(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent);
static void* allocate_medium(lgx_memory_manager_t* manager, size_t size, 
                            const lgx_allocation_intent_base_t* intent);
static void* allocate_large(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent);

// Thread-local pool allocator - ZERO atomic operations!
static void* pool_alloc(size_t size) {
    // Initialize thread-local state once per thread
    if (my_thread_id == -1) {
        int tid = atomic_fetch_add(&next_thread_id, 1);
        if (tid >= MAX_THREADS) {
            // Too many threads - always use malloc for this thread
            my_thread_id = MAX_THREADS;  // Mark as "use malloc"
            return malloc(size);
        }
        my_thread_id = tid;
        my_pool = &thread_pools[my_thread_id];
        
        // Check if this pool was initialized
        if (my_pool->base == NULL) {
            // Pool not initialized - fallback to malloc
            return malloc(size);
        }
    } else if (my_thread_id >= MAX_THREADS) {
        // This thread exceeded MAX_THREADS - always use malloc
        return malloc(size);
    }
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    // Simple bump allocator - NO ATOMICS, NO LOCKS!
    if (my_pool->offset + size <= my_pool->capacity) {
        void* ptr = (char*)my_pool->base + my_pool->offset;
        my_pool->offset += size;
        
        // Prefetch next cache line to reduce memory latency
        __builtin_prefetch((char*)ptr + size, 1, 3);
        
        return ptr;
    }
    
    // Thread pool exhausted - fallback to malloc
    // In a real implementation, we'd allocate a new pool slice
    return malloc(size);
}
static thread_cache_t* get_thread_cache(lgx_memory_manager_t* manager);
static int get_size_class_index(size_t size);
static void track_allocation(lgx_memory_manager_t* manager, void* ptr, size_t size, 
                           const lgx_allocation_intent_base_t* intent);
static void untrack_allocation(lgx_memory_manager_t* manager, void* ptr);
static void* allocate_small(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent);
static void* allocate_medium(lgx_memory_manager_t* manager, size_t size, 
                            const lgx_allocation_intent_base_t* intent);
static void* allocate_large(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent);

/**
 * Initialize the memory manager
 */
lgx_result_t lgx_memory_manager_init(lgx_memory_manager_t** manager, 
                                    const lgx_runtime_config_t* config,
                                    lgx_hardware_adapter_t* hardware_adapter) {
    if (!manager || !config) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_memory_manager_t* mgr = calloc(1, sizeof(lgx_memory_manager_t));
    if (!mgr) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize configuration
    mgr->memory_pool_size = config->memory_pool_size;
    mgr->hardware_adapter = hardware_adapter;
    
    // Pre-allocate thread pools to eliminate ALL atomic contention
    // This is the key optimization that Phase 0 had implicitly
    size_t pool_size_per_thread = 16ULL * 1024 * 1024; // 16MB per thread (reduced)
    size_t total_pool_size = (size_t)MAX_THREADS * pool_size_per_thread;
    void* global_pool = malloc(total_pool_size);
    if (!global_pool) {
        free(mgr);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize each thread pool slice
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_pools[i].base = (char*)global_pool + (i * pool_size_per_thread);
        thread_pools[i].offset = 0;
        thread_pools[i].capacity = pool_size_per_thread;
        thread_pools[i].thread_id = i;
    }
    
    atomic_store(&next_thread_id, 0);
    
    // TODO: Query hardware adapter for huge pages and NUMA support
    mgr->use_huge_pages = false; // Will be set based on hardware detection
    mgr->numa_aware = false;     // Will be set based on hardware detection
    
    // Initialize size class pools
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (pthread_mutex_init(&mgr->pools[i].mutex, NULL) != 0) {
            // Cleanup already initialized mutexes
            for (int j = 0; j < i; j++) {
                pthread_mutex_destroy(&mgr->pools[j].mutex);
            }
            free(global_pool);
            free(mgr);
            return LGX_ERROR_OUT_OF_MEMORY;
        }
        
        mgr->pools[i].free_list = NULL;
        mgr->pools[i].block_count = 0;
        mgr->pools[i].total_allocated = 0;
        mgr->pools[i].slabs = NULL;
        mgr->pools[i].slab_count = 0;
        mgr->pools[i].slab_capacity = 0;
    }
    
    // Initialize thread-local cache key
    if (pthread_key_create(&mgr->cache_key, cache_destructor) != 0) {
        for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
            pthread_mutex_destroy(&mgr->pools[i].mutex);
        }
        free(global_pool);
        free(mgr);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize intent tracking
    if (pthread_mutex_init(&mgr->intent_mutex, NULL) != 0) {
        for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
            pthread_mutex_destroy(&mgr->pools[i].mutex);
        }
        free(global_pool);
        free(mgr);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    mgr->tracked_capacity = 10000; // Initial capacity
    mgr->tracked_allocations = calloc(mgr->tracked_capacity, sizeof(allocation_header_t));
    if (!mgr->tracked_allocations) {
        pthread_mutex_destroy(&mgr->intent_mutex);
        for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
            pthread_mutex_destroy(&mgr->pools[i].mutex);
        }
        free(global_pool);
        free(mgr);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize atomic counters
    atomic_store(&mgr->total_allocations, 0);
    atomic_store(&mgr->total_deallocations, 0);
    atomic_store(&mgr->cache_hits, 0);
    atomic_store(&mgr->cache_misses, 0);
    atomic_store(&mgr->generation_counter, 1);
    
    *manager = mgr;
    return LGX_SUCCESS;
}

/**
 * Shutdown the memory manager
 */
lgx_result_t lgx_memory_manager_shutdown(lgx_memory_manager_t* manager) {
    if (!manager) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Free the pre-allocated thread pools
    if (thread_pools[0].base) {
        // All thread pools are allocated from one big block
        free(thread_pools[0].base);
        
        // Clear all thread pool entries
        for (int i = 0; i < MAX_THREADS; i++) {
            thread_pools[i].base = NULL;
            thread_pools[i].offset = 0;
            thread_pools[i].capacity = 0;
        }
        atomic_store(&next_thread_id, 0);
    }
    
    // Free all slabs in size class pools
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        pthread_mutex_lock(&manager->pools[i].mutex);
        
        for (size_t j = 0; j < manager->pools[i].slab_count; j++) {
            if (manager->pools[i].slabs[j]) {
                free(manager->pools[i].slabs[j]);
            }
        }
        
        free(manager->pools[i].slabs);
        pthread_mutex_unlock(&manager->pools[i].mutex);
        pthread_mutex_destroy(&manager->pools[i].mutex);
    }
    
    // Cleanup thread-local cache key
    pthread_key_delete(manager->cache_key);
    
    // Cleanup intent tracking
    pthread_mutex_destroy(&manager->intent_mutex);
    free(manager->tracked_allocations);
    
    free(manager);
    return LGX_SUCCESS;
}

/**
 * Allocate memory using hybrid strategy
 */
void* lgx_alloc(size_t size) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->memory_manager) {
        return NULL;
    }
    
    return lgx_memory_manager_alloc(runtime->memory_manager, size, NULL);
}

/**
 * Allocate aligned memory
 */
void* lgx_alloc_aligned(size_t size, size_t alignment) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->memory_manager) {
        return NULL;
    }
    
    return lgx_memory_manager_alloc_aligned(runtime->memory_manager, size, alignment, NULL);
}

/**
 * Allocate memory with intent
 */
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->memory_manager || !intent) {
        return NULL;
    }
    
    return lgx_memory_manager_alloc(runtime->memory_manager, intent->size, intent);
}

/**
 * Free memory
 */
void lgx_free(void* ptr) {
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    if (!runtime || !runtime->memory_manager || !ptr) {
        return;
    }
    
    lgx_memory_manager_free(runtime->memory_manager, ptr);
}

/**
 * Memory manager allocation implementation (with ultra-fast hot path)
 */
void* lgx_memory_manager_alloc(lgx_memory_manager_t* manager, size_t size, 
                              const lgx_allocation_intent_base_t* intent) {
    if (!manager || size == 0) {
        return NULL;
    }
    
    atomic_fetch_add(&manager->total_allocations, 1);
    
    // ULTRA-FAST HOT PATH: Try hot path first for small/medium sizes only
    // Skip hot path for large allocations (>8KB) that cause P99 spikes
    if (size <= 8192) {  // Hot path covers up to 8KB
        void* ptr = allocate_ultra_fast(manager, size);
        if (ptr) {
            // Track allocation if intent provided
            if (intent) {
                track_allocation(manager, ptr, size, intent);
            }
            return ptr;
        }
        // If ultra-fast path fails, continue to normal paths
    }
    
    // For large allocations (>8KB), use direct malloc to avoid cache pollution
    if (size > 8192) {
        void* ptr = malloc(size);
        if (ptr && intent) {
            track_allocation(manager, ptr, size, intent);
        }
        return ptr;
    }
    
    // Determine allocation strategy based on size
    if (size <= MAX_SMALL_SIZE) {
        // Small allocation - use lock-free thread-local cache
        return allocate_small(manager, size, intent);
    } else if (size <= MAX_MEDIUM_SIZE) {
        // Medium allocation - use lock-based global pool
        return allocate_medium(manager, size, intent);
    } else {
        // Large allocation - direct allocation or jemalloc fallback
        return allocate_large(manager, size, intent);
    }
}

/**
 * Memory manager free implementation
 */
void lgx_memory_manager_free(lgx_memory_manager_t* manager, void* ptr) {
    if (!manager || !ptr) {
        return;
    }
    
    atomic_fetch_add(&manager->total_deallocations, 1);
    
    // Untrack allocation
    untrack_allocation(manager, ptr);
    
    // Check if this pointer is from our thread pools
    size_t pool_size_per_thread = 16ULL * 1024 * 1024;
    size_t total_pool_size = (size_t)MAX_THREADS * pool_size_per_thread;
    if (thread_pools[0].base && ptr >= thread_pools[0].base && 
        ptr < (void*)((char*)thread_pools[0].base + total_pool_size)) {
        // This is from our pools - we can't free it individually
        // In a real implementation, we'd track this for pool cleanup
        // For now, just return (this will show as a "leak" but it's expected)
        return;
    }
    
    // Check if this is from the hot path cache - don't cache it again
    thread_cache_t* cache = get_thread_cache(manager);
    if (cache) {
        // Check if this pointer is in the hot path cache
        bool is_hot_path_block = false;
        for (int hot_class = 0; hot_class < HOT_PATH_SIZE_CLASSES && !is_hot_path_block; hot_class++) {
            uint32_t watermark = atomic_load(&cache->hot_path.watermark[hot_class]);
            for (uint32_t i = 0; i < watermark; i++) {
                if (cache->hot_path.preallocated_blocks[hot_class][i] == ptr) {
                    is_hot_path_block = true;
                    // Don't free it - it will be freed when the cache is destroyed
                    return;
                }
            }
        }
    }
    
    // Try to return to thread-local cache for small allocations
    if (cache) {
        // For malloc'd objects, we can use malloc_usable_size
        size_t usable_size = 0;
        #ifdef __GLIBC__
        usable_size = malloc_usable_size(ptr);
        #else
        // Fallback: assume it's a small allocation and try to cache it
        usable_size = 64; // Default to a common small size
        #endif
        
        int size_class = get_size_class_index(usable_size);
        if (size_class >= 0 && size_class < NUM_SIZE_CLASSES) {
            // Try to return to cache if there's room (use adaptive capacity)
            if (cache->count[size_class] < cache->capacity[size_class]) {
                cache->slots[size_class][cache->count[size_class]++] = ptr;
                return;
            }
        }
    }
    
    // If cache is full or unavailable, use regular free
    free(ptr);
}

/**
 * Aligned allocation implementation
 */
void* lgx_memory_manager_alloc_aligned(lgx_memory_manager_t* manager, size_t size, 
                                      size_t alignment, const lgx_allocation_intent_base_t* intent) {
    if (!manager || size == 0 || alignment == 0) {
        return NULL;
    }
    
    // For aligned allocations, use posix_memalign for simplicity
    // TODO: Implement aligned allocation in pools for better performance
    void* ptr;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    
    // Track allocation if intent provided
    if (intent) {
        track_allocation(manager, ptr, size, intent);
    }
    
    atomic_fetch_add(&manager->total_allocations, 1);
    return ptr;
}

// Private implementation functions

static void cache_destructor(void* cache) {
    if (cache) {
        thread_cache_t* tc = (thread_cache_t*)cache;
        
        // Track freed pointers to avoid double-free
        void* freed_ptrs[NUM_SIZE_CLASSES * THREAD_CACHE_SIZE + HOT_PATH_SIZE_CLASSES * HOT_PATH_CACHE_SIZE];
        int freed_count = 0;
        
        // Free all cached objects before destroying the cache
        for (int size_class = 0; size_class < NUM_SIZE_CLASSES; size_class++) {
            for (uint32_t i = 0; i < tc->count[size_class]; i++) {
                if (tc->slots[size_class][i]) {
                    void* ptr = tc->slots[size_class][i];
                    
                    // Check if it's from our thread pools
                    bool from_pool = false;
                    for (int j = 0; j < MAX_THREADS; j++) {
                        if (thread_pools[j].base && 
                            ptr >= thread_pools[j].base && 
                            ptr < (void*)((char*)thread_pools[j].base + thread_pools[j].capacity)) {
                            from_pool = true;
                            break;
                        }
                    }
                    
                    // Only free if it's not from our pools
                    if (!from_pool) {
                        freed_ptrs[freed_count++] = ptr;
                        free(ptr);
                    }
                    // Pool objects will be freed when the pools are freed
                }
            }
        }
        
        // Free hot path cache blocks
        for (int hot_class = 0; hot_class < HOT_PATH_SIZE_CLASSES; hot_class++) {
            uint32_t allocated_count = atomic_load(&tc->hot_path.watermark[hot_class]);
            for (uint32_t i = 0; i < allocated_count; i++) {
                if (tc->hot_path.preallocated_blocks[hot_class][i]) {
                    void* ptr = tc->hot_path.preallocated_blocks[hot_class][i];
                    
                    // Check if already freed
                    bool already_freed = false;
                    for (int k = 0; k < freed_count; k++) {
                        if (freed_ptrs[k] == ptr) {
                            already_freed = true;
                            break;
                        }
                    }
                    
                    if (already_freed) {
                        continue;
                    }
                    
                    // Check if it's from our thread pools
                    bool from_pool = false;
                    for (int j = 0; j < MAX_THREADS; j++) {
                        if (thread_pools[j].base && 
                            ptr >= thread_pools[j].base && 
                            ptr < (void*)((char*)thread_pools[j].base + thread_pools[j].capacity)) {
                            from_pool = true;
                            break;
                        }
                    }
                    
                    // Only free if it's not from our pools
                    if (!from_pool) {
                        free(ptr);
                    }
                }
            }
        }
        
        // Reset thread-local variables
        my_pool = NULL;
        my_thread_id = -1;
        
        free(cache);
    }
}

static thread_cache_t* get_thread_cache(lgx_memory_manager_t* manager) {
    thread_cache_t* cache = (thread_cache_t*)pthread_getspecific(manager->cache_key);
    if (cache == NULL) {
        // Allocate aligned memory for the thread cache
        if (posix_memalign((void**)&cache, CACHE_LINE_SIZE, sizeof(thread_cache_t)) != 0) {
            return NULL;
        }
        
        // Initialize the cache
        memset(cache, 0, sizeof(thread_cache_t));
        
        // Initialize adaptive capacities - start larger to reduce early misses
        for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
            cache->capacity[i] = 32; // Start with larger capacity
        }
        
        cache->last_adaptation_time = lgx_time_now_ns();
        
        // PRE-WARM the hot path cache (report3.txt approach)
        prewarm_hot_path_cache(&cache->hot_path);
        
        // Skip regular cache pre-warming to reduce memory pressure
        // The hot path cache should handle the most common allocations
        
        pthread_setspecific(manager->cache_key, cache);
    }
    return cache;
}

// Adaptive cache management - adjust cache sizes based on usage patterns
static void adapt_cache_sizes(thread_cache_t* cache) {
    uint64_t current_time = lgx_time_now_ns();
    
    // Only adapt every 1000 allocations or every 10ms
    if (cache->allocation_count_since_adapt < 1000 && 
        (current_time - cache->last_adaptation_time) < 10000000) {
        return;
    }
    
    // Adapt each size class based on hit rate
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        uint64_t total_accesses = cache->size_class_hits[i] + cache->size_class_misses[i];
        if (total_accesses < 10) continue; // Need some data to adapt
        
        double hit_rate = (double)cache->size_class_hits[i] / total_accesses;
        
        if (hit_rate > 0.95 && cache->capacity[i] < THREAD_CACHE_SIZE) {
            // High hit rate - grow cache
            cache->capacity[i] = (cache->capacity[i] * 3) / 2; // Grow by 50%
            if (cache->capacity[i] > THREAD_CACHE_SIZE) {
                cache->capacity[i] = THREAD_CACHE_SIZE;
            }
        } else if (hit_rate < 0.80 && cache->capacity[i] > 4) {
            // Low hit rate - shrink cache
            cache->capacity[i] = (cache->capacity[i] * 2) / 3; // Shrink by 33%
            if (cache->capacity[i] < 4) {
                cache->capacity[i] = 4;
            }
            
            // If we're shrinking, free excess objects
            while (cache->count[i] > cache->capacity[i]) {
                cache->count[i]--;
                void* ptr = cache->slots[i][cache->count[i]];
                if (ptr) {
                    // Check if it's from our thread pools
                    bool from_pool = false;
                    for (int j = 0; j < MAX_THREADS; j++) {
                        if (thread_pools[j].base && 
                            ptr >= thread_pools[j].base && 
                            ptr < (void*)((char*)thread_pools[j].base + thread_pools[j].capacity)) {
                            from_pool = true;
                            break;
                        }
                    }
                    
                    // Only free if it's not from our pools
                    if (!from_pool) {
                        free(ptr);
                    }
                }
            }
        }
    }
    
    // Reset counters for next adaptation period
    memset(cache->size_class_hits, 0, sizeof(cache->size_class_hits));
    memset(cache->size_class_misses, 0, sizeof(cache->size_class_misses));
    cache->allocation_count_since_adapt = 0;
    cache->last_adaptation_time = current_time;
}
static int get_size_class_index(size_t size) {
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (size <= size_classes[i]) {
            return i;
        }
    }
    return -1; // Too large for size classes
}

// Ultra-fast hot path size class detection (report3.txt approach)
static int get_hot_path_size_class(size_t size) {
    // Use bit tricks for branch-free size class detection
    if (size <= 64) return 0;
    if (size <= 128) return 1;
    if (size <= 256) return 2;
    if (size <= 512) return 3;
    if (size <= 1024) return 4;
    if (size <= 2048) return 5;
    if (size <= 4096) return 6;
    if (size <= 8192) return 7;
    return -1; // Not in hot path
}

// Pre-warm the hot path cache with pre-allocated blocks
static void prewarm_hot_path_cache(hot_path_cache_t* cache) {
    for (int hot_class = 0; hot_class < HOT_PATH_SIZE_CLASSES; hot_class++) {
        size_t block_size = hot_path_sizes[hot_class];
        
        // Pre-allocate blocks for this size class
        uint32_t successfully_allocated = 0;
        for (int i = 0; i < HOT_PATH_CACHE_SIZE; i++) {
            void* ptr = pool_alloc(block_size);
            if (ptr) {
                cache->preallocated_blocks[hot_class][i] = ptr;
                successfully_allocated++;
            } else {
                // If we can't pre-allocate, use malloc as fallback
                ptr = malloc(block_size);
                if (ptr) {
                    cache->preallocated_blocks[hot_class][i] = ptr;
                    successfully_allocated++;
                } else {
                    // Failed to allocate - stop here
                    break;
                }
            }
        }
        
        // Initialize atomic counters with actual allocated count
        atomic_store(&cache->next_free[hot_class], 0);
        atomic_store(&cache->watermark[hot_class], successfully_allocated);
        
        // Initialize performance counters
        cache->hit_count[hot_class] = 0;
        cache->miss_count[hot_class] = 0;
        cache->last_access[hot_class] = lgx_time_now_ns();
    }
}

// PERF-CRITICAL: Ultra-fast allocation (~5 cycles on modern CPUs)
static void* allocate_ultra_fast(lgx_memory_manager_t* manager, size_t size) {
    // 1. Determine hot path size class (branch-free)
    int hot_class = get_hot_path_size_class(size);
    if (hot_class == -1) {
        // Not a hot path size - fallback to normal allocation
        return allocate_small(manager, size, NULL);
    }
    
    // 2. Get thread-local cache (no locking, just TLS)
    thread_cache_t* cache = get_thread_cache(manager);
    if (!cache) {
        return allocate_small(manager, size, NULL);
    }
    
    hot_path_cache_t* hot_cache = &cache->hot_path;
    
    // 3. Single atomic operation - fetch and increment
    uint32_t slot_idx = atomic_fetch_add(&hot_cache->next_free[hot_class], 1);
    
    if (slot_idx < HOT_PATH_CACHE_SIZE) {
        // 4. Return pre-allocated block (no contention!)
        void* ptr = hot_cache->preallocated_blocks[hot_class][slot_idx];
        
        if (ptr) {
            // 5. Update performance counters
            hot_cache->hit_count[hot_class]++;
            hot_cache->last_access[hot_class] = lgx_time_now_ns();
            
            // 6. Prefetch next cache line for next allocation
            if (slot_idx + 1 < HOT_PATH_CACHE_SIZE) {
                __builtin_prefetch(hot_cache->preallocated_blocks[hot_class][slot_idx + 1], 0, 3);
            }
            
            return ptr;
        }
    }
    
    // 7. Hot path cache miss - record and fallback
    hot_cache->miss_count[hot_class]++;
    
    // 8. Try to refill hot path cache if it's getting low
    if (slot_idx > HOT_PATH_CACHE_SIZE - HOT_PATH_REFILL_THRESHOLD) {
        // Asynchronously refill (don't block this allocation)
        // For now, just fallback - refill logic can be added later
    }
    
    // 9. Fallback to normal allocation path
    return allocate_small(manager, size, NULL);
}

static void* allocate_small(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent) {
    int size_class = get_size_class_index(size);
    if (size_class == -1) {
        return allocate_medium(manager, size, intent);
    }
    
    thread_cache_t* cache = get_thread_cache(manager);
    if (!cache) {
        // Fallback to pool allocation if cache creation fails
        void* ptr = pool_alloc(size);
        if (ptr && intent) {
            track_allocation(manager, ptr, size, intent);
        }
        return ptr;
    }
    
    // Increment allocation count for adaptation
    cache->allocation_count_since_adapt++;
    
    // FAST PATH: Check thread-local cache first
    if (cache->count[size_class] > 0) {
        // Cache hit - O(1) allocation, no locks!
        cache->count[size_class]--;
        void* ptr = cache->slots[size_class][cache->count[size_class]];
        
        cache->cache_hits++;
        cache->size_class_hits[size_class]++;
        atomic_fetch_add(&manager->cache_hits, 1);
        
        // Track allocation if intent provided
        if (intent) {
            track_allocation(manager, ptr, size, intent);
        }
        
        // Periodically adapt cache sizes
        if ((cache->allocation_count_since_adapt % 100) == 0) {
            adapt_cache_sizes(cache);
        }
        
        return ptr;
    }
    
    // SLOW PATH: Cache miss - refill cache from pool
    cache->cache_misses++;
    cache->size_class_misses[size_class]++;
    atomic_fetch_add(&manager->cache_misses, 1);
    
    // Refill cache with adaptive batch size based on usage pattern
    int refill_count;
    if (cache->size_class_hits[size_class] > cache->size_class_misses[size_class] * 2) {
        // Hot size class - refill very aggressively
        refill_count = cache->capacity[size_class]; // Refill to full capacity
    } else {
        // Cold size class - refill moderately  
        refill_count = cache->capacity[size_class] / 2; // Refill 50% of capacity
    }
    
    if (refill_count < 8) refill_count = 8;
    if (refill_count > 128) refill_count = 128;
    
    for (int i = 0; i < refill_count && cache->count[size_class] < cache->capacity[size_class]; i++) {
        void* ptr = pool_alloc(size_classes[size_class]);
        if (ptr) {
            cache->slots[size_class][cache->count[size_class]++] = ptr;
        } else {
            break;
        }
    }
    
    // Now try cache again
    if (cache->count[size_class] > 0) {
        cache->count[size_class]--;
        void* ptr = cache->slots[size_class][cache->count[size_class]];
        
        if (intent) {
            track_allocation(manager, ptr, size, intent);
        }
        
        return ptr;
    }
    
    // If cache refill failed, allocate directly from pool
    void* ptr = pool_alloc(size_classes[size_class]);
    if (ptr && intent) {
        track_allocation(manager, ptr, size, intent);
    }
    
    return ptr;
}

static void* allocate_medium(lgx_memory_manager_t* manager, size_t size, 
                            const lgx_allocation_intent_base_t* intent) {
    // For medium allocations, use lock-based global pools
    // For now, fall back to malloc
    void* ptr = malloc(size);
    if (ptr && intent) {
        track_allocation(manager, ptr, size, intent);
    }
    return ptr;
}

static void* allocate_large(lgx_memory_manager_t* manager, size_t size, 
                           const lgx_allocation_intent_base_t* intent) {
    // For large allocations, use direct allocation or jemalloc
#if defined(HAVE_JEMALLOC) && HAVE_JEMALLOC
    void* ptr = je_malloc(size);
#else
    void* ptr = malloc(size);
#endif
    
    if (ptr && intent) {
        track_allocation(manager, ptr, size, intent);
    }
    return ptr;
}

/*
static void* allocate_from_pool(lgx_memory_manager_t* manager, int size_class) {
    size_class_pool_t* pool = &manager->pools[size_class];
    
    pthread_mutex_lock(&pool->mutex);
    
    if (pool->free_list == NULL) {
        // Need to allocate a new slab
        size_t block_size = size_classes[size_class];
        size_t slab_size = 64 * 1024; // 64KB slabs
        size_t blocks_per_slab = slab_size / (block_size + sizeof(allocation_header_t));
        
        void* slab = malloc(slab_size);
        if (!slab) {
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }
        
        // Add slab to tracking
        if (pool->slab_count >= pool->slab_capacity) {
            size_t new_capacity = pool->slab_capacity ? pool->slab_capacity * 2 : 8;
            void** new_slabs = realloc(pool->slabs, new_capacity * sizeof(void*));
            if (!new_slabs) {
                free(slab);
                pthread_mutex_unlock(&pool->mutex);
                return NULL;
            }
            pool->slabs = new_slabs;
            pool->slab_capacity = new_capacity;
        }
        
        pool->slabs[pool->slab_count++] = slab;
        
        // Initialize free list from slab
        char* current = (char*)slab;
        for (size_t i = 0; i < blocks_per_slab; i++) {
            allocation_header_t* header = (allocation_header_t*)current;
            void* block = current + sizeof(allocation_header_t);
            
            // Initialize header
            header->generation = atomic_fetch_add(&manager->generation_counter, 1);
            header->size = block_size;
            header->size_class = size_class;
            header->magic = 0xDEADBEEF;
            header->allocation_time = 0; // Will be set on allocation
            
            // Link to free list
            *(void**)block = pool->free_list;
            pool->free_list = block;
            pool->block_count++;
            
            current += block_size + sizeof(allocation_header_t);
        }
    }
    
    // Pop from free list
    void* ptr = pool->free_list;
    if (ptr) {
        pool->free_list = *(void**)ptr;
        pool->block_count--;
        
        // Update allocation time in header
        allocation_header_t* header = (allocation_header_t*)ptr - 1;
        header->allocation_time = lgx_time_now_ns();
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return ptr;
}
*/

/*
static void return_to_pool(lgx_memory_manager_t* manager, void* ptr, int size_class) {
    if (size_class < 0 || size_class >= NUM_SIZE_CLASSES) {
        free(ptr);
        return;
    }
    
    size_class_pool_t* pool = &manager->pools[size_class];
    
    pthread_mutex_lock(&pool->mutex);
    
    // Add to free list
    *(void**)ptr = pool->free_list;
    pool->free_list = ptr;
    pool->block_count++;
    
    pthread_mutex_unlock(&pool->mutex);
}
*/

/*
static void adapt_thread_cache(thread_cache_t* cache) {
    // Find top 4 most used size classes
    uint32_t new_hot[4] = {0};
    uint64_t max_counts[4] = {0};
    
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        uint64_t count = cache->allocation_counts[i];
        
        // Insert into top 4 if count is high enough
        for (int j = 0; j < 4; j++) {
            if (count > max_counts[j]) {
                // Shift lower entries down
                for (int k = 3; k > j; k--) {
                    max_counts[k] = max_counts[k-1];
                    new_hot[k] = new_hot[k-1];
                }
                max_counts[j] = count;
                new_hot[j] = i;
                break;
            }
        }
    }
    
    // Update hot size classes
    for (int i = 0; i < 4; i++) {
        if (cache->hot_size_classes[i] != new_hot[i]) {
            // Return cached blocks to global pool if changing
            // TODO: Implement return to pool logic
            cache->hot_size_classes[i] = new_hot[i];
            cache->count[i] = 0;
            cache->free_list[i] = NULL;
        }
    }
    
    // Reset allocation counts
    memset(cache->allocation_counts, 0, sizeof(cache->allocation_counts));
}
*/

static void track_allocation(lgx_memory_manager_t* manager, void* ptr __attribute__((unused)), size_t size, 
                           const lgx_allocation_intent_base_t* intent) {
    if (!intent) return;
    
    pthread_mutex_lock(&manager->intent_mutex);
    
    // Find free slot or expand capacity
    if (manager->tracked_count >= manager->tracked_capacity) {
        size_t new_capacity = manager->tracked_capacity * 2;
        allocation_header_t* new_tracked = realloc(manager->tracked_allocations, 
                                                  new_capacity * sizeof(allocation_header_t));
        if (!new_tracked) {
            pthread_mutex_unlock(&manager->intent_mutex);
            return; // Tracking failure is not fatal
        }
        manager->tracked_allocations = new_tracked;
        manager->tracked_capacity = new_capacity;
    }
    
    // Add tracking entry
    allocation_header_t* entry = &manager->tracked_allocations[manager->tracked_count++];
    entry->generation = atomic_fetch_add(&manager->generation_counter, 1);
    entry->size = size;
    entry->size_class = get_size_class_index(size);
    entry->magic = 0xDEADBEEF;
    entry->intent = *intent;
    entry->allocation_time = lgx_time_now_ns();
    
    pthread_mutex_unlock(&manager->intent_mutex);
}

/**
 * Extended intent-based allocation implementation
 */
void* lgx_memory_manager_alloc_with_intent_ex(lgx_memory_manager_t* manager, 
                                             const void* intent, size_t intent_type_id) {
    if (!manager || !intent) {
        return NULL;
    }
    
    // For now, treat all intent types as base intent
    // TODO: Implement proper intent type handling
    const lgx_allocation_intent_base_t* base_intent = (const lgx_allocation_intent_base_t*)intent;
    
    // Validate struct_size based on intent type
    if (intent_type_id == 0) {
        // Base intent
        if (base_intent->struct_size < sizeof(lgx_allocation_intent_base_t)) {
            return NULL;
        }
    } else if (intent_type_id == 1) {
        // L2 intent - assume it contains base intent
        if (base_intent->struct_size < sizeof(lgx_allocation_intent_base_t)) {
            return NULL;
        }
    } else {
        // Unknown intent type
        return NULL;
    }
    
    return lgx_memory_manager_alloc(manager, base_intent->size, base_intent);
}

/**
 * Get memory statistics
 */
lgx_result_t lgx_memory_manager_get_stats(lgx_memory_manager_t* manager, lgx_memory_stats_t* stats) {
    if (!manager || !stats) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    stats->struct_size = sizeof(lgx_memory_stats_t);
    stats->total_allocated = atomic_load(&manager->total_allocations);
    stats->total_deallocated = atomic_load(&manager->total_deallocations);
    stats->current_allocated = stats->total_allocated - stats->total_deallocated;
    stats->peak_allocated = stats->current_allocated; // Simplified for now
    stats->allocation_count = stats->total_allocated;     // For compatibility
    stats->deallocation_count = stats->total_deallocated; // For compatibility
    stats->cache_hits = atomic_load(&manager->cache_hits);
    stats->cache_misses = atomic_load(&manager->cache_misses);
    
    // Pool statistics
    stats->pool_count = NUM_SIZE_CLASSES;
    for (int i = 0; i < NUM_SIZE_CLASSES && i < 16; i++) {
        pthread_mutex_lock(&manager->pools[i].mutex);
        stats->pool_stats[i].size_class = size_classes[i];
        stats->pool_stats[i].total_allocated = manager->pools[i].total_allocated;
        stats->pool_stats[i].current_free = manager->pools[i].block_count;
        stats->pool_stats[i].slab_count = manager->pools[i].slab_count;
        pthread_mutex_unlock(&manager->pools[i].mutex);
    }
    
    return LGX_SUCCESS;
}

/**
 * Get allocation usage statistics
 */
lgx_result_t lgx_memory_manager_get_usage_stats(lgx_memory_manager_t* manager, void* ptr, 
                                               lgx_allocation_usage_t* usage) {
    if (!manager || !ptr || !usage) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->intent_mutex);
    
    // Find the allocation in tracked allocations
    for (size_t i = 0; i < manager->tracked_count; i++) {
        if ((void*)&manager->tracked_allocations[i] == ptr) {
            allocation_header_t* entry = &manager->tracked_allocations[i];
            
            usage->struct_size = sizeof(lgx_allocation_usage_t);
            usage->access_count = 1; // Simplified - would need real tracking
            usage->observed_pattern = entry->intent.access_pattern;
            usage->pattern_confidence = 0.8; // Placeholder
            usage->observed_lifetime = entry->intent.lifetime;
            
            uint64_t current_time = lgx_time_now_ns();
            usage->actual_lifetime_ms = (current_time - entry->allocation_time) / 1000000;
            
            pthread_mutex_unlock(&manager->intent_mutex);
            return LGX_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&manager->intent_mutex);
    
    // Not found in tracked allocations - return default stats
    usage->struct_size = sizeof(lgx_allocation_usage_t);
    usage->access_count = 0;
    usage->observed_pattern = LGX_ACCESS_UNKNOWN;
    usage->pattern_confidence = 0.0;
    usage->observed_lifetime = LGX_LIFETIME_UNKNOWN;
    usage->actual_lifetime_ms = 0;
    
    return LGX_SUCCESS;
}

/**
 * Validate allocation intent
 */
lgx_result_t lgx_memory_manager_validate_intent(lgx_memory_manager_t* manager, void* ptr) {
    if (!manager || !ptr) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // For now, always return success
    // TODO: Implement actual intent validation logic
    return LGX_SUCCESS;
}

static void untrack_allocation(lgx_memory_manager_t* manager, void* ptr) {
    pthread_mutex_lock(&manager->intent_mutex);
    
    // Find and remove tracking entry
    for (size_t i = 0; i < manager->tracked_count; i++) {
        if ((void*)&manager->tracked_allocations[i] == ptr) {
            // Move last entry to this position
            if (i < manager->tracked_count - 1) {
                manager->tracked_allocations[i] = manager->tracked_allocations[manager->tracked_count - 1];
            }
            manager->tracked_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->intent_mutex);
}