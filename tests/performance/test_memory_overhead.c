#include "../../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

// Get memory stats from /proc/self/status
static void get_memory_stats(size_t* vm_size, size_t* vm_rss, size_t* vm_data) {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) {
        *vm_size = 0;
        *vm_rss = 0;
        *vm_data = 0;
        return;
    }
    
    char line[256];
    *vm_size = 0;
    *vm_rss = 0;
    *vm_data = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%zu", vm_size);
            *vm_size *= 1024;  // Convert KB to bytes
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%zu", vm_rss);
            *vm_rss *= 1024;
        } else if (strncmp(line, "VmData:", 7) == 0) {
            sscanf(line + 7, "%zu", vm_data);
            *vm_data *= 1024;
        }
    }
    
    fclose(f);
}

int main(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused
    
    printf("=== LGX Runtime Memory Overhead Measurement ===\n\n");
    
    // Measure baseline memory before initialization
    size_t baseline_vm, baseline_rss, baseline_data;
    get_memory_stats(&baseline_vm, &baseline_rss, &baseline_data);
    
    printf("Baseline memory (before init):\n");
    printf("  VmSize: %8.2f MB\n", baseline_vm / (1024.0 * 1024.0));
    printf("  VmRSS:  %8.2f MB\n", baseline_rss / (1024.0 * 1024.0));
    printf("  VmData: %8.2f MB\n\n", baseline_data / (1024.0 * 1024.0));
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Measure memory after initialization
    size_t init_vm, init_rss, init_data;
    get_memory_stats(&init_vm, &init_rss, &init_data);
    
    size_t init_overhead_vm = init_vm - baseline_vm;
    size_t init_overhead_rss = init_rss - baseline_rss;
    size_t init_overhead_data = init_data - baseline_data;
    
    printf("Memory after initialization:\n");
    printf("  VmSize: %8.2f MB (+%.2f MB)\n", 
           init_vm / (1024.0 * 1024.0),
           init_overhead_vm / (1024.0 * 1024.0));
    printf("  VmRSS:  %8.2f MB (+%.2f MB)\n",
           init_rss / (1024.0 * 1024.0),
           init_overhead_rss / (1024.0 * 1024.0));
    printf("  VmData: %8.2f MB (+%.2f MB)\n\n",
           init_data / (1024.0 * 1024.0),
           init_overhead_data / (1024.0 * 1024.0));
    
    // Test memory overhead with allocations
    printf("Testing memory overhead with allocations...\n\n");
    
    const int num_allocations = 10000;
    const size_t alloc_size = 1024;  // 1KB each
    const size_t expected_usage = num_allocations * alloc_size;
    
    void** ptrs = malloc(sizeof(void*) * num_allocations);
    assert(ptrs != NULL);
    
    // Allocate memory
    for (int i = 0; i < num_allocations; i++) {
        ptrs[i] = lgx_alloc(alloc_size);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], 0xAB, alloc_size);  // Touch memory to ensure RSS
    }
    
    // Measure memory after allocations
    size_t alloc_vm, alloc_rss, alloc_data;
    get_memory_stats(&alloc_vm, &alloc_rss, &alloc_data);
    
    size_t alloc_overhead_vm = alloc_vm - init_vm;
    size_t alloc_overhead_rss = alloc_rss - init_rss;
    
    printf("After %d allocations of %zu bytes:\n", num_allocations, alloc_size);
    printf("  Expected usage: %.2f MB\n", expected_usage / (1024.0 * 1024.0));
    printf("  Actual VmSize:  %.2f MB (+%.2f MB from init)\n",
           alloc_vm / (1024.0 * 1024.0),
           alloc_overhead_vm / (1024.0 * 1024.0));
    printf("  Actual VmRSS:   %.2f MB (+%.2f MB from init)\n",
           alloc_rss / (1024.0 * 1024.0),
           alloc_overhead_rss / (1024.0 * 1024.0));
    
    // Calculate overhead percentage
    double overhead_percent = 0.0;
    if (expected_usage > 0) {
        overhead_percent = ((double)alloc_overhead_rss - expected_usage) * 100.0 / expected_usage;
    }
    
    printf("  Memory overhead: %.2f%%\n\n", overhead_percent);
    
    // Get runtime memory stats
    lgx_memory_stats_t mem_stats;
    result = lgx_memory_stats(&mem_stats);
    assert(result == LGX_SUCCESS);
    
    printf("Runtime memory statistics:\n");
    printf("  Total allocated:   %8.2f MB\n", mem_stats.total_allocated / (1024.0 * 1024.0));
    printf("  Peak allocated:    %8.2f MB\n", mem_stats.peak_allocated / (1024.0 * 1024.0));
    printf("  Current allocated: %8.2f MB\n", mem_stats.current_allocated / (1024.0 * 1024.0));
    printf("  Allocation count:  %lu\n", mem_stats.allocation_count);
    printf("  Deallocation count: %lu\n\n", mem_stats.deallocation_count);
    
    // Free allocations
    for (int i = 0; i < num_allocations; i++) {
        lgx_free(ptrs[i]);
    }
    free(ptrs);
    
    // Measure memory after freeing
    size_t free_vm, free_rss, free_data;
    get_memory_stats(&free_vm, &free_rss, &free_data);
    
    printf("After freeing all allocations:\n");
    printf("  VmSize: %8.2f MB\n", free_vm / (1024.0 * 1024.0));
    printf("  VmRSS:  %8.2f MB\n", free_rss / (1024.0 * 1024.0));
    printf("  VmData: %8.2f MB\n\n", free_data / (1024.0 * 1024.0));
    
    // Performance target validation
    printf("=== Performance Target Validation ===\n");
    const double tier1_target_mb = 300.0;
    const double tier2_target_mb = 200.0;
    
    printf("Tier 1 Target: < 300MB initialization overhead\n");
    printf("Tier 2 Target: < 200MB initialization overhead\n\n");
    
    double init_overhead_mb = init_overhead_rss / (1024.0 * 1024.0);
    
    int passed = 0;
    if (init_overhead_mb < tier2_target_mb) {
        printf("✅ PASSED Tier 2: Init overhead = %.2f MB < 200MB\n", init_overhead_mb);
        passed = 2;
    } else if (init_overhead_mb < tier1_target_mb) {
        printf("✅ PASSED Tier 1: Init overhead = %.2f MB < 300MB\n", init_overhead_mb);
        printf("⚠️  MISSED Tier 2: Init overhead = %.2f MB >= 200MB\n", init_overhead_mb);
        passed = 1;
    } else {
        printf("❌ FAILED: Init overhead = %.2f MB >= 300MB\n", init_overhead_mb);
        passed = 0;
    }
    
    // Check allocation overhead
    printf("\nAllocation overhead target: < 20%%\n");
    if (overhead_percent < 20.0) {
        printf("✅ PASSED: Allocation overhead = %.2f%% < 20%%\n", overhead_percent);
    } else {
        printf("❌ FAILED: Allocation overhead = %.2f%% >= 20%%\n", overhead_percent);
        if (passed > 0) passed = 1;  // Downgrade if allocation overhead fails
    }
    
    // Export results for regression detection
    FILE* results_file = fopen("benchmark_results_memory.txt", "w");
    if (results_file) {
        fprintf(results_file, "benchmark=memory_overhead\n");
        fprintf(results_file, "baseline_vm_bytes=%zu\n", baseline_vm);
        fprintf(results_file, "baseline_rss_bytes=%zu\n", baseline_rss);
        fprintf(results_file, "init_overhead_vm_bytes=%zu\n", init_overhead_vm);
        fprintf(results_file, "init_overhead_rss_bytes=%zu\n", init_overhead_rss);
        fprintf(results_file, "init_overhead_data_bytes=%zu\n", init_overhead_data);
        fprintf(results_file, "alloc_overhead_vm_bytes=%zu\n", alloc_overhead_vm);
        fprintf(results_file, "alloc_overhead_rss_bytes=%zu\n", alloc_overhead_rss);
        fprintf(results_file, "alloc_overhead_percent=%.2f\n", overhead_percent);
        fprintf(results_file, "peak_allocated_bytes=%zu\n", mem_stats.peak_allocated);
        fprintf(results_file, "tier_passed=%d\n", passed);
        fclose(results_file);
        printf("\n✅ Results exported to benchmark_results_memory.txt\n");
    }
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return (passed > 0) ? 0 : 1;
}
