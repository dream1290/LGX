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

int main(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused
    
    printf("=== LGX Runtime Initialization Time Benchmark ===\n\n");
    
    // Configuration
    const int num_iterations = 100;
    const int warmup_iterations = 10;
    
    uint64_t* init_times = malloc(sizeof(uint64_t) * num_iterations);
    uint64_t* shutdown_times = malloc(sizeof(uint64_t) * num_iterations);
    
    assert(init_times != NULL);
    assert(shutdown_times != NULL);
    
    // Warmup phase
    printf("Running %d warmup iterations...\n", warmup_iterations);
    for (int i = 0; i < warmup_iterations; i++) {
        lgx_runtime_config_t* config = lgx_config_create();
        lgx_runtime_init(config);
        lgx_runtime_shutdown();
        lgx_config_destroy(config);
    }
    
    // Measurement phase
    printf("Running %d measurement iterations...\n", num_iterations);
    for (int i = 0; i < num_iterations; i++) {
        lgx_runtime_config_t* config = lgx_config_create();
        assert(config != NULL);
        
        // Measure initialization
        uint64_t init_start = lgx_time_now_ns();
        lgx_result_t result = lgx_runtime_init(config);
        uint64_t init_end = lgx_time_now_ns();
        
        assert(result == LGX_SUCCESS);
        (void)result;  // Suppress unused warning
        init_times[i] = init_end - init_start;
        
        // Measure shutdown
        uint64_t shutdown_start = lgx_time_now_ns();
        result = lgx_runtime_shutdown();
        uint64_t shutdown_end = lgx_time_now_ns();
        
        assert(result == LGX_SUCCESS);
        (void)result;  // Suppress unused warning
        shutdown_times[i] = shutdown_end - shutdown_start;
        
        lgx_config_destroy(config);
        
        if ((i + 1) % 10 == 0) {
            printf("  Completed %d/%d iterations\n", i + 1, num_iterations);
        }
    }
    
    // Sort for percentile calculation
    qsort(init_times, num_iterations, sizeof(uint64_t), compare_uint64);
    qsort(shutdown_times, num_iterations, sizeof(uint64_t), compare_uint64);
    
    // Calculate statistics
    uint64_t init_p50 = calculate_percentile(init_times, num_iterations, 50.0);
    uint64_t init_p95 = calculate_percentile(init_times, num_iterations, 95.0);
    uint64_t init_p99 = calculate_percentile(init_times, num_iterations, 99.0);
    uint64_t init_min = init_times[0];
    uint64_t init_max = init_times[num_iterations - 1];
    
    uint64_t shutdown_p50 = calculate_percentile(shutdown_times, num_iterations, 50.0);
    uint64_t shutdown_p95 = calculate_percentile(shutdown_times, num_iterations, 95.0);
    uint64_t shutdown_p99 = calculate_percentile(shutdown_times, num_iterations, 99.0);
    
    // Print results
    printf("\n=== Initialization Time Results ===\n");
    printf("Min:  %10lu ns (%8.2f ms)\n", init_min, init_min / 1000000.0);
    printf("P50:  %10lu ns (%8.2f ms)\n", init_p50, init_p50 / 1000000.0);
    printf("P95:  %10lu ns (%8.2f ms)\n", init_p95, init_p95 / 1000000.0);
    printf("P99:  %10lu ns (%8.2f ms)\n", init_p99, init_p99 / 1000000.0);
    printf("Max:  %10lu ns (%8.2f ms)\n", init_max, init_max / 1000000.0);
    
    printf("\n=== Shutdown Time Results ===\n");
    printf("P50:  %10lu ns (%8.2f ms)\n", shutdown_p50, shutdown_p50 / 1000000.0);
    printf("P95:  %10lu ns (%8.2f ms)\n", shutdown_p95, shutdown_p95 / 1000000.0);
    printf("P99:  %10lu ns (%8.2f ms)\n", shutdown_p99, shutdown_p99 / 1000000.0);
    
    // Performance targets
    printf("\n=== Performance Target Validation ===\n");
    const uint64_t tier1_target_ns = 1000000000ULL; // 1000ms
    const uint64_t tier2_target_ns = 500000000ULL;  // 500ms
    
    printf("Tier 1 Target: < 1000ms\n");
    printf("Tier 2 Target: < 500ms\n\n");
    
    int passed = 0;
    if (init_p99 < tier2_target_ns) {
        printf("✅ PASSED Tier 2: P99 = %.2f ms < 500ms\n", init_p99 / 1000000.0);
        passed = 2;
    } else if (init_p99 < tier1_target_ns) {
        printf("✅ PASSED Tier 1: P99 = %.2f ms < 1000ms\n", init_p99 / 1000000.0);
        printf("⚠️  MISSED Tier 2: P99 = %.2f ms >= 500ms\n", init_p99 / 1000000.0);
        passed = 1;
    } else {
        printf("❌ FAILED: P99 = %.2f ms >= 1000ms\n", init_p99 / 1000000.0);
        passed = 0;
    }
    
    // Export results for regression detection
    FILE* results_file = fopen("benchmark_results_init.txt", "w");
    if (results_file) {
        fprintf(results_file, "benchmark=initialization\n");
        fprintf(results_file, "iterations=%d\n", num_iterations);
        fprintf(results_file, "init_p50_ns=%lu\n", init_p50);
        fprintf(results_file, "init_p95_ns=%lu\n", init_p95);
        fprintf(results_file, "init_p99_ns=%lu\n", init_p99);
        fprintf(results_file, "init_min_ns=%lu\n", init_min);
        fprintf(results_file, "init_max_ns=%lu\n", init_max);
        fprintf(results_file, "shutdown_p50_ns=%lu\n", shutdown_p50);
        fprintf(results_file, "shutdown_p95_ns=%lu\n", shutdown_p95);
        fprintf(results_file, "shutdown_p99_ns=%lu\n", shutdown_p99);
        fprintf(results_file, "tier_passed=%d\n", passed);
        fclose(results_file);
        printf("\n✅ Results exported to benchmark_results_init.txt\n");
    }
    
    free(init_times);
    free(shutdown_times);
    
    return (passed > 0) ? 0 : 1;
}
