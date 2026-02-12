#include "../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

// Performance test for initialization time measurement
int main() {
    printf("LGX Runtime Performance Test\n");
    printf("============================\n\n");
    
    // Test multiple initialization cycles to get statistical data
    #define NUM_PERF_TESTS 10
    uint64_t init_times[NUM_PERF_TESTS];
    
    printf("Running %d initialization tests...\n", NUM_PERF_TESTS);
    
    for (int i = 0; i < NUM_PERF_TESTS; i++) {
        lgx_runtime_config_t* config = lgx_config_create();
        assert(config != NULL);
        
        // Measure initialization time
        uint64_t start_time = lgx_time_now_ns();
        lgx_result_t result = lgx_runtime_init(config);
        uint64_t end_time = lgx_time_now_ns();
        
        assert(result == LGX_SUCCESS);
        (void)result;  // Suppress unused warning
        
        init_times[i] = end_time - start_time;
        
        printf("Test %d: Init time = %lu ns (%.2f ms)\n", 
               i + 1, init_times[i], init_times[i] / 1000000.0);
        
        // Shutdown and cleanup
        result = lgx_runtime_shutdown();
        assert(result == LGX_SUCCESS);
        
        lgx_config_destroy(config);
    }
    
    // Calculate statistics
    uint64_t total_time = 0;
    uint64_t min_time = init_times[0];
    uint64_t max_time = init_times[0];
    
    for (int i = 0; i < NUM_PERF_TESTS; i++) {
        total_time += init_times[i];
        if (init_times[i] < min_time) min_time = init_times[i];
        if (init_times[i] > max_time) max_time = init_times[i];
    }
    
    uint64_t avg_time = total_time / NUM_PERF_TESTS;
    #undef NUM_PERF_TESTS
    
    printf("\nInitialization Time Statistics:\n");
    printf("Average: %lu ns (%.2f ms)\n", avg_time, avg_time / 1000000.0);
    printf("Minimum: %lu ns (%.2f ms)\n", min_time, min_time / 1000000.0);
    printf("Maximum: %lu ns (%.2f ms)\n", max_time, max_time / 1000000.0);
    
    // Check against performance targets
    printf("\nPerformance Target Validation:\n");
    printf("Tier 1 Target: <1000ms (1,000,000,000 ns)\n");
    printf("Tier 2 Target: <500ms (500,000,000 ns)\n");
    
    if (avg_time < 500000000ULL) {
        printf("✅ PASSED: Tier 2 target (avg: %.2f ms < 500ms)\n", avg_time / 1000000.0);
    } else if (avg_time < 1000000000ULL) {
        printf("✅ PASSED: Tier 1 target (avg: %.2f ms < 1000ms)\n", avg_time / 1000000.0);
        printf("⚠️  MISSED: Tier 2 target (avg: %.2f ms >= 500ms)\n", avg_time / 1000000.0);
    } else {
        printf("❌ FAILED: Both targets (avg: %.2f ms >= 1000ms)\n", avg_time / 1000000.0);
    }
    
    // Test performance characteristics API
    printf("\nTesting Performance Characteristics API:\n");
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    lgx_performance_characteristics_t perf_chars;
    result = lgx_runtime_get_performance_characteristics(&perf_chars);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    printf("Kernel Version: %s\n", perf_chars.kernel_version);
    printf("CPU Model: %s\n", perf_chars.cpu_model);
    printf("Real-time Kernel: %s\n", perf_chars.real_time_kernel ? "Yes" : "No");
    printf("CPU Isolation: %s\n", perf_chars.cpu_isolation ? "Yes" : "No");
    printf("Measurement Conditions: %s\n", perf_chars.measurement_conditions);
    
    printf("Init Time Statistics from Runtime:\n");
    printf("  P50: %lu ns (%.2f ms)\n", 
           perf_chars.measured_init_time.p50_ns, 
           perf_chars.measured_init_time.p50_ns / 1000000.0);
    printf("  P95: %lu ns (%.2f ms)\n", 
           perf_chars.measured_init_time.p95_ns, 
           perf_chars.measured_init_time.p95_ns / 1000000.0);
    printf("  P99: %lu ns (%.2f ms)\n", 
           perf_chars.measured_init_time.p99_ns, 
           perf_chars.measured_init_time.p99_ns / 1000000.0);
    printf("  Sample Size: %zu\n", perf_chars.measured_init_time.sample_size);
    
    // Test memory allocation performance
    printf("\nTesting Memory Allocation Performance:\n");
    #define NUM_ALLOC_TESTS 1000
    uint64_t alloc_start = lgx_time_now_ns();
    
    void* ptrs[NUM_ALLOC_TESTS];
    for (int i = 0; i < NUM_ALLOC_TESTS; i++) {
        ptrs[i] = lgx_alloc(1024); // 1KB allocations
        assert(ptrs[i] != NULL);
    }
    
    uint64_t alloc_end = lgx_time_now_ns();
    uint64_t total_alloc_time = alloc_end - alloc_start;
    uint64_t avg_alloc_time = total_alloc_time / NUM_ALLOC_TESTS;
    
    printf("Allocated %d blocks of 1KB each\n", NUM_ALLOC_TESTS);
    printf("Total time: %lu ns (%.2f ms)\n", total_alloc_time, total_alloc_time / 1000000.0);
    printf("Average per allocation: %lu ns (%.2f μs)\n", avg_alloc_time, avg_alloc_time / 1000.0);
    
    // Check allocation performance targets
    printf("\nAllocation Performance Target Validation:\n");
    printf("Tier 1 Target: <5μs (5,000 ns)\n");
    printf("Tier 2 Target: <1μs (1,000 ns)\n");
    
    if (avg_alloc_time < 1000ULL) {
        printf("✅ PASSED: Tier 2 target (avg: %.2f μs < 1μs)\n", avg_alloc_time / 1000.0);
    } else if (avg_alloc_time < 5000ULL) {
        printf("✅ PASSED: Tier 1 target (avg: %.2f μs < 5μs)\n", avg_alloc_time / 1000.0);
        printf("⚠️  MISSED: Tier 2 target (avg: %.2f μs >= 1μs)\n", avg_alloc_time / 1000.0);
    } else {
        printf("❌ FAILED: Both targets (avg: %.2f μs >= 5μs)\n", avg_alloc_time / 1000.0);
    }
    
    // Clean up allocations
    for (int i = 0; i < NUM_ALLOC_TESTS; i++) {
        lgx_free(ptrs[i]);
    }
    #undef NUM_ALLOC_TESTS
    
    // Test memory statistics
    lgx_memory_stats_t mem_stats;
    result = lgx_memory_stats(&mem_stats);
    assert(result == LGX_SUCCESS);
    
    printf("\nMemory Statistics:\n");
    printf("Total Allocated: %zu bytes\n", mem_stats.total_allocated);
    printf("Peak Allocated: %zu bytes (%.2f MB)\n", 
           mem_stats.peak_allocated, mem_stats.peak_allocated / (1024.0 * 1024.0));
    printf("Current Allocated: %zu bytes\n", mem_stats.current_allocated);
    printf("Allocation Count: %lu\n", mem_stats.allocation_count);
    printf("Deallocation Count: %lu\n", mem_stats.deallocation_count);
    
    // Check memory usage target
    printf("\nMemory Usage Target Validation:\n");
    printf("Tier 1 Target: <300MB\n");
    printf("Tier 2 Target: <200MB\n");
    
    double peak_mb = mem_stats.peak_allocated / (1024.0 * 1024.0);
    if (peak_mb < 200.0) {
        printf("✅ PASSED: Tier 2 target (peak: %.2f MB < 200MB)\n", peak_mb);
    } else if (peak_mb < 300.0) {
        printf("✅ PASSED: Tier 1 target (peak: %.2f MB < 300MB)\n", peak_mb);
        printf("⚠️  MISSED: Tier 2 target (peak: %.2f MB >= 200MB)\n", peak_mb);
    } else {
        printf("❌ FAILED: Both targets (peak: %.2f MB >= 300MB)\n", peak_mb);
    }
    
    // Shutdown
    result = lgx_runtime_shutdown();
    assert(result == LGX_SUCCESS);
    
    lgx_config_destroy(config);
    
    printf("\n✅ All tests completed successfully!\n");
    return 0;
}