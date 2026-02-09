/**
 * LGX Memory Safety - Security Hardening
 * 
 * Provides memory safety features to detect and prevent memory corruption:
 * - Guard pages (debug builds)
 * - Memory canaries
 * - Delayed reclamation (3-frame)
 * - Allocation tracking (double-free prevention)
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>

// Memory canary values (random-looking patterns)
#define LGX_CANARY_PREFIX   0xDEADBEEFCAFEBABEULL
#define LGX_CANARY_SUFFIX   0xFEEDFACEDEADC0DEULL
#define LGX_FREED_PATTERN   0xFEFEFEFEFEFEFEFEULL

// Delayed reclamation settings
#define LGX_DELAYED_FRAMES  3
#define LGX_MAX_DELAYED_ALLOCS 10000

// Allocation metadata (stored before user data)
typedef struct lgx_alloc_metadata {
    uint64_t canary_prefix;
    size_t size;
    uint32_t frame_allocated;
    uint32_t frame_freed;
    void* user_ptr;
    struct lgx_alloc_metadata* next;  // For tracking list
    uint64_t canary_suffix;
} lgx_alloc_metadata_t;

// Delayed reclamation queue
typedef struct lgx_delayed_free {
    void* ptr;
    uint32_t frame_freed;
    struct lgx_delayed_free* next;
} lgx_delayed_free_t;

// Memory safety state
typedef struct {
    pthread_mutex_t mutex;
    
    // Allocation tracking
    lgx_alloc_metadata_t* active_allocations;
    size_t active_count;
    
    // Delayed reclamation
    lgx_delayed_free_t* delayed_queue;
    size_t delayed_count;
    uint32_t current_frame;
    
    // Statistics
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t canary_violations;
    uint64_t double_free_attempts;
    uint64_t use_after_free_attempts;
    
    // Configuration
    bool guard_pages_enabled;
    bool canaries_enabled;
    bool delayed_reclamation_enabled;
    bool tracking_enabled;
} lgx_memory_safety_state_t;

static lgx_memory_safety_state_t g_safety_state = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .active_allocations = NULL,
    .active_count = 0,
    .delayed_queue = NULL,
    .delayed_count = 0,
    .current_frame = 0,
    .total_allocations = 0,
    .total_frees = 0,
    .canary_violations = 0,
    .double_free_attempts = 0,
    .use_after_free_attempts = 0,
#ifdef DEBUG
    .guard_pages_enabled = true,
    .canaries_enabled = true,
    .delayed_reclamation_enabled = true,
    .tracking_enabled = true,
#else
    .guard_pages_enabled = false,
    .canaries_enabled = false,
    .delayed_reclamation_enabled = false,
    .tracking_enabled = false,
#endif
};

/**
 * Initialize memory safety system
 */
lgx_result_t lgx_memory_safety_init(void) {
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "Initializing memory safety features...");
    
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Guard pages: %s", g_safety_state.guard_pages_enabled ? "enabled" : "disabled");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Canaries: %s", g_safety_state.canaries_enabled ? "enabled" : "disabled");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Delayed reclamation: %s", g_safety_state.delayed_reclamation_enabled ? "enabled" : "disabled");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Allocation tracking: %s", g_safety_state.tracking_enabled ? "enabled" : "disabled");
    
    return LGX_SUCCESS;
}

/**
 * Shutdown memory safety system
 */
lgx_result_t lgx_memory_safety_shutdown(void) {
    pthread_mutex_lock(&g_safety_state.mutex);
    
    // Report statistics
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "Memory safety statistics:");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Total allocations: %lu", g_safety_state.total_allocations);
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Total frees: %lu", g_safety_state.total_frees);
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Canary violations: %lu", g_safety_state.canary_violations);
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Double-free attempts: %lu", g_safety_state.double_free_attempts);
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO,
                  "  Use-after-free attempts: %lu", g_safety_state.use_after_free_attempts);
    
    // Check for memory leaks
    if (g_safety_state.active_count > 0) {
        lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_WARN,
                      "Memory leak detected: %zu allocations not freed", g_safety_state.active_count);
    }
    
    // Free delayed reclamation queue
    lgx_delayed_free_t* delayed = g_safety_state.delayed_queue;
    while (delayed) {
        lgx_delayed_free_t* next = delayed->next;
        free(delayed);
        delayed = next;
    }
    
    pthread_mutex_unlock(&g_safety_state.mutex);
    
    return LGX_SUCCESS;
}

/**
 * Check canaries for corruption
 */
static bool check_canaries(lgx_alloc_metadata_t* metadata) {
    if (!g_safety_state.canaries_enabled) {
        return true;
    }
    
    // Check prefix canary
    if (metadata->canary_prefix != LGX_CANARY_PREFIX) {
        lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_ERROR,
                      "Canary corruption detected: prefix canary mismatch (ptr=%p, size=%zu)",
                      metadata->user_ptr, metadata->size);
        g_safety_state.canary_violations++;
        return false;
    }
    
    // Check suffix canary
    if (metadata->canary_suffix != LGX_CANARY_SUFFIX) {
        lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_ERROR,
                      "Canary corruption detected: suffix canary mismatch (ptr=%p, size=%zu)",
                      metadata->user_ptr, metadata->size);
        g_safety_state.canary_violations++;
        return false;
    }
    
    // Check suffix canary after user data
    uint64_t* suffix_canary = (uint64_t*)((uint8_t*)metadata->user_ptr + metadata->size);
    if (*suffix_canary != LGX_CANARY_SUFFIX) {
        lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_ERROR,
                      "Buffer overflow detected: suffix canary after data corrupted (ptr=%p, size=%zu)",
                      metadata->user_ptr, metadata->size);
        g_safety_state.canary_violations++;
        return false;
    }
    
    return true;
}

/**
 * Add allocation to tracking list
 */
static void track_allocation(lgx_alloc_metadata_t* metadata) {
    if (!g_safety_state.tracking_enabled) {
        return;
    }
    
    pthread_mutex_lock(&g_safety_state.mutex);
    
    metadata->next = g_safety_state.active_allocations;
    g_safety_state.active_allocations = metadata;
    g_safety_state.active_count++;
    g_safety_state.total_allocations++;
    
    pthread_mutex_unlock(&g_safety_state.mutex);
}

/**
 * Remove allocation from tracking list
 */
static bool untrack_allocation(void* user_ptr, lgx_alloc_metadata_t** out_metadata) {
    if (!g_safety_state.tracking_enabled) {
        return true;
    }
    
    pthread_mutex_lock(&g_safety_state.mutex);
    
    lgx_alloc_metadata_t** current = &g_safety_state.active_allocations;
    while (*current) {
        if ((*current)->user_ptr == user_ptr) {
            *out_metadata = *current;
            *current = (*current)->next;
            g_safety_state.active_count--;
            g_safety_state.total_frees++;
            pthread_mutex_unlock(&g_safety_state.mutex);
            return true;
        }
        current = &(*current)->next;
    }
    
    pthread_mutex_unlock(&g_safety_state.mutex);
    
    // Not found - double free attempt
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_ERROR,
                  "Double-free detected: ptr=%p not in active allocations", user_ptr);
    g_safety_state.double_free_attempts++;
    return false;
}

/**
 * Allocate memory with safety features
 */
void* lgx_memory_safety_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Calculate total size with metadata and canaries
    size_t metadata_size = sizeof(lgx_alloc_metadata_t);
    size_t canary_size = g_safety_state.canaries_enabled ? sizeof(uint64_t) : 0;
    size_t guard_page_size = g_safety_state.guard_pages_enabled ? (size_t)sysconf(_SC_PAGESIZE) : 0;
    size_t total_size = metadata_size + size + canary_size + guard_page_size;
    
    // Allocate memory
    void* raw_ptr = malloc(total_size);
    if (!raw_ptr) {
        return NULL;
    }
    
    // Initialize metadata
    lgx_alloc_metadata_t* metadata = (lgx_alloc_metadata_t*)raw_ptr;
    metadata->canary_prefix = LGX_CANARY_PREFIX;
    metadata->size = size;
    metadata->frame_allocated = g_safety_state.current_frame;
    metadata->frame_freed = 0;
    metadata->user_ptr = (uint8_t*)raw_ptr + metadata_size;
    metadata->next = NULL;
    metadata->canary_suffix = LGX_CANARY_SUFFIX;
    
    // Add suffix canary after user data
    if (g_safety_state.canaries_enabled) {
        uint64_t* suffix_canary = (uint64_t*)((uint8_t*)metadata->user_ptr + size);
        *suffix_canary = LGX_CANARY_SUFFIX;
    }
    
    // Set up guard page (if enabled)
    if (g_safety_state.guard_pages_enabled && guard_page_size > 0) {
        void* guard_page = (uint8_t*)metadata->user_ptr + size + canary_size;
        mprotect(guard_page, guard_page_size, PROT_NONE);
    }
    
    // Track allocation
    track_allocation(metadata);
    
    return metadata->user_ptr;
}

/**
 * Free memory with safety features
 */
void lgx_memory_safety_free(void* user_ptr) {
    if (!user_ptr) {
        return;
    }
    
    // Get metadata
    lgx_alloc_metadata_t* metadata = (lgx_alloc_metadata_t*)((uint8_t*)user_ptr - sizeof(lgx_alloc_metadata_t));
    
    // Check canaries
    if (!check_canaries(metadata)) {
        // Canary violation - don't free to preserve evidence
        return;
    }
    
    // Untrack allocation
    lgx_alloc_metadata_t* tracked_metadata = NULL;
    if (!untrack_allocation(user_ptr, &tracked_metadata)) {
        // Double-free detected
        return;
    }
    
    // Mark as freed
    metadata->frame_freed = g_safety_state.current_frame;
    
    // Fill with freed pattern (helps detect use-after-free)
    memset(user_ptr, 0xFE, metadata->size);
    
    // Delayed reclamation (if enabled)
    if (g_safety_state.delayed_reclamation_enabled) {
        pthread_mutex_lock(&g_safety_state.mutex);
        
        if (g_safety_state.delayed_count < LGX_MAX_DELAYED_ALLOCS) {
            lgx_delayed_free_t* delayed = malloc(sizeof(lgx_delayed_free_t));
            if (delayed) {
                delayed->ptr = metadata;
                delayed->frame_freed = g_safety_state.current_frame;
                delayed->next = g_safety_state.delayed_queue;
                g_safety_state.delayed_queue = delayed;
                g_safety_state.delayed_count++;
                pthread_mutex_unlock(&g_safety_state.mutex);
                return;
            }
        }
        
        pthread_mutex_unlock(&g_safety_state.mutex);
    }
    
    // Free immediately if delayed reclamation disabled or queue full
    free(metadata);
}

/**
 * Advance frame counter and process delayed frees
 */
void lgx_memory_safety_advance_frame(void) {
    pthread_mutex_lock(&g_safety_state.mutex);
    
    g_safety_state.current_frame++;
    
    // Process delayed frees
    if (g_safety_state.delayed_reclamation_enabled) {
        lgx_delayed_free_t** current = &g_safety_state.delayed_queue;
        while (*current) {
            lgx_delayed_free_t* delayed = *current;
            
            // Free if enough frames have passed
            if (g_safety_state.current_frame - delayed->frame_freed >= LGX_DELAYED_FRAMES) {
                *current = delayed->next;
                free(delayed->ptr);
                free(delayed);
                g_safety_state.delayed_count--;
            } else {
                current = &delayed->next;
            }
        }
    }
    
    pthread_mutex_unlock(&g_safety_state.mutex);
}

/**
 * Get memory safety statistics
 */
void lgx_memory_safety_get_stats(lgx_memory_safety_stats_t* stats) {
    if (!stats) {
        return;
    }
    
    pthread_mutex_lock(&g_safety_state.mutex);
    
    stats->total_allocations = g_safety_state.total_allocations;
    stats->total_frees = g_safety_state.total_frees;
    stats->active_allocations = g_safety_state.active_count;
    stats->delayed_frees = g_safety_state.delayed_count;
    stats->canary_violations = g_safety_state.canary_violations;
    stats->double_free_attempts = g_safety_state.double_free_attempts;
    stats->use_after_free_attempts = g_safety_state.use_after_free_attempts;
    stats->current_frame = g_safety_state.current_frame;
    
    pthread_mutex_unlock(&g_safety_state.mutex);
}
