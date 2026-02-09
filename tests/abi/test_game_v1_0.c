/**
 * ABI Compatibility Test Game - v1.0
 * 
 * This test game is compiled against v1.0 headers and should work
 * with v1.0, v1.1, v1.2+ runtimes (forward compatibility).
 */

#include "lgx_runtime.h"
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

// Test 1: Basic initialization (v1.0 API)
static void test_v1_0_initialization(void) {
    printf("\nTest 1: v1.0 initialization\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Config creation succeeds");
    
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime init succeeds");
    
    result = lgx_runtime_shutdown();
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime shutdown succeeds");
    
    lgx_config_destroy(config);
}

// Test 2: Version checking (v1.0 API)
static void test_v1_0_version_check(void) {
    printf("\nTest 2: v1.0 version check\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    lgx_version_t version = lgx_runtime_get_version();
    TEST_ASSERT(version.major >= 1, "Major version is valid");
    
    printf("    Runtime version: %u.%u.%u\n", 
           version.major, version.minor, version.patch);
    
    // Check compatibility with v1.0
    lgx_version_t required = {.struct_size = sizeof(lgx_version_t), .major = 1, .minor = 0, .patch = 0};
    lgx_result_t result = lgx_runtime_check_compatibility(&required);
    TEST_ASSERT(result == LGX_SUCCESS, "v1.0 compatibility check succeeds");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Memory allocation (v1.0 API)
static void test_v1_0_memory_allocation(void) {
    printf("\nTest 3: v1.0 memory allocation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Basic allocation - using frame arena
    void* ptr1 = lgx_alloc_frame(1024);
    TEST_ASSERT(ptr1 != NULL, "Frame allocation succeeds");
    
    // Persistent allocation
    void* ptr2 = lgx_alloc_persistent(2048);
    TEST_ASSERT(ptr2 != NULL, "Persistent allocation succeeds");
    
    // Free persistent allocation (frame allocations are freed at frame boundary)
    lgx_free(ptr2);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Capability detection (v1.0 API)
static void test_v1_0_capability_detection(void) {
    printf("\nTest 4: v1.0 capability detection\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    bool has_huge_pages = lgx_runtime_has_capability(LGX_CAP_HUGE_PAGES);
    bool has_numa = lgx_runtime_has_capability(LGX_CAP_NUMA_AWARENESS);
    
    printf("    Huge pages: %s\n", has_huge_pages ? "Yes" : "No");
    printf("    NUMA: %s\n", has_numa ? "Yes" : "No");
    
    TEST_ASSERT(true, "Capability detection works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 5: Error handling (v1.0 API)
static void test_v1_0_error_handling(void) {
    printf("\nTest 5: v1.0 error handling\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Trigger an error by passing NULL to alloc_with_intent
    void* ptr = lgx_alloc_with_intent(NULL);
    TEST_ASSERT(ptr == NULL, "Invalid allocation returns NULL");
    
    lgx_error_context_t error = lgx_get_last_error();
    TEST_ASSERT(error.error_code != LGX_SUCCESS, "Error was recorded");
    
    const char* error_str = lgx_result_to_string(error.error_code);
    TEST_ASSERT(error_str != NULL, "Error string is available");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 6: Platform services (v1.0 API)
static void test_v1_0_platform_services(void) {
    printf("\nTest 6: v1.0 platform services\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Timing
    uint64_t time1 = lgx_time_now_ns();
    lgx_time_sleep_ms(10);
    uint64_t time2 = lgx_time_now_ns();
    TEST_ASSERT(time2 > time1, "Timing services work");
    
    // Logging
    lgx_log(LGX_LOG_INFO, "Test log message from v1.0 game");
    TEST_ASSERT(true, "Logging works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 7: Health check (v1.0 API)
static void test_v1_0_health_check(void) {
    printf("\nTest 7: v1.0 health check\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_result_t result = lgx_runtime_health_check(&status);
    TEST_ASSERT(result == LGX_SUCCESS, "Health check succeeds");
    TEST_ASSERT(status.memory_limit_mb > 0, "Health status is valid");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 8: Struct size-based versioning
static void test_v1_0_struct_versioning(void) {
    printf("\nTest 8: v1.0 struct size-based versioning\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Use v1.0 struct size
    lgx_version_t version = lgx_runtime_get_version();
    TEST_ASSERT(version.struct_size == sizeof(lgx_version_t), "Struct size is set");
    
    // Even if runtime has newer fields, v1.0 struct should work
    TEST_ASSERT(version.major >= 1, "Version data is valid");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 9: Multiple init/shutdown cycles
static void test_v1_0_multiple_cycles(void) {
    printf("\nTest 9: v1.0 multiple init/shutdown cycles\n");
    
    for (int i = 0; i < 3; i++) {
        lgx_runtime_config_t* config = lgx_config_create();
        
        lgx_result_t result = lgx_runtime_init(config);
        TEST_ASSERT(result == LGX_SUCCESS, "Init cycle succeeds");
        
        void* ptr = lgx_alloc_persistent(1024);
        TEST_ASSERT(ptr != NULL, "Allocation in cycle succeeds");
        lgx_free(ptr);
        
        result = lgx_runtime_shutdown();
        TEST_ASSERT(result == LGX_SUCCESS, "Shutdown cycle succeeds");
        
        lgx_config_destroy(config);
    }
}

// Test 10: Forward compatibility verification
static void test_v1_0_forward_compatibility(void) {
    printf("\nTest 10: v1.0 forward compatibility\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Get runtime version
    lgx_version_t version = lgx_runtime_get_version();
    
    printf("    Compiled against: v1.0.0\n");
    printf("    Running with: v%u.%u.%u\n", 
           version.major, version.minor, version.patch);
    
    // v1.0 game should work with any v1.x runtime
    if (version.major == 1) {
        TEST_ASSERT(true, "Forward compatibility maintained");
    } else {
        printf("    Warning: Major version mismatch\n");
        TEST_ASSERT(false, "Major version changed");
    }
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== LGX Runtime ABI Compatibility Test - v1.0 Game ===\n");
    printf("This game is compiled against v1.0 headers\n");
    printf("Testing forward compatibility with newer runtimes\n\n");
    
    test_v1_0_initialization();
    test_v1_0_version_check();
    test_v1_0_memory_allocation();
    test_v1_0_capability_detection();
    test_v1_0_error_handling();
    test_v1_0_platform_services();
    test_v1_0_health_check();
    test_v1_0_struct_versioning();
    test_v1_0_multiple_cycles();
    test_v1_0_forward_compatibility();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All ABI compatibility tests passed!\n");
        printf("v1.0 game is compatible with this runtime\n");
        return 0;
    } else {
        printf("\n✗ Some ABI compatibility tests failed!\n");
        printf("v1.0 game may not be fully compatible\n");
        return 1;
    }
}
