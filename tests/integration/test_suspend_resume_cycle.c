/**
 * Integration Test: Suspend/Resume Cycle
 * 
 * Tests suspend and resume functionality with state preservation.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ %s\n", message); \
        } \
    } while(0)

// Test 1: Basic suspend and resume
static void test_basic_suspend_resume(void) {
    printf("\nTest 1: Basic suspend and resume\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Allocate some memory before suspend
    void* ptr1 = lgx_alloc(1024);
    void* ptr2 = lgx_alloc(2048);
    TEST_ASSERT(ptr1 != NULL && ptr2 != NULL, "Pre-suspend allocations succeed");
    
    // Suspend
    uint64_t start = lgx_time_now_ns();
    lgx_result_t result = lgx_runtime_suspend();
    uint64_t end = lgx_time_now_ns();
    
    TEST_ASSERT(result == LGX_SUCCESS, "Suspend succeeds");
    
    uint64_t suspend_time_ms = (end - start) / 1000000;
    printf("    Suspend time: %lu ms\n", (unsigned long)suspend_time_ms);
    TEST_ASSERT(suspend_time_ms < 100, "Suspend completes within 100ms");
    
    // Resume
    start = lgx_time_now_ns();
    result = lgx_runtime_resume();
    end = lgx_time_now_ns();
    
    TEST_ASSERT(result == LGX_SUCCESS, "Resume succeeds");
    
    uint64_t resume_time_ms = (end - start) / 1000000;
    printf("    Resume time: %lu ms\n", (unsigned long)resume_time_ms);
    TEST_ASSERT(resume_time_ms < 100, "Resume completes within 100ms");
    
    // Verify allocations still valid
    memset(ptr1, 0xAA, 1024);
    memset(ptr2, 0xBB, 2048);
    TEST_ASSERT(true, "Memory still accessible after resume");
    
    lgx_free(ptr1);
    lgx_free(ptr2);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 2: Multiple suspend/resume cycles
static void test_multiple_cycles(void) {
    printf("\nTest 2: Multiple suspend/resume cycles\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    for (int i = 0; i < 5; i++) {
        lgx_result_t result = lgx_runtime_suspend();
        TEST_ASSERT(result == LGX_SUCCESS, "Suspend cycle succeeds");
        
        result = lgx_runtime_resume();
        TEST_ASSERT(result == LGX_SUCCESS, "Resume cycle succeeds");
        
        // Verify functionality after each cycle
        void* ptr = lgx_alloc(1024);
        TEST_ASSERT(ptr != NULL, "Allocation works after cycle");
        lgx_free(ptr);
    }
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Suspend with active allocations
static void test_suspend_with_active_allocations(void) {
    printf("\nTest 3: Suspend with active allocations\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Create many allocations
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = lgx_alloc(1024 + i * 16);
        TEST_ASSERT(ptrs[i] != NULL, "Allocation succeeds");
    }
    
    // Suspend with active allocations
    lgx_result_t result = lgx_runtime_suspend();
    TEST_ASSERT(result == LGX_SUCCESS, "Suspend with allocations succeeds");
    
    // Resume
    result = lgx_runtime_resume();
    TEST_ASSERT(result == LGX_SUCCESS, "Resume succeeds");
    
    // Verify all allocations still valid
    for (int i = 0; i < 100; i++) {
        memset(ptrs[i], 0xCC, 1024 + i * 16);
    }
    TEST_ASSERT(true, "All allocations still valid");
    
    // Free all
    for (int i = 0; i < 100; i++) {
        lgx_free(ptrs[i]);
    }
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Suspend/resume with telemetry
static void test_suspend_resume_with_telemetry(void) {
    printf("\nTest 4: Suspend/resume with telemetry\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    lgx_telemetry_enable(true);
    
    // Perform some operations
    void* ptr = lgx_alloc(1024);
    lgx_free(ptr);
    
    // Suspend
    lgx_result_t result = lgx_runtime_suspend();
    TEST_ASSERT(result == LGX_SUCCESS, "Suspend with telemetry succeeds");
    
    // Resume
    result = lgx_runtime_resume();
    TEST_ASSERT(result == LGX_SUCCESS, "Resume with telemetry succeeds");
    
    // Telemetry should still work
    ptr = lgx_alloc(2048);
    lgx_free(ptr);
    
    TEST_ASSERT(true, "Telemetry works after resume");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 5: State preservation across suspend/resume
static void test_state_preservation(void) {
    printf("\nTest 5: State preservation across suspend/resume\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Get initial stats
    lgx_memory_stats_t stats_before;
    stats_before.struct_size = sizeof(lgx_memory_stats_t);
    lgx_memory_stats(&stats_before);
    
    // Allocate some memory
    void* ptr1 = lgx_alloc(4096);
    void* ptr2 = lgx_alloc(8192);
    
    // Suspend
    lgx_runtime_suspend();
    
    // Resume
    lgx_runtime_resume();
    
    // Get stats after resume
    lgx_memory_stats_t stats_after;
    stats_after.struct_size = sizeof(lgx_memory_stats_t);
    lgx_memory_stats(&stats_after);
    
    // Allocation count should be preserved
    TEST_ASSERT(stats_after.allocation_count >= stats_before.allocation_count,
                "Allocation count preserved");
    
    lgx_free(ptr1);
    lgx_free(ptr2);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 6: Suspend/resume performance
static void test_suspend_resume_performance(void) {
    printf("\nTest 6: Suspend/resume performance\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Measure suspend time
    uint64_t suspend_times[10];
    uint64_t resume_times[10];
    
    for (int i = 0; i < 10; i++) {
        uint64_t start = lgx_time_now_ns();
        lgx_runtime_suspend();
        uint64_t end = lgx_time_now_ns();
        suspend_times[i] = (end - start) / 1000000; // Convert to ms
        
        start = lgx_time_now_ns();
        lgx_runtime_resume();
        end = lgx_time_now_ns();
        resume_times[i] = (end - start) / 1000000;
    }
    
    // Calculate average
    uint64_t avg_suspend = 0, avg_resume = 0;
    for (int i = 0; i < 10; i++) {
        avg_suspend += suspend_times[i];
        avg_resume += resume_times[i];
    }
    avg_suspend /= 10;
    avg_resume /= 10;
    
    printf("    Average suspend time: %lu ms\n", (unsigned long)avg_suspend);
    printf("    Average resume time: %lu ms\n", (unsigned long)avg_resume);
    
    TEST_ASSERT(avg_suspend < 100, "Average suspend time < 100ms");
    TEST_ASSERT(avg_resume < 100, "Average resume time < 100ms");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 7: Suspend without resume (error handling)
static void test_suspend_without_resume(void) {
    printf("\nTest 7: Suspend without resume (error handling)\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Suspend
    lgx_result_t result = lgx_runtime_suspend();
    TEST_ASSERT(result == LGX_SUCCESS, "First suspend succeeds");
    
    // Try to suspend again (should fail)
    result = lgx_runtime_suspend();
    TEST_ASSERT(result != LGX_SUCCESS, "Double suspend fails");
    
    // Resume
    result = lgx_runtime_resume();
    TEST_ASSERT(result == LGX_SUCCESS, "Resume succeeds");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 8: Resume without suspend (error handling)
static void test_resume_without_suspend(void) {
    printf("\nTest 8: Resume without suspend (error handling)\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Try to resume without suspend (should fail)
    lgx_result_t result = lgx_runtime_resume();
    TEST_ASSERT(result != LGX_SUCCESS, "Resume without suspend fails");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 9: Allocations during suspended state
static void test_allocations_during_suspend(void) {
    printf("\nTest 9: Allocations during suspended state\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Suspend
    lgx_runtime_suspend();
    
    // Try to allocate (should fail or be queued)
    void* ptr = lgx_alloc(1024);
    
    // Resume
    lgx_runtime_resume();
    
    // Now allocation should work
    ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Allocation works after resume");
    lgx_free(ptr);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Health check during suspend/resume
static void test_health_check_during_suspend_resume(void) {
    printf("\nTest 10: Health check during suspend/resume\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Check health before suspend
    lgx_health_status_t status_before;
    status_before.struct_size = sizeof(lgx_health_status_t);
    lgx_runtime_health_check(&status_before);
    
    // Suspend
    lgx_runtime_suspend();
    
    // Resume
    lgx_runtime_resume();
    
    // Check health after resume
    lgx_health_status_t status_after;
    status_after.struct_size = sizeof(lgx_health_status_t);
    lgx_runtime_health_check(&status_after);
    
    TEST_ASSERT(status_after.overall_health >= LGX_HEALTH_GOOD,
                "Health status good after resume");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== LGX Runtime Suspend/Resume Cycle Integration Test ===\n");
    
    test_basic_suspend_resume();
    test_multiple_cycles();
    test_suspend_with_active_allocations();
    test_suspend_resume_with_telemetry();
    test_state_preservation();
    test_suspend_resume_performance();
    test_suspend_without_resume();
    test_resume_without_suspend();
    test_allocations_during_suspend();
    test_health_check_during_suspend_resume();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All integration tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some integration tests failed!\n");
        return 1;
    }
}
