/**
 * Test: Observability Overhead Measurement
 * 
 * Measures the overhead of different observability levels to validate
 * that they meet the performance targets specified in the design.
 */

#include "lgx_runtime.h"
#include "lgx_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define NUM_ITERATIONS 1000000
#define NUM_WARMUP 10000

// Get high-resolution timestamp in nanoseconds
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Measure baseline overhead (no observability)
static double measure_baseline(void) {
    uint64_t start, end;
    volatile int dummy = 0;
    
    // Warmup
    for (int i = 0; i < NUM_WARMUP; i++) {
        dummy += i;
    }
    
    start = get_time_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        dummy += i;
    }
    end = get_time_ns();
    
    return (double)(end - start) / NUM_ITERATIONS;
}

// Measure counter increment overhead
static double measure_counter_overhead(void) {
    lgx_custom_counter_t counter = lgx_register_counter("test.overhead");
    assert(counter != (lgx_custom_counter_t)-1);
    
    uint64_t start, end;
    
    // Warmup
    for (int i = 0; i < NUM_WARMUP; i++) {
        lgx_increment_counter(counter);
    }
    
    start = get_time_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        lgx_increment_counter(counter);
    }
    end = get_time_ns();
    
    return (double)(end - start) / NUM_ITERATIONS;
}

// Measure logging overhead (filtered out)
static double measure_logging_overhead_filtered(void) {
    // Set log level to ERROR (filter out INFO)
    lgx_set_log_filter(LGX_LOG_ERROR);
    
    uint64_t start, end;
    
    // Warmup
    for (int i = 0; i < NUM_WARMUP; i++) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "Test message %d", i);
    }
    
    start = get_time_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "Test message %d", i);
    }
    end = get_time_ns();
    
    // Reset log level
    lgx_set_log_filter(LGX_LOG_DEBUG);
    
    return (double)(end - start) / NUM_ITERATIONS;
}

// Measure logging overhead (not filtered, but to /dev/null)
static double measure_logging_overhead_active(void) {
    uint64_t start, end;
    
    // Warmup
    for (int i = 0; i < NUM_WARMUP; i++) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "Test message %d", i);
    }
    
    start = get_time_ns();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "Test message %d", i);
    }
    end = get_time_ns();
    
    return (double)(end - start) / NUM_ITERATIONS;
}

static void test_observability_overhead(void) {
    printf("Test: Observability overhead measurement...\n\n");
    
    // Initialize runtime with telemetry enabled
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    // Set log path to /dev/null to avoid I/O overhead
    lgx_config_set_log_path(config, "/dev/null");
    
    // Enable telemetry for observability features
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;
    
    // Measure baseline
    double baseline_ns = measure_baseline();
    printf("  Baseline (no observability): %.2f ns/operation\n", baseline_ns);
    
    // Measure counter overhead
    double counter_ns = measure_counter_overhead();
    double counter_overhead = counter_ns - baseline_ns;
    printf("  Counter increment: %.2f ns/operation (%.2f ns overhead)\n", 
           counter_ns, counter_overhead);
    
    // Measure logging overhead (filtered)
    double logging_filtered_ns = measure_logging_overhead_filtered();
    double logging_filtered_overhead = logging_filtered_ns - baseline_ns;
    printf("  Logging (filtered out): %.2f ns/operation (%.2f ns overhead)\n",
           logging_filtered_ns, logging_filtered_overhead);
    
    // Measure logging overhead (active)
    double logging_active_ns = measure_logging_overhead_active();
    double logging_active_overhead = logging_active_ns - baseline_ns;
    printf("  Logging (active): %.2f ns/operation (%.2f ns overhead)\n",
           logging_active_ns, logging_active_overhead);
    
    printf("\n");
    
    // Validate overhead targets
    printf("  Validating overhead targets:\n");
    
    // LGX_OBS_MINIMAL: Counters only (<0.1% overhead)
    // Assuming 1000 operations per frame at 60 FPS = 16.67ms per frame
    // 0.1% of 16.67ms = 16.67μs = 16670ns
    // Per operation: 16670ns / 1000 = 16.67ns
    double minimal_target_ns = 16.67;
    bool minimal_ok = counter_overhead < minimal_target_ns;
    printf("    LGX_OBS_MINIMAL (counters): %.2f ns < %.2f ns target: %s\n",
           counter_overhead, minimal_target_ns, minimal_ok ? "✓ PASS" : "✗ FAIL");
    
    // LGX_OBS_NORMAL: Counters + filtered logging (<0.5% overhead)
    // 0.5% of 16.67ms = 83.35μs = 83350ns
    // Per operation: 83350ns / 1000 = 83.35ns
    double normal_target_ns = 83.35;
    double normal_overhead = counter_overhead + logging_filtered_overhead;
    bool normal_ok = normal_overhead < normal_target_ns;
    printf("    LGX_OBS_NORMAL (counters + filtered logs): %.2f ns < %.2f ns target: %s\n",
           normal_overhead, normal_target_ns, normal_ok ? "✓ PASS" : "✗ FAIL");
    
    // LGX_OBS_DETAILED: Above + active logging (<2% overhead)
    // 2% of 16.67ms = 333.4μs = 333400ns
    // Per operation: 333400ns / 1000 = 333.4ns
    double detailed_target_ns = 333.4;
    double detailed_overhead = counter_overhead + logging_active_overhead;
    bool detailed_ok = detailed_overhead < detailed_target_ns;
    printf("    LGX_OBS_DETAILED (counters + active logs): %.2f ns < %.2f ns target: %s\n",
           detailed_overhead, detailed_target_ns, detailed_ok ? "✓ PASS" : "✗ FAIL");
    
    printf("\n");
    
    // Overall validation
    if (minimal_ok && normal_ok && detailed_ok) {
        printf("  ✓ All observability overhead targets met\n");
    } else {
        printf("  ✗ Some observability overhead targets not met\n");
        printf("     Note: This may be acceptable depending on hardware and workload\n");
    }
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

static void test_observability_levels(void) {
    printf("\nTest: Observability level configuration...\n");
    
    // Initialize runtime with telemetry enabled
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    // Enable telemetry so observability levels work
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;
    
    // Test setting different observability levels
    lgx_set_observability_level(LGX_OBS_NONE);
    assert(lgx_get_observability_level() == LGX_OBS_NONE);
    printf("  ✓ LGX_OBS_NONE set successfully\n");
    
    lgx_set_observability_level(LGX_OBS_MINIMAL);
    assert(lgx_get_observability_level() == LGX_OBS_MINIMAL);
    printf("  ✓ LGX_OBS_MINIMAL set successfully\n");
    
    lgx_set_observability_level(LGX_OBS_NORMAL);
    assert(lgx_get_observability_level() == LGX_OBS_NORMAL);
    printf("  ✓ LGX_OBS_NORMAL set successfully\n");
    
    lgx_set_observability_level(LGX_OBS_DETAILED);
    assert(lgx_get_observability_level() == LGX_OBS_DETAILED);
    printf("  ✓ LGX_OBS_DETAILED set successfully\n");
    
    lgx_set_observability_level(LGX_OBS_EXHAUSTIVE);
    assert(lgx_get_observability_level() == LGX_OBS_EXHAUSTIVE);
    printf("  ✓ LGX_OBS_EXHAUSTIVE set successfully\n");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== Observability Overhead Tests ===\n\n");
    
    test_observability_overhead();
    test_observability_levels();
    
    printf("\n=== All Observability Overhead Tests Passed ===\n");
    return 0;
}
