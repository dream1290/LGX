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

// Simulate a typical game frame with allocations
static uint64_t simulate_frame(int num_allocations) {
    uint64_t frame_start = lgx_time_now_ns();
    
    void** ptrs = malloc(sizeof(void*) * num_allocations);
    assert(ptrs != NULL);
    
    // Simulate frame allocations (mix of sizes)
    for (int i = 0; i < num_allocations; i++) {
        size_t size;
        if (i % 10 == 0) {
            size = 4096;  // Large allocation (10%)
        } else if (i % 3 == 0) {
            size = 512;   // Medium allocation (30%)
        } else {
            size = 64;    // Small allocation (60%)
        }
        
        ptrs[i] = lgx_alloc_frame(size);
        assert(ptrs[i] != NULL);
    }
    
    // Simulate some work (memory access)
    for (int i = 0; i < num_allocations; i++) {
        memset(ptrs[i], 0xAB, 64);  // Touch memory
    }
    
    // Free allocations (frame arena doesn't actually free, but test the API)
    for (int i = 0; i < num_allocations; i++) {
        lgx_free(ptrs[i]);
    }
    
    free(ptrs);
    
    uint64_t frame_end = lgx_time_now_ns();
    return frame_end - frame_start;
}

int main(int argc, char** argv) {
    (void)argc;  // Unused
    (void)argv;  // Unused
    
    printf("=== LGX Runtime Frame-Time Contribution Benchmark ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Test different allocation counts per frame
    int allocation_counts[] = {10, 50, 100, 200, 500, 1000};
    const int num_tests = sizeof(allocation_counts) / sizeof(allocation_counts[0]);
    const int frames_per_test = 1000;
    
    printf("Simulating %d frames for each allocation count...\n\n", frames_per_test);
    
    for (int test = 0; test < num_tests; test++) {
        int alloc_count = allocation_counts[test];
        uint64_t* frame_times = malloc(sizeof(uint64_t) * frames_per_test);
        assert(frame_times != NULL);
        
        // Warmup
        for (int i = 0; i < 10; i++) {
            simulate_frame(alloc_count);
        }
        
        // Measure frames
        for (int i = 0; i < frames_per_test; i++) {
            frame_times[i] = simulate_frame(alloc_count);
        }
        
        // Calculate statistics
        qsort(frame_times, frames_per_test, sizeof(uint64_t), compare_uint64);
        
        uint64_t p50 = calculate_percentile(frame_times, frames_per_test, 50.0);
        uint64_t p95 = calculate_percentile(frame_times, frames_per_test, 95.0);
        uint64_t p99 = calculate_percentile(frame_times, frames_per_test, 99.0);
        
        // Calculate average allocation time
        uint64_t avg_alloc_time = p50 / alloc_count;
        
        printf("=== %d allocations per frame ===\n", alloc_count);
        printf("Frame time P50: %8lu ns (%6.2f μs)\n", p50, p50 / 1000.0);
        printf("Frame time P95: %8lu ns (%6.2f μs)\n", p95, p95 / 1000.0);
        printf("Frame time P99: %8lu ns (%6.2f μs)\n", p99, p99 / 1000.0);
        printf("Avg per alloc:  %8lu ns (%6.2f μs)\n", avg_alloc_time, avg_alloc_time / 1000.0);
        
        // Calculate frame budget impact (60 FPS = 16.67ms per frame)
        const uint64_t frame_budget_ns = 16666666;  // 16.67ms in nanoseconds
        double budget_percent = (p99 * 100.0) / frame_budget_ns;
        
        printf("Frame budget:   %6.2f%% of 16.67ms (60 FPS)\n", budget_percent);
        printf("\n");
        
        free(frame_times);
    }
    
    // Performance target validation
    printf("=== Performance Target Validation ===\n");
    printf("Target: Frame allocations should use < 5%% of frame budget\n");
    printf("Frame budget: 16.67ms (60 FPS)\n");
    printf("Max allowed:  833μs (5%% of 16.67ms)\n\n");
    
    // Test with typical game frame (100 allocations)
    const int typical_alloc_count = 100;
    uint64_t* typical_frames = malloc(sizeof(uint64_t) * frames_per_test);
    assert(typical_frames != NULL);
    
    for (int i = 0; i < frames_per_test; i++) {
        typical_frames[i] = simulate_frame(typical_alloc_count);
    }
    
    qsort(typical_frames, frames_per_test, sizeof(uint64_t), compare_uint64);
    uint64_t typical_p99 = calculate_percentile(typical_frames, frames_per_test, 99.0);
    
    const uint64_t frame_budget_ns = 16666666;
    const uint64_t max_allowed_ns = frame_budget_ns / 20;  // 5%
    
    double actual_percent = (typical_p99 * 100.0) / frame_budget_ns;
    
    int passed = 0;
    if (typical_p99 < max_allowed_ns) {
        printf("✅ PASSED: 100 allocs P99 = %.2f μs (%.2f%% < 5%%)\n",
               typical_p99 / 1000.0, actual_percent);
        passed = 1;
    } else {
        printf("❌ FAILED: 100 allocs P99 = %.2f μs (%.2f%% >= 5%%)\n",
               typical_p99 / 1000.0, actual_percent);
        passed = 0;
    }
    
    // Export results for regression detection
    FILE* results_file = fopen("benchmark_results_frame.txt", "w");
    if (results_file) {
        fprintf(results_file, "benchmark=frame_time_contribution\n");
        fprintf(results_file, "frames_per_test=%d\n", frames_per_test);
        
        for (int test = 0; test < num_tests; test++) {
            int alloc_count = allocation_counts[test];
            uint64_t* frame_times = malloc(sizeof(uint64_t) * frames_per_test);
            
            for (int i = 0; i < frames_per_test; i++) {
                frame_times[i] = simulate_frame(alloc_count);
            }
            
            qsort(frame_times, frames_per_test, sizeof(uint64_t), compare_uint64);
            
            uint64_t p50 = calculate_percentile(frame_times, frames_per_test, 50.0);
            uint64_t p95 = calculate_percentile(frame_times, frames_per_test, 95.0);
            uint64_t p99 = calculate_percentile(frame_times, frames_per_test, 99.0);
            
            fprintf(results_file, "alloc_%d_p50_ns=%lu\n", alloc_count, p50);
            fprintf(results_file, "alloc_%d_p95_ns=%lu\n", alloc_count, p95);
            fprintf(results_file, "alloc_%d_p99_ns=%lu\n", alloc_count, p99);
            
            free(frame_times);
        }
        
        fprintf(results_file, "typical_100_p99_ns=%lu\n", typical_p99);
        fprintf(results_file, "budget_percent=%.2f\n", actual_percent);
        fprintf(results_file, "passed=%d\n", passed);
        
        fclose(results_file);
        printf("\n✅ Results exported to benchmark_results_frame.txt\n");
    }
    
    free(typical_frames);
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return passed ? 0 : 1;
}
