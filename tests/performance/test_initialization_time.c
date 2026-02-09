#include "../../include/lgx_runtime.h"
#include "../../include/lgx/lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

// Measure time in nanoseconds
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int main(void) {
    printf("=== LGX Runtime Initialization Time Test ===\n\n");
    
    // Run multiple iterations to get stable measurements
    const int iterations = 10;
    uint64_t times[iterations];
    
    for (int i = 0; i < iterations; i++) {
        lgx_runtime_config_t* config = lgx_config_create();
        assert(config != NULL);
        
        // Measure initialization time
        uint64_t start = get_time_ns();
        lgx_result_t result = lgx_runtime_init(config);
        uint64_t end = get_time_ns();
        
        assert(result == LGX_SUCCESS);
        (void)result;  // Suppress unused warning
        times[i] = end - start;
        
        printf("Iteration %d: %.2f ms\n", i + 1, times[i] / 1000000.0);
        
        // Shutdown
        lgx_runtime_shutdown();
        lgx_config_destroy(config);
        
        // Small delay between iterations
        struct timespec sleep_time = {0, 100000000};  // 100ms
        nanosleep(&sleep_time, NULL);
    }
    
    // Calculate statistics
    uint64_t min_time = times[0];
    uint64_t max_time = times[0];
    uint64_t total_time = 0;
    
    for (int i = 0; i < iterations; i++) {
        if (times[i] < min_time) min_time = times[i];
        if (times[i] > max_time) max_time = times[i];
        total_time += times[i];
    }
    
    double avg_time = (double)total_time / iterations;
    
    // Calculate median (sort first)
    for (int i = 0; i < iterations - 1; i++) {
        for (int j = i + 1; j < iterations; j++) {
            if (times[j] < times[i]) {
                uint64_t temp = times[i];
                times[i] = times[j];
                times[j] = temp;
            }
        }
    }
    uint64_t median_time = times[iterations / 2];
    
    printf("\n=== Initialization Time Statistics ===\n\n");
    printf("Iterations: %d\n", iterations);
    printf("Min:        %.2f ms\n", min_time / 1000000.0);
    printf("Max:        %.2f ms\n", max_time / 1000000.0);
    printf("Average:    %.2f ms\n", avg_time / 1000000.0);
    printf("Median:     %.2f ms\n", median_time / 1000000.0);
    printf("\n");
    
    // Check against targets
    printf("=== Target Validation ===\n\n");
    printf("Tier 1 Target: <1000ms\n");
    printf("Tier 2 Target: <500ms\n");
    printf("Tier 3 Target: <100ms\n\n");
    
    int passed = 0;
    if (avg_time < 100000000) {  // 100ms
        printf("✅ PASSED Tier 3: %.2f ms < 100ms\n", avg_time / 1000000.0);
        passed = 3;
    } else if (avg_time < 500000000) {  // 500ms
        printf("✅ PASSED Tier 2: %.2f ms < 500ms\n", avg_time / 1000000.0);
        passed = 2;
    } else if (avg_time < 1000000000) {  // 1000ms
        printf("✅ PASSED Tier 1: %.2f ms < 1000ms\n", avg_time / 1000000.0);
        passed = 1;
    } else {
        printf("❌ FAILED: %.2f ms >= 1000ms\n", avg_time / 1000000.0);
        passed = 0;
    }
    
    return (passed > 0) ? 0 : 1;
}
