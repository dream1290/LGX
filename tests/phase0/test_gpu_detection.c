/**
 * GPU Detection Test
 * 
 * Tests GPU vendor and driver version detection (Tasks 2.4.3 and 2.4.4)
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while (0)

void test_gpu_detection(void) {
    printf("\n[TEST] gpu_detection\n");
    
    // Initialize hardware adapter
    lgx_hardware_adapter_t* adapter = NULL;
    lgx_result_t result = lgx_hardware_adapter_init(&adapter);
    TEST_ASSERT(result == LGX_SUCCESS, "Hardware adapter initialization");
    
    if (result != LGX_SUCCESS) {
        return;
    }
    
    // Check if GPU is available
    bool has_gpu = lgx_hardware_adapter_has_gpu(adapter);
    printf("  GPU Available: %s\n", has_gpu ? "Yes" : "No");
    
    // Get GPU vendor
    const char* vendor = lgx_hardware_adapter_get_gpu_vendor(adapter);
    TEST_ASSERT(vendor != NULL, "GPU vendor string is not NULL");
    printf("  GPU Vendor: %s\n", vendor);
    
    // Get driver version
    const char* driver = lgx_hardware_adapter_get_driver_version(adapter);
    TEST_ASSERT(driver != NULL, "Driver version string is not NULL");
    printf("  Driver Version: %s\n", driver);
    
    // Validate vendor is one of the known values
    bool valid_vendor = (strcmp(vendor, "NVIDIA") == 0 ||
                        strcmp(vendor, "AMD") == 0 ||
                        strcmp(vendor, "Intel") == 0 ||
                        strcmp(vendor, "AMD/Intel") == 0 ||
                        strcmp(vendor, "Unknown") == 0);
    TEST_ASSERT(valid_vendor, "GPU vendor is a known value");
    
    // If GPU is available, vendor should not be "Unknown"
    if (has_gpu) {
        TEST_ASSERT(strcmp(vendor, "Unknown") != 0, 
                   "GPU available implies vendor is detected");
    }
    
    // Driver version should not be empty
    TEST_ASSERT(strlen(driver) > 0, "Driver version is not empty");
    
    // Cleanup
    lgx_hardware_adapter_shutdown(adapter);
    printf("  Test completed\n");
}

void test_hardware_status_includes_gpu_info(void) {
    printf("\n[TEST] hardware_status_includes_gpu_info\n");
    
    // Initialize hardware adapter
    lgx_hardware_adapter_t* adapter = NULL;
    lgx_result_t result = lgx_hardware_adapter_init(&adapter);
    TEST_ASSERT(result == LGX_SUCCESS, "Hardware adapter initialization");
    
    if (result != LGX_SUCCESS) {
        return;
    }
    
    // Get hardware status
    lgx_hardware_status_t status = lgx_hardware_adapter_get_status(adapter);
    TEST_ASSERT(status.struct_size == sizeof(lgx_hardware_status_t), 
               "Hardware status struct size is correct");
    
    // Check tier classification
    bool valid_tier = (status.achieved_tier == LGX_HW_TIER_OPTIMAL ||
                      status.achieved_tier == LGX_HW_TIER_COMPATIBLE ||
                      status.achieved_tier == LGX_HW_TIER_DEGRADED);
    TEST_ASSERT(valid_tier, "Hardware tier is valid");
    
    printf("  Hardware Tier: %s\n", 
           status.achieved_tier == LGX_HW_TIER_OPTIMAL ? "OPTIMAL" :
           status.achieved_tier == LGX_HW_TIER_COMPATIBLE ? "COMPATIBLE" : "DEGRADED");
    
    // If no GPU, tier should be DEGRADED
    bool has_gpu = lgx_hardware_adapter_has_gpu(adapter);
    if (!has_gpu) {
        TEST_ASSERT(status.achieved_tier == LGX_HW_TIER_DEGRADED,
                   "No GPU implies DEGRADED tier");
        TEST_ASSERT(strstr(status.degradation_reason, "No GPU") != NULL,
                   "Degradation reason mentions GPU");
    }
    
    // Cleanup
    lgx_hardware_adapter_shutdown(adapter);
    printf("  Test completed\n");
}

void test_multiple_gpu_queries(void) {
    printf("\n[TEST] multiple_gpu_queries\n");
    
    // Initialize hardware adapter
    lgx_hardware_adapter_t* adapter = NULL;
    lgx_result_t result = lgx_hardware_adapter_init(&adapter);
    TEST_ASSERT(result == LGX_SUCCESS, "Hardware adapter initialization");
    
    if (result != LGX_SUCCESS) {
        return;
    }
    
    // Query GPU info multiple times - should be consistent
    const char* vendor1 = lgx_hardware_adapter_get_gpu_vendor(adapter);
    const char* vendor2 = lgx_hardware_adapter_get_gpu_vendor(adapter);
    const char* vendor3 = lgx_hardware_adapter_get_gpu_vendor(adapter);
    
    TEST_ASSERT(strcmp(vendor1, vendor2) == 0, "Vendor query is consistent (1st vs 2nd)");
    TEST_ASSERT(strcmp(vendor2, vendor3) == 0, "Vendor query is consistent (2nd vs 3rd)");
    
    const char* driver1 = lgx_hardware_adapter_get_driver_version(adapter);
    const char* driver2 = lgx_hardware_adapter_get_driver_version(adapter);
    const char* driver3 = lgx_hardware_adapter_get_driver_version(adapter);
    
    TEST_ASSERT(strcmp(driver1, driver2) == 0, "Driver query is consistent (1st vs 2nd)");
    TEST_ASSERT(strcmp(driver2, driver3) == 0, "Driver query is consistent (2nd vs 3rd)");
    
    // Cleanup
    lgx_hardware_adapter_shutdown(adapter);
    printf("  Test completed\n");
}

void test_null_adapter_handling(void) {
    printf("\n[TEST] null_adapter_handling\n");
    
    // Test NULL adapter handling
    bool has_gpu = lgx_hardware_adapter_has_gpu(NULL);
    TEST_ASSERT(has_gpu == false, "NULL adapter returns false for has_gpu");
    
    const char* vendor = lgx_hardware_adapter_get_gpu_vendor(NULL);
    TEST_ASSERT(strcmp(vendor, "Unknown") == 0, "NULL adapter returns 'Unknown' vendor");
    
    const char* driver = lgx_hardware_adapter_get_driver_version(NULL);
    TEST_ASSERT(strcmp(driver, "Unknown") == 0, "NULL adapter returns 'Unknown' driver");
    
    printf("  Test completed\n");
}

int main(void) {
    printf("=== GPU Detection Tests ===\n");
    printf("Testing GPU vendor and driver version detection (Tasks 2.4.3 and 2.4.4)\n");
    
    // Run tests
    test_gpu_detection();
    test_hardware_status_includes_gpu_info();
    test_multiple_gpu_queries();
    test_null_adapter_handling();
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n");
        return 1;
    }
}
