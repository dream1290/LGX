/**
 * Test: Suspend/Resume Lifecycle Management
 * 
 * Validates AC-5: Lifecycle Management
 * - Suspend SHALL complete in <100ms
 * - Resume SHALL complete in <100ms
 * - State SHALL be preserved across suspend/resume
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

// Helper: Get time in nanoseconds
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Test 1: Basic suspend/resume cycle
static void test_basic_suspend_resume(void) {
    printf("Test 1: Basic suspend/resume cycle\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    // Suspend
    result = lgx_runtime_suspend();
    assert(result == LGX_SUCCESS);
    
    // Resume
    result = lgx_runtime_resume();
    assert(result == LGX_SUCCESS);
    
    // Cleanup
    result = lgx_runtime_shutdown();
    (void)result; // Suppress unused warning
    lgx_config_destroy(config);
    
    printf("  ✓ Basic suspend/resume works\n");
}

// Test 2: Suspend time budget (<100ms)
static void test_suspend_time_budget(void) {
    printf("Test 2: Suspend time budget (<100ms)\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    // Measure suspend time
    uint64_t start = get_time_ns();
    result = lgx_runtime_suspend();
    uint64_t end = get_time_ns();
    
    assert(result == LGX_SUCCESS);
    
    uint64_t elapsed_ns = end - start;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    
    printf("  Suspend time: %lu ms\n", (unsigned long)elapsed_ms);
    
    // Validate <100ms budget (Tier 1 requirement)
    if (elapsed_ms < 100) {
        printf("  ✓ Suspend time within budget (<100ms)\n");
    } else {
        printf("  ✗ Suspend time exceeded budget: %lu ms (target: <100ms)\n", 
               (unsigned long)elapsed_ms);
    }
    
    // Resume for cleanup
    result = lgx_runtime_resume();
    (void)result; // Suppress unused warning
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Resume time budget (<100ms)
static void test_resume_time_budget(void) {
    printf("Test 3: Resume time budget (<100ms)\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    // Suspend first
    result = lgx_runtime_suspend();
    assert(result == LGX_SUCCESS);
    
    // Measure resume time
    uint64_t start = get_time_ns();
    result = lgx_runtime_resume();
    uint64_t end = get_time_ns();
    
    assert(result == LGX_SUCCESS);
    
    uint64_t elapsed_ns = end - start;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    
    printf("  Resume time: %lu ms\n", (unsigned long)elapsed_ms);
    
    // Validate <100ms budget (Tier 1 requirement)
    if (elapsed_ms < 100) {
        printf("  ✓ Resume time within budget (<100ms)\n");
    } else {
        printf("  ✗ Resume time exceeded budget: %lu ms (target: <100ms)\n", 
               (unsigned long)elapsed_ms);
    }
    
    // Cleanup
    result = lgx_runtime_shutdown();
    (void)result; // Suppress unused warning
    lgx_config_destroy(config);
}

// Test 4: Multiple suspend/resume cycles
static void test_multiple_cycles(void) {
    printf("Test 4: Multiple suspend/resume cycles\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    // Perform 10 suspend/resume cycles
    for (int i = 0; i < 10; i++) {
        result = lgx_runtime_suspend();
        assert(result == LGX_SUCCESS);
        
        result = lgx_runtime_resume();
        assert(result == LGX_SUCCESS);
    }
    
    printf("  ✓ 10 suspend/resume cycles completed successfully\n");
    
    // Cleanup
    result = lgx_runtime_shutdown();
    (void)result; // Suppress unused warning
    lgx_config_destroy(config);
}

// Test 5: Error handling - double suspend
static void test_double_suspend(void) {
    printf("Test 5: Error handling - double suspend\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    // First suspend should succeed
    result = lgx_runtime_suspend();
    assert(result == LGX_SUCCESS);
    
    // Second suspend should fail
    result = lgx_runtime_suspend();
    assert(result == LGX_ERROR_INVALID_STATE);
    
    printf("  ✓ Double suspend correctly rejected\n");
    
    // Resume and cleanup
    result = lgx_runtime_resume();
    assert(result == LGX_SUCCESS);
    result = lgx_runtime_shutdown();
    (void)result; // Suppress unused warning
    lgx_config_destroy(config);
}

// Test 6: Error handling - resume without suspend
static void test_resume_without_suspend(void) {
    printf("Test 6: Error handling - resume without suspend\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    // Resume without suspend should fail
    result = lgx_runtime_resume();
    assert(result == LGX_ERROR_INVALID_STATE);
    
    printf("  ✓ Resume without suspend correctly rejected\n");
    
    // Cleanup
    result = lgx_runtime_shutdown();
    (void)result; // Suppress unused warning
    lgx_config_destroy(config);
}

// Test 7: State preservation across suspend/resume
static void test_state_preservation(void) {
    printf("Test 7: State preservation across suspend/resume\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    // Get initial counter values
    uint64_t allocs_before = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
    
    // Suspend
    result = lgx_runtime_suspend();
    assert(result == LGX_SUCCESS);
    
    // Resume
    result = lgx_runtime_resume();
    assert(result == LGX_SUCCESS);
    
    // Verify counters unchanged
    uint64_t allocs_after = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
    if (allocs_before != allocs_after) {
        printf("  ✗ State NOT preserved: allocs changed from %lu to %lu\n",
               (unsigned long)allocs_before, (unsigned long)allocs_after);
        assert(0);
    }
    
    printf("  ✓ State preserved across suspend/resume (allocs: %lu)\n", 
           (unsigned long)allocs_before);
    
    // Cleanup
    result = lgx_runtime_shutdown();
    (void)result; // Suppress unused warning
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== Suspend/Resume Lifecycle Tests ===\n\n");
    
    test_basic_suspend_resume();
    test_suspend_time_budget();
    test_resume_time_budget();
    test_multiple_cycles();
    test_double_suspend();
    test_resume_without_suspend();
    test_state_preservation();
    
    printf("\n=== All Suspend/Resume Tests Passed ===\n");
    return 0;
}
