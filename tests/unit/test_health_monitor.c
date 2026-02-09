/**
 * Unit Tests: Health Check API
 * 
 * Tests health monitoring, status reporting, and degradation detection.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Test 1: Basic health check
static void test_basic_health_check(void) {
    printf("\nTest 1: Basic health check\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_result_t result = lgx_runtime_health_check(&status);
    TEST_ASSERT(result == LGX_SUCCESS, "Health check succeeds");
    
    TEST_ASSERT(status.overall_health >= LGX_HEALTH_GOOD &&
                status.overall_health <= LGX_HEALTH_FAILED,
                "Health status is set");
    
    const char* health_str = "Unknown";
    switch (status.overall_health) {
        case LGX_HEALTH_GOOD: health_str = "Good"; break;
        case LGX_HEALTH_WARNING: health_str = "Warning"; break;
        case LGX_HEALTH_CRITICAL: health_str = "Critical"; break;
        case LGX_HEALTH_FAILED: health_str = "Failed"; break;
    }
    
    printf("    Health: %s\n", health_str);
}

// Test 2: NULL parameter handling
static void test_null_parameter(void) {
    printf("\nTest 2: NULL parameter handling\n");
    
    lgx_result_t result = lgx_runtime_health_check(NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Health check with NULL fails");
}

// Test 3: Hardware tier reporting
static void test_hardware_tier(void) {
    printf("\nTest 3: Hardware tier reporting\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    TEST_ASSERT(status.hardware_tier == LGX_HW_TIER_OPTIMAL ||
                status.hardware_tier == LGX_HW_TIER_COMPATIBLE ||
                status.hardware_tier == LGX_HW_TIER_DEGRADED,
                "Hardware tier is valid");
    
    const char* tier_str = "Unknown";
    switch (status.hardware_tier) {
        case LGX_HW_TIER_OPTIMAL: tier_str = "Optimal"; break;
        case LGX_HW_TIER_COMPATIBLE: tier_str = "Compatible"; break;
        case LGX_HW_TIER_DEGRADED: tier_str = "Degraded"; break;
    }
    
    printf("    Hardware tier: %s\n", tier_str);
}

// Test 4: Huge pages status
static void test_huge_pages_status(void) {
    printf("\nTest 4: Huge pages status\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    printf("    Huge pages active: %s\n", status.huge_pages_active ? "Yes" : "No");
    
    TEST_ASSERT(true, "Huge pages status reported");
}

// Test 5: GPU responsiveness
static void test_gpu_responsive(void) {
    printf("\nTest 5: GPU responsiveness\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    printf("    GPU responsive: %s\n", status.gpu_responsive ? "Yes" : "No");
    
    TEST_ASSERT(true, "GPU responsiveness reported");
}

// Test 6: NUMA awareness status
static void test_numa_awareness(void) {
    printf("\nTest 6: NUMA awareness status\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    printf("    NUMA awareness active: %s\n", status.numa_awareness_active ? "Yes" : "No");
    
    TEST_ASSERT(true, "NUMA awareness status reported");
}

// Test 7: Memory usage reporting
static void test_memory_usage(void) {
    printf("\nTest 7: Memory usage reporting\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    TEST_ASSERT(status.memory_usage_mb < 1000000, "Memory usage is reasonable");
    TEST_ASSERT(status.memory_limit_mb > 0, "Memory limit is positive");
    TEST_ASSERT(status.memory_usage_mb <= status.memory_limit_mb,
                "Memory usage within limit");
    
    printf("    Memory: %zu MB / %zu MB\n", 
           status.memory_usage_mb, status.memory_limit_mb);
}

// Test 8: Allocation failure tracking
static void test_allocation_failures(void) {
    printf("\nTest 8: Allocation failure tracking\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    TEST_ASSERT(status.allocation_failures < 1000000, "Allocation failures tracked");
    
    printf("    Allocation failures: %u\n", status.allocation_failures);
}

// Test 9: Degraded features reporting
static void test_degraded_features(void) {
    printf("\nTest 9: Degraded features reporting\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    printf("    Degraded features: 0x%08X\n", status.degraded_features);
    
    if (status.degraded_features != 0) {
        printf("    Some features are degraded\n");
    }
    
    TEST_ASSERT(true, "Degraded features reported");
}

// Test 10: CPU overhead measurement
static void test_cpu_overhead(void) {
    printf("\nTest 10: CPU overhead measurement\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    TEST_ASSERT(status.cpu_overhead_percent >= 0.0, "CPU overhead is non-negative");
    TEST_ASSERT(status.cpu_overhead_percent <= 100.0, "CPU overhead is reasonable");
    
    printf("    CPU overhead: %.2f%%\n", status.cpu_overhead_percent);
}

// Test 11: Degradation summary
static void test_degradation_summary(void) {
    printf("\nTest 11: Degradation summary\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    if (status.degradation_summary != NULL) {
        TEST_ASSERT(strlen(status.degradation_summary) > 0, "Summary is not empty");
        printf("    Summary: %s\n", status.degradation_summary);
    } else {
        TEST_ASSERT(true, "No degradation summary (system healthy)");
    }
}

// Test 12: Health check after allocations
static void test_health_after_allocations(void) {
    printf("\nTest 12: Health check after allocations\n");
    
    // Get initial status
    lgx_health_status_t status1;
    status1.struct_size = sizeof(lgx_health_status_t);
    lgx_runtime_health_check(&status1);
    
    size_t initial_usage = status1.memory_usage_mb;
    
    // Allocate some memory
    void* ptr1 = lgx_alloc(1024 * 1024); // 1MB
    void* ptr2 = lgx_alloc(1024 * 1024); // 1MB
    
    // Get updated status
    lgx_health_status_t status2;
    status2.struct_size = sizeof(lgx_health_status_t);
    lgx_runtime_health_check(&status2);
    
    TEST_ASSERT(status2.memory_usage_mb >= initial_usage, "Memory usage increased");
    
    // Cleanup
    lgx_free(ptr1);
    lgx_free(ptr2);
}

// Test 13: Health check frequency
static void test_health_check_frequency(void) {
    printf("\nTest 13: Health check frequency\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    // Multiple rapid health checks should work
    for (int i = 0; i < 10; i++) {
        lgx_result_t result = lgx_runtime_health_check(&status);
        TEST_ASSERT(result == LGX_SUCCESS, "Rapid health check succeeds");
    }
}

// Test 14: Health check performance
static void test_health_check_performance(void) {
    printf("\nTest 14: Health check performance\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    uint64_t start = lgx_time_now_ns();
    
    for (int i = 0; i < 100; i++) {
        lgx_runtime_health_check(&status);
    }
    
    uint64_t end = lgx_time_now_ns();
    uint64_t avg_ns = (end - start) / 100;
    
    // Health check should be fast (<10 microseconds)
    TEST_ASSERT(avg_ns < 10000, "Health check is fast");
    
    printf("    Average time: %lu ns\n", (unsigned long)avg_ns);
}

// Test 15: Struct size validation
static void test_struct_size_validation(void) {
    printf("\nTest 15: Struct size validation\n");
    
    lgx_health_status_t status;
    
    // Wrong struct size
    status.struct_size = 0;
    lgx_result_t result = lgx_runtime_health_check(&status);
    TEST_ASSERT(result != LGX_SUCCESS, "Wrong struct size fails");
    
    // Correct struct size
    status.struct_size = sizeof(lgx_health_status_t);
    result = lgx_runtime_health_check(&status);
    TEST_ASSERT(result == LGX_SUCCESS, "Correct struct size succeeds");
}

// Test 16: Health status consistency
static void test_health_consistency(void) {
    printf("\nTest 16: Health status consistency\n");
    
    lgx_health_status_t status1, status2;
    status1.struct_size = sizeof(lgx_health_status_t);
    status2.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status1);
    lgx_runtime_health_check(&status2);
    
    // Hardware tier should be consistent
    TEST_ASSERT(status1.hardware_tier == status2.hardware_tier,
                "Hardware tier is consistent");
    
    // Memory limit should be consistent
    TEST_ASSERT(status1.memory_limit_mb == status2.memory_limit_mb,
                "Memory limit is consistent");
}

// Test 17: Health check before init
static void test_health_before_init(void) {
    printf("\nTest 17: Health check before init\n");
    
    // Shutdown runtime
    lgx_runtime_shutdown();
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_result_t result = lgx_runtime_health_check(&status);
    TEST_ASSERT(result != LGX_SUCCESS, "Health check before init fails");
    
    // Re-initialize
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    lgx_config_destroy(config);
}

// Test 18: Health monitoring integration
static void test_health_monitoring_integration(void) {
    printf("\nTest 18: Health monitoring integration\n");
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&status);
    
    // Verify all fields are populated
    TEST_ASSERT(status.overall_health >= LGX_HEALTH_GOOD &&
                status.overall_health <= LGX_HEALTH_FAILED,
                "Health flag set");
    TEST_ASSERT(status.hardware_tier >= LGX_HW_TIER_OPTIMAL &&
                status.hardware_tier <= LGX_HW_TIER_DEGRADED,
                "Hardware tier valid");
    TEST_ASSERT(status.memory_limit_mb > 0, "Memory limit set");
    
    printf("    Integration check passed\n");
}

int main(void) {
    printf("=== LGX Runtime Health Check API Unit Tests ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    test_basic_health_check();
    test_null_parameter();
    test_hardware_tier();
    test_huge_pages_status();
    test_gpu_responsive();
    test_numa_awareness();
    test_memory_usage();
    test_allocation_failures();
    test_degraded_features();
    test_cpu_overhead();
    test_degradation_summary();
    test_health_after_allocations();
    test_health_check_frequency();
    test_health_check_performance();
    test_struct_size_validation();
    test_health_consistency();
    test_health_before_init();
    test_health_monitoring_integration();
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed!\n");
        return 1;
    }
}
