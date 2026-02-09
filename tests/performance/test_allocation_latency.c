#include "../../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Statistical helpers
static int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    return (ua > ub) - (ua < ub);
}

static uint64_t calculate_percentile(uint64_t* sorted_data, size_t count, double percentile) {
    size_t index = (size_t)((percentile / 100.0) * count);
    if (index >= count) index = count - 1;
    return sorted_data[index];
}

typedef struct {
    const char* name;
    size_t size;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t min;
    uint64_t max;
} benchmark_result_t;

static void benchmark_allocation_size(size_t size, int iterations, benchmark_result_t* result) {
    uint64_t* alloc_times = malloc(sizeof(uint64_t) * iterations);
    void** ptrs = malloc(sizeof(void*) * iterations);
    
    assert(alloc_times != NULL);
    assert(ptrs != NULL);
    
    // Measure allocations
    for (int i = 0; i < iterations; i++) {
        uint64_t start = lgx_time_now_ns();
        ptrs[i] = lgx_alloc(size);
        uint64_t end = lgx_time_now_ns();
        
        assert(ptrs[i] != NULL);
        alloc_times[i] = end - start;
    }
    
    // Free all allocations
    for (int i = 0; i < iterations; i++) {
        lgx_free(ptrs[i]);
    }
    
    // Calculate statistics
    qsort(alloc_times, iterations, sizeof(uint64_t), compare_uint64);
    
    result->size = size;
    result->p50 = calculate_percentile(alloc_times, iterations, 50.0);
    result->p95 = calculate_percentile(alloc_times, iterations, 95.0);
    result->p99 = calculate_percentile(alloc_times, iterations, 99.0);
    result->min = alloc_times[0];
    result->max = alloc_times[iterations - 1];
    
    free(alloc_times);
    free(ptrs);
}

static void benchmark_intent_allocation(const char* name, 
                                       void* (*alloc_fn)(size_t),
                                       size_t size,
                                       int iterations,
                                       benchmark_result_t* result) {
    uint64_t* alloc_times = malloc(sizeof(uint64_t) * iterations);
    void** ptrs = malloc(sizeof(void*) * iterations);
    
    assert(alloc_times != NULL);
    assert(ptrs != NULL);
    
    // Measure allocations
    for (int i = 0; i < iterations; i++) {
        uint64_t start = lgx_time_now_ns();
        ptrs[i] = alloc_fn(size);
        uint64_t end = lgx_time_now_ns();
        
        assert(ptrs[i] != NULL);
        alloc_times[i] = end - start;
    }
    
    // Free all allocations
    for (int i = 0; i < iterations; i++) {
        lgx_free(ptrs[i]);
    }
    
    // Calculate statistics
    qsort(alloc_times, iterations, sizeof(uint64_t), compare_uint64);
    
    result->name = name;
    result->size = size;
    result->p50 = calculate_percentile(alloc_times, iterations, 50.0);
    result->p95 = calculate_percentile(alloc_times, iterations, 95.0);
    result->p99 = calculate_percentile(alloc_times, iterations, 99.0);
    result->min = alloc_times[0];
    result->max = alloc_times[iterations - 1];
    
    free(alloc_times);
    free(ptrs);
}

int main(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused
    
    printf("=== LGX Runtime Allocation Latency Benchmark ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    const int iterations = 10000;
    
    // Test different allocation sizes
    printf("Testing allocation sizes (lgx_alloc)...\n");
    
    size_t test_sizes[] = {
        16, 32, 64, 128, 256, 512, 1024,
        2048, 4096, 8192, 16384, 32768, 65536
    };
    const int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    benchmark_result_t* size_results = malloc(sizeof(benchmark_result_t) * num_sizes);
    assert(size_results != NULL);
    
    for (int i = 0; i < num_sizes; i++) {
        benchmark_allocation_size(test_sizes[i], iterations, &size_results[i]);
        printf("  %6zu bytes: P50=%6lu ns, P95=%6lu ns, P99=%6lu ns\n",
               test_sizes[i],
               size_results[i].p50,
               size_results[i].p95,
               size_results[i].p99);
    }
    
    // Test intent-based allocations
    printf("\nTesting intent-based allocations...\n");
    
    benchmark_result_t frame_result, persistent_result, level_result;
    
    benchmark_intent_allocation("frame", lgx_alloc_frame, 1024, iterations, &frame_result);
    printf("  Frame (1KB):      P50=%6lu ns, P95=%6lu ns, P99=%6lu ns\n",
           frame_result.p50, frame_result.p95, frame_result.p99);
    
    benchmark_intent_allocation("persistent", lgx_alloc_persistent, 1024, iterations, &persistent_result);
    printf("  Persistent (1KB): P50=%6lu ns, P95=%6lu ns, P99=%6lu ns\n",
           persistent_result.p50, persistent_result.p95, persistent_result.p99);
    
    benchmark_intent_allocation("level", lgx_alloc_level, 1024, iterations, &level_result);
    printf("  Level (1KB):      P50=%6lu ns, P95=%6lu ns, P99=%6lu ns\n",
           level_result.p50, level_result.p95, level_result.p99);
    
    // Performance target validation
    printf("\n=== Performance Target Validation ===\n");
    const uint64_t tier1_target_ns = 5000;  // 5μs
    const uint64_t tier2_target_ns = 1000;  // 1μs
    
    printf("Tier 1 Target: < 5μs\n");
    printf("Tier 2 Target: < 1μs\n\n");
    
    // Check 1KB allocation (most common size)
    benchmark_result_t kb_result;
    benchmark_allocation_size(1024, iterations, &kb_result);
    
    int passed = 0;
    if (kb_result.p99 < tier2_target_ns) {
        printf("✅ PASSED Tier 2: 1KB P99 = %.2f μs < 1μs\n", kb_result.p99 / 1000.0);
        passed = 2;
    } else if (kb_result.p99 < tier1_target_ns) {
        printf("✅ PASSED Tier 1: 1KB P99 = %.2f μs < 5μs\n", kb_result.p99 / 1000.0);
        printf("⚠️  MISSED Tier 2: 1KB P99 = %.2f μs >= 1μs\n", kb_result.p99 / 1000.0);
        passed = 1;
    } else {
        printf("❌ FAILED: 1KB P99 = %.2f μs >= 5μs\n", kb_result.p99 / 1000.0);
        passed = 0;
    }
    
    // Export results for regression detection
    FILE* results_file = fopen("benchmark_results_alloc.txt", "w");
    if (results_file) {
        fprintf(results_file, "benchmark=allocation_latency\n");
        fprintf(results_file, "iterations=%d\n", iterations);
        
        // Export size results
        for (int i = 0; i < num_sizes; i++) {
            fprintf(results_file, "size_%zu_p50_ns=%lu\n", test_sizes[i], size_results[i].p50);
            fprintf(results_file, "size_%zu_p95_ns=%lu\n", test_sizes[i], size_results[i].p95);
            fprintf(results_file, "size_%zu_p99_ns=%lu\n", test_sizes[i], size_results[i].p99);
        }
        
        // Export intent results
        fprintf(results_file, "frame_p50_ns=%lu\n", frame_result.p50);
        fprintf(results_file, "frame_p95_ns=%lu\n", frame_result.p95);
        fprintf(results_file, "frame_p99_ns=%lu\n", frame_result.p99);
        fprintf(results_file, "persistent_p50_ns=%lu\n", persistent_result.p50);
        fprintf(results_file, "persistent_p95_ns=%lu\n", persistent_result.p95);
        fprintf(results_file, "persistent_p99_ns=%lu\n", persistent_result.p99);
        fprintf(results_file, "level_p50_ns=%lu\n", level_result.p50);
        fprintf(results_file, "level_p95_ns=%lu\n", level_result.p95);
        fprintf(results_file, "level_p99_ns=%lu\n", level_result.p99);
        
        fprintf(results_file, "tier_passed=%d\n", passed);
        fclose(results_file);
        printf("\n✅ Results exported to benchmark_results_alloc.txt\n");
    }
    
    // Cleanup
    free(size_results);
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return (passed > 0) ? 0 : 1;
}
