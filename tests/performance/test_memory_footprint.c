#include "../../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    printf("=== LGX Runtime Memory Footprint Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    printf("Initializing runtime...\n");
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Get memory usage
    lgx_memory_usage_t usage;
    result = lgx_get_memory_usage(&usage);
    assert(result == LGX_SUCCESS);
    
    printf("\n=== Memory Usage After Initialization ===\n\n");
    printf("RSS (Resident Set Size):\n");
    printf("  Baseline:  %8.2f MB\n", usage.baseline_rss_bytes / (1024.0 * 1024.0));
    printf("  Current:   %8.2f MB\n", usage.rss_bytes / (1024.0 * 1024.0));
    printf("  Peak:      %8.2f MB\n", usage.peak_rss_bytes / (1024.0 * 1024.0));
    printf("\n");
    printf("Runtime Overhead:\n");
    printf("  Current:   %8.2f MB\n", usage.overhead_bytes / (1024.0 * 1024.0));
    printf("  Peak:      %8.2f MB\n", usage.peak_overhead_bytes / (1024.0 * 1024.0));
    printf("\n");
    printf("Per-Allocator Usage:\n");
    printf("  Frame Arena:\n");
    printf("    Current: %8.2f MB\n", usage.frame_arena_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.frame_arena_peak_bytes / (1024.0 * 1024.0));
    printf("  GPU Pool:\n");
    printf("    Current: %8.2f MB\n", usage.gpu_pool_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.gpu_pool_peak_bytes / (1024.0 * 1024.0));
    printf("  Persistent Heap:\n");
    printf("    Current: %8.2f MB\n", usage.persistent_heap_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.persistent_heap_peak_bytes / (1024.0 * 1024.0));
    printf("\n");
    
    // Perform some allocations to see memory growth
    printf("Performing test allocations...\n");
    
    // Frame allocations (should be minimal overhead)
    for (int i = 0; i < 1000; i++) {
        void* ptr = lgx_alloc_frame(1024);
        assert(ptr != NULL);
        (void)ptr;  // Suppress unused warning
    }
    
    // Persistent allocations
    void* persistent_ptrs[100];
    for (int i = 0; i < 100; i++) {
        persistent_ptrs[i] = lgx_alloc_persistent(4096);
        assert(persistent_ptrs[i] != NULL);
    }
    
    // Get updated memory usage
    result = lgx_get_memory_usage(&usage);
    assert(result == LGX_SUCCESS);
    
    printf("\n=== Memory Usage After Allocations ===\n\n");
    printf("Runtime Overhead:\n");
    printf("  Current:   %8.2f MB\n", usage.overhead_bytes / (1024.0 * 1024.0));
    printf("  Peak:      %8.2f MB\n", usage.peak_overhead_bytes / (1024.0 * 1024.0));
    printf("\n");
    printf("Per-Allocator Usage:\n");
    printf("  Frame Arena:\n");
    printf("    Current: %8.2f MB\n", usage.frame_arena_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.frame_arena_peak_bytes / (1024.0 * 1024.0));
    printf("  Persistent Heap:\n");
    printf("    Current: %8.2f MB\n", usage.persistent_heap_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.persistent_heap_peak_bytes / (1024.0 * 1024.0));
    printf("\n");
    
    // Free persistent allocations
    for (int i = 0; i < 100; i++) {
        lgx_free(persistent_ptrs[i]);
    }
    
    // Check targets
    printf("=== Target Validation ===\n\n");
    printf("Tier 1 Target: <300MB\n");
    printf("Tier 2 Target: <200MB\n\n");
    
    int passed = 0;
    if (usage.peak_overhead_bytes < 200 * 1024 * 1024) {
        printf("✅ PASSED Tier 2: %.2f MB < 200MB\n", 
               usage.peak_overhead_bytes / (1024.0 * 1024.0));
        passed = 2;
    } else if (usage.peak_overhead_bytes < 300 * 1024 * 1024) {
        printf("✅ PASSED Tier 1: %.2f MB < 300MB\n", 
               usage.peak_overhead_bytes / (1024.0 * 1024.0));
        printf("⚠️  MISSED Tier 2: %.2f MB >= 200MB\n", 
               usage.peak_overhead_bytes / (1024.0 * 1024.0));
        passed = 1;
    } else {
        printf("❌ FAILED: %.2f MB >= 300MB\n", 
               usage.peak_overhead_bytes / (1024.0 * 1024.0));
        passed = 0;
    }
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return (passed > 0) ? 0 : 1;
}
