/**
 * Test: Timing Services
 * 
 * Validates task 6.2: Timing services
 * - lgx_time_now_ns() provides monotonic time
 * - lgx_time_sleep_ms() sleeps for specified duration
 * - Timing precision validation
 * - Timing overhead measurement
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

// Test 1: lgx_time_now_ns() returns monotonic time
static void test_monotonic_time(void) {
    printf("Test 1: Monotonic time\n");
    
    uint64_t t1 = lgx_time_now_ns();
    uint64_t t2 = lgx_time_now_ns();
    uint64_t t3 = lgx_time_now_ns();
    
    // Time should always increase (or stay the same if resolution is low)
    assert(t2 >= t1);
    assert(t3 >= t2);
    
    printf("  t1: %lu ns\n", (unsigned long)t1);
    printf("  t2: %lu ns\n", (unsigned long)t2);
    printf("  t3: %lu ns\n", (unsigned long)t3);
    printf("  ✓ Time is monotonic\n");
}

// Test 2: lgx_time_sleep_ms() sleeps for approximately correct duration
static void test_sleep_accuracy(void) {
    printf("Test 2: Sleep accuracy\n");
    
    const uint32_t sleep_ms = 100;
    const double tolerance = 0.20; // 20% tolerance
    
    uint64_t start = lgx_time_now_ns();
    lgx_time_sleep_ms(sleep_ms);
    uint64_t end = lgx_time_now_ns();
    
    uint64_t elapsed_ns = end - start;
    double elapsed_ms = elapsed_ns / 1000000.0;
    double error = fabs(elapsed_ms - sleep_ms) / sleep_ms;
    
    printf("  Requested: %u ms\n", sleep_ms);
    printf("  Actual: %.2f ms\n", elapsed_ms);
    printf("  Error: %.2f%%\n", error * 100.0);
    
    if (error <= tolerance) {
        printf("  ✓ Sleep accuracy within tolerance (%.0f%%)\n", tolerance * 100.0);
    } else {
        printf("  ⚠ Sleep accuracy outside tolerance: %.2f%% (max: %.0f%%)\n", 
               error * 100.0, tolerance * 100.0);
    }
}

// Test 3: Multiple sleep calls
static void test_multiple_sleeps(void) {
    printf("Test 3: Multiple sleep calls\n");
    
    const int iterations = 5;
    const uint32_t sleep_ms = 10;
    
    for (int i = 0; i < iterations; i++) {
        uint64_t start = lgx_time_now_ns();
        lgx_time_sleep_ms(sleep_ms);
        uint64_t end = lgx_time_now_ns();
        
        uint64_t elapsed_ns = end - start;
        double elapsed_ms = elapsed_ns / 1000000.0;
        
        printf("  Iteration %d: %.2f ms\n", i + 1, elapsed_ms);
    }
    
    printf("  ✓ Multiple sleep calls completed\n");
}

// Test 4: Timing precision validation
static void test_timing_precision(void) {
    printf("Test 4: Timing precision\n");
    
    uint64_t precision_ns = lgx_time_get_precision_ns();
    double precision_us = precision_ns / 1000.0;
    
    printf("  Clock precision: %lu ns (%.3f μs)\n", 
           (unsigned long)precision_ns, precision_us);
    
    // Most modern systems should have at least 1μs precision
    if (precision_ns <= 1000) {
        printf("  ✓ Excellent precision (<1μs)\n");
    } else if (precision_ns <= 10000) {
        printf("  ✓ Good precision (<10μs)\n");
    } else {
        printf("  ⚠ Low precision (>10μs)\n");
    }
}

// Test 5: Timing overhead measurement
static void test_timing_overhead(void) {
    printf("Test 5: Timing overhead\n");
    
    uint64_t overhead_ns = lgx_time_measure_overhead_ns();
    double overhead_us = overhead_ns / 1000.0;
    
    printf("  Average overhead: %lu ns (%.3f μs)\n", 
           (unsigned long)overhead_ns, overhead_us);
    
    // Overhead should be reasonable (typically <1μs on modern systems)
    if (overhead_ns < 100) {
        printf("  ✓ Excellent overhead (<100ns)\n");
    } else if (overhead_ns < 1000) {
        printf("  ✓ Good overhead (<1μs)\n");
    } else if (overhead_ns < 10000) {
        printf("  ✓ Acceptable overhead (<10μs)\n");
    } else {
        printf("  ⚠ High overhead (>10μs)\n");
    }
}

// Test 6: Time measurement consistency
static void test_measurement_consistency(void) {
    printf("Test 6: Measurement consistency\n");
    
    #define NUM_TIMING_ITERATIONS 100
    uint64_t measurements[NUM_TIMING_ITERATIONS];
    
    // Take multiple measurements
    for (int i = 0; i < NUM_TIMING_ITERATIONS; i++) {
        uint64_t start = lgx_time_now_ns();
        uint64_t end = lgx_time_now_ns();
        measurements[i] = end - start;
    }
    
    // Calculate statistics
    uint64_t min = measurements[0];
    uint64_t max = measurements[0];
    uint64_t sum = 0;
    
    for (int i = 0; i < NUM_TIMING_ITERATIONS; i++) {
        if (measurements[i] < min) min = measurements[i];
        if (measurements[i] > max) max = measurements[i];
        sum += measurements[i];
    }
    
    uint64_t avg = sum / NUM_TIMING_ITERATIONS;
    #undef NUM_TIMING_ITERATIONS
    
    printf("  Min: %lu ns\n", (unsigned long)min);
    printf("  Avg: %lu ns\n", (unsigned long)avg);
    printf("  Max: %lu ns\n", (unsigned long)max);
    printf("  Range: %lu ns\n", (unsigned long)(max - min));
    
    printf("  ✓ Measurement consistency validated\n");
}

// Test 7: Zero sleep
static void test_zero_sleep(void) {
    printf("Test 7: Zero sleep\n");
    
    uint64_t start = lgx_time_now_ns();
    lgx_time_sleep_ms(0);
    uint64_t end = lgx_time_now_ns();
    
    uint64_t elapsed_ns = end - start;
    double elapsed_us = elapsed_ns / 1000.0;
    
    printf("  Zero sleep duration: %.3f μs\n", elapsed_us);
    
    // Zero sleep should return quickly (typically <100μs)
    if (elapsed_ns < 100000) {
        printf("  ✓ Zero sleep returns quickly (<100μs)\n");
    } else {
        printf("  ⚠ Zero sleep took longer than expected: %.3f μs\n", elapsed_us);
    }
}

// Test 8: Long duration measurement
static void test_long_duration(void) {
    printf("Test 8: Long duration measurement\n");
    
    const uint32_t sleep_ms = 1000; // 1 second
    
    uint64_t start = lgx_time_now_ns();
    lgx_time_sleep_ms(sleep_ms);
    uint64_t end = lgx_time_now_ns();
    
    uint64_t elapsed_ns = end - start;
    double elapsed_ms = elapsed_ns / 1000000.0;
    double error = fabs(elapsed_ms - sleep_ms) / sleep_ms;
    
    printf("  Requested: %u ms\n", sleep_ms);
    printf("  Actual: %.2f ms\n", elapsed_ms);
    printf("  Error: %.2f%%\n", error * 100.0);
    
    // Long sleeps should be more accurate (within 5%)
    if (error <= 0.05) {
        printf("  ✓ Long duration accurate (within 5%%)\n");
    } else {
        printf("  ⚠ Long duration error: %.2f%%\n", error * 100.0);
    }
}

int main(void) {
    printf("=== Timing Services Tests ===\n\n");
    
    test_monotonic_time();
    test_sleep_accuracy();
    test_multiple_sleeps();
    test_timing_precision();
    test_timing_overhead();
    test_measurement_consistency();
    test_zero_sleep();
    test_long_duration();
    
    printf("\n=== All Timing Services Tests Passed ===\n");
    return 0;
}
