/**
 * LGX Huge Pages Support - Day 10 Breakthrough Optimization
 * 
 * Implements 2MB huge page allocation to reduce TLB misses and improve P99 latency.
 * 
 * Key Features:
 * - Transparent huge page support with graceful fallback
 * - Selective huge page usage for hot path caches
 * - TLB miss reduction for improved memory access latency
 * - Performance monitoring and statistics
 */

#define _GNU_SOURCE
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

// Huge page configuration
#define HUGEPAGE_SIZE (2 * 1024 * 1024)  // 2MB
#define HUGEPAGE_SHIFT 21                 // log2(2MB)
#define REGULAR_PAGE_SIZE 4096            // 4KB

// Huge page allocation strategy
typedef enum {
    HUGEPAGE_STRATEGY_ALWAYS,      // Always try huge pages
    HUGEPAGE_STRATEGY_SELECTIVE,   // Only for large, long-lived allocations
    HUGEPAGE_STRATEGY_NEVER,       // Never use huge pages (fallback mode)
} hugepage_strategy_t;

// Huge page statistics
typedef struct {
    uint64_t hugepage_allocations;
    uint64_t hugepage_allocation_failures;
    uint64_t regular_page_fallbacks;
    uint64_t total_hugepage_bytes;
    uint64_t tlb_miss_estimate_before;
    uint64_t tlb_miss_estimate_after;
} hugepage_stats_t;

// Global huge page state
static struct {
    bool initialized;
    bool available;
    hugepage_strategy_t strategy;
    hugepage_stats_t stats;
    int hugetlbfs_fd;  // File descriptor for hugetlbfs mount (if available)
} g_hugepage_state = {
    .initialized = false,
    .available = false,
    .strategy = HUGEPAGE_STRATEGY_SELECTIVE,
    .stats = {0},
    .hugetlbfs_fd = -1,
};

// Forward declarations
static bool detect_hugepage_support(void);
static bool check_transparent_hugepages(void);
static int find_hugetlbfs_mount(char* mount_path, size_t path_size);

/**
 * Initialize huge page support
 */
lgx_result_t lgx_hugepages_init(lgx_hardware_adapter_t* hardware_adapter) {
    if (g_hugepage_state.initialized) {
        return LGX_SUCCESS;
    }
    
    // Detect if huge pages are available
    g_hugepage_state.available = detect_hugepage_support();
    
    // If hardware adapter reports huge pages unavailable, respect that
    if (hardware_adapter && !lgx_hardware_adapter_has_huge_pages(hardware_adapter)) {
        g_hugepage_state.available = false;
        g_hugepage_state.strategy = HUGEPAGE_STRATEGY_NEVER;
    }
    
    // Try to find hugetlbfs mount point
    char mount_path[256];
    if (g_hugepage_state.available && find_hugetlbfs_mount(mount_path, sizeof(mount_path)) == 0) {
        // Try to open hugetlbfs for allocation
        g_hugepage_state.hugetlbfs_fd = open(mount_path, O_RDWR);
        if (g_hugepage_state.hugetlbfs_fd < 0) {
            // Can't open hugetlbfs - will use mmap with MAP_HUGETLB instead
            g_hugepage_state.hugetlbfs_fd = -1;
        }
    }
    
    // Initialize statistics
    memset(&g_hugepage_state.stats, 0, sizeof(hugepage_stats_t));
    
    g_hugepage_state.initialized = true;
    
    return LGX_SUCCESS;
}

/**
 * Shutdown huge page support
 */
lgx_result_t lgx_hugepages_shutdown(void) {
    if (!g_hugepage_state.initialized) {
        return LGX_SUCCESS;
    }
    
    // Close hugetlbfs file descriptor if open
    if (g_hugepage_state.hugetlbfs_fd >= 0) {
        close(g_hugepage_state.hugetlbfs_fd);
        g_hugepage_state.hugetlbfs_fd = -1;
    }
    
    g_hugepage_state.initialized = false;
    
    return LGX_SUCCESS;
}

/**
 * Allocate memory using huge pages
 * 
 * @param size Size to allocate (will be rounded up to huge page boundary)
 * @param flags Allocation flags (e.g., MAP_PRIVATE, MAP_ANONYMOUS)
 * @return Pointer to allocated memory, or NULL on failure
 */
void* lgx_hugepages_alloc(size_t size) {
    if (!g_hugepage_state.initialized || !g_hugepage_state.available) {
        return NULL;
    }
    
    // Round size up to huge page boundary
    size_t aligned_size = (size + HUGEPAGE_SIZE - 1) & ~(HUGEPAGE_SIZE - 1);
    
    // Try to allocate with MAP_HUGETLB
    void* ptr = mmap(NULL, aligned_size, 
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                     -1, 0);
    
    if (ptr != MAP_FAILED) {
        // Success!
        g_hugepage_state.stats.hugepage_allocations++;
        g_hugepage_state.stats.total_hugepage_bytes += aligned_size;
        
        // Advise kernel to use huge pages (for transparent huge pages)
        madvise(ptr, aligned_size, MADV_HUGEPAGE);
        
        return ptr;
    }
    
    // Huge page allocation failed
    g_hugepage_state.stats.hugepage_allocation_failures++;
    
    // Try transparent huge pages as fallback
    ptr = mmap(NULL, aligned_size,
               PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS,
               -1, 0);
    
    if (ptr != MAP_FAILED) {
        // Advise kernel to use transparent huge pages
        madvise(ptr, aligned_size, MADV_HUGEPAGE);
        g_hugepage_state.stats.regular_page_fallbacks++;
        return ptr;
    }
    
    return NULL;
}

/**
 * Free memory allocated with huge pages
 */
void lgx_hugepages_free(void* ptr, size_t size) {
    if (!ptr) {
        return;
    }
    
    // Round size up to huge page boundary
    size_t aligned_size = (size + HUGEPAGE_SIZE - 1) & ~(HUGEPAGE_SIZE - 1);
    
    munmap(ptr, aligned_size);
}

/**
 * Allocate memory with selective huge page usage
 * 
 * Uses huge pages only for allocations that benefit from them:
 * - Large allocations (>= 2MB)
 * - Long-lived allocations
 * - Hot path caches
 */
void* lgx_hugepages_alloc_selective(size_t size, bool is_long_lived, bool is_hot_path) {
    if (!g_hugepage_state.initialized || !g_hugepage_state.available) {
        return NULL;
    }
    
    // Selective strategy: only use huge pages for beneficial cases
    if (g_hugepage_state.strategy == HUGEPAGE_STRATEGY_SELECTIVE) {
        // Use huge pages if:
        // 1. Size >= 2MB (full huge page utilization)
        // 2. Long-lived AND hot path (reduces TLB pressure)
        // 3. Size >= 512KB AND (long-lived OR hot path)
        
        bool should_use_hugepages = false;
        
        if (size >= HUGEPAGE_SIZE) {
            should_use_hugepages = true;
        } else if (size >= 512 * 1024 && (is_long_lived || is_hot_path)) {
            should_use_hugepages = true;
        } else if (is_long_lived && is_hot_path) {
            should_use_hugepages = true;
        }
        
        if (!should_use_hugepages) {
            return NULL;  // Caller should use regular allocation
        }
    } else if (g_hugepage_state.strategy == HUGEPAGE_STRATEGY_NEVER) {
        return NULL;
    }
    
    // Try huge page allocation
    return lgx_hugepages_alloc(size);
}

/**
 * Check if huge pages are available
 */
bool lgx_hugepages_available(void) {
    return g_hugepage_state.initialized && g_hugepage_state.available;
}

/**
 * Get huge page statistics
 */
static void __attribute__((unused)) lgx_hugepages_get_stats(hugepage_stats_t* stats) {
    if (stats) {
        *stats = g_hugepage_state.stats;
    }
}

/**
 * Set huge page allocation strategy
 */
static void __attribute__((unused)) lgx_hugepages_set_strategy(hugepage_strategy_t strategy) {
    g_hugepage_state.strategy = strategy;
}

// Private implementation functions

/**
 * Detect if huge pages are supported on this system
 */
static bool detect_hugepage_support(void) {
    // Check /proc/meminfo for huge page support
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) {
        return false;
    }
    
    char line[256];
    bool has_hugepages = false;
    int total_pages = 0;
    
    while (fgets(line, sizeof(line), meminfo)) {
        if (strncmp(line, "HugePages_Total:", 16) == 0) {
            if (sscanf(line, "HugePages_Total: %d", &total_pages) == 1) {
                has_hugepages = (total_pages > 0);
            }
            break;
        }
    }
    
    fclose(meminfo);
    
    // If no pre-allocated huge pages, check for transparent huge pages
    if (!has_hugepages) {
        has_hugepages = check_transparent_hugepages();
    }
    
    return has_hugepages;
}

/**
 * Check if transparent huge pages are enabled
 */
static bool check_transparent_hugepages(void) {
    FILE* thp_enabled = fopen("/sys/kernel/mm/transparent_hugepage/enabled", "r");
    if (!thp_enabled) {
        return false;
    }
    
    char line[256];
    bool enabled = false;
    
    if (fgets(line, sizeof(line), thp_enabled)) {
        // Check if "always" or "madvise" is enabled
        // Format: "always [madvise] never" or "[always] madvise never"
        if (strstr(line, "[always]") || strstr(line, "[madvise]")) {
            enabled = true;
        }
    }
    
    fclose(thp_enabled);
    return enabled;
}

/**
 * Find hugetlbfs mount point
 */
static int find_hugetlbfs_mount(char* mount_path, size_t path_size) {
    FILE* mounts = fopen("/proc/mounts", "r");
    if (!mounts) {
        return -1;
    }
    
    char line[512];
    int found = -1;
    
    while (fgets(line, sizeof(line), mounts)) {
        char device[256], path[256], fstype[64];
        if (sscanf(line, "%s %s %s", device, path, fstype) == 3) {
            if (strcmp(fstype, "hugetlbfs") == 0) {
                size_t len = strlen(path);
                if (len >= path_size) {
                    len = path_size - 1;
                }
                memcpy(mount_path, path, len);
                mount_path[len] = '\0';
                found = 0;
                break;
            }
        }
    }
    
    fclose(mounts);
    return found;
}

/**
 * Estimate TLB miss reduction from huge pages
 * 
 * This is a rough estimate based on the number of pages accessed.
 * With 4KB pages: 512 pages per 2MB
 * With 2MB pages: 1 page per 2MB
 * 
 * TLB miss reduction = (512 - 1) / 512 = 99.8%
 */
double lgx_hugepages_estimate_tlb_improvement(size_t memory_size) {
    if (memory_size < HUGEPAGE_SIZE) {
        return 0.0;  // No benefit for small allocations
    }
    
    // Calculate number of regular pages
    size_t regular_pages = (memory_size + REGULAR_PAGE_SIZE - 1) / REGULAR_PAGE_SIZE;
    
    // Calculate number of huge pages
    size_t huge_pages = (memory_size + HUGEPAGE_SIZE - 1) / HUGEPAGE_SIZE;
    
    // TLB miss reduction percentage
    double reduction = 1.0 - ((double)huge_pages / (double)regular_pages);
    
    return reduction * 100.0;  // Return as percentage
}
