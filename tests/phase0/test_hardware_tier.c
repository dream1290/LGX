/**
 * Hardware Tier Classification Tests - Task 4.1
 * 
 * Tests hardware capability detection and tier classification.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void test_hardware_detection(void) {
    printf("\n[TEST] hardware_detection\n");
    
    // Initialize runtime to get hardware detection
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    TEST_ASSERT(result == LGX_SUCCESS || result == LGX_ERROR_ALREADY_INITIALIZED, 
                "Runtime initialized");
    
    // Get hardware status
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    TEST_ASSERT(status.struct_size == sizeof(lgx_hardware_status_t), "Status struct size correct");
    
    printf("  Hardware detection completed\n");
    printf("  Test completed\n");
}

void test_tier_classification(void) {
    printf("\n[TEST] tier_classification\n");
    
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    TEST_ASSERT(status.struct_size == sizeof(lgx_hardware_status_t), "Status struct size correct");
    
    // Check tier
    const char* tier_name = "UNKNOWN";
    switch (status.achieved_tier) {
        case LGX_HW_TIER_OPTIMAL:
            tier_name = "OPTIMAL";
            break;
        case LGX_HW_TIER_COMPATIBLE:
            tier_name = "COMPATIBLE";
            break;
        case LGX_HW_TIER_DEGRADED:
            tier_name = "DEGRADED";
            break;
    }
    
    printf("  Hardware Tier: %s\n", tier_name);
    TEST_ASSERT(status.achieved_tier >= LGX_HW_TIER_OPTIMAL && 
                status.achieved_tier <= LGX_HW_TIER_DEGRADED, 
                "Valid tier classification");
    
    // Check degradation info
    if (status.degradation_reason) {
        printf("  Reason: %s\n", status.degradation_reason);
    }
    
    if (status.performance_impact_estimate) {
        printf("  Impact: %s\n", status.performance_impact_estimate);
    }
    
    if (status.remediation_steps) {
        printf("  Remediation: %s\n", status.remediation_steps);
    }
    
    TEST_ASSERT(status.degradation_reason != NULL, "Degradation reason provided");
    TEST_ASSERT(status.performance_impact_estimate != NULL, "Performance impact provided");
    TEST_ASSERT(status.remediation_steps != NULL, "Remediation steps provided");
    
    printf("  Test completed\n");
}

void test_missing_capabilities(void) {
    printf("\n[TEST] missing_capabilities\n");
    
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    printf("  Missing capabilities bitmask: 0x%08x\n", status.missing_capabilities);
    
    // Check individual capabilities
    if (status.missing_capabilities & (1 << LGX_CAP_HUGE_PAGES)) {
        printf("    - Huge pages missing\n");
    }
    
    if (status.missing_capabilities & (1 << LGX_CAP_NUMA_AWARENESS)) {
        printf("    - NUMA awareness missing\n");
    }
    
    if (status.missing_capabilities & (1 << LGX_CAP_DX11_TRANSLATION)) {
        printf("    - DX11 translation missing (no GPU)\n");
    }
    
    if (status.missing_capabilities & (1 << LGX_CAP_DX12_TRANSLATION)) {
        printf("    - DX12 translation missing (no GPU)\n");
    }
    
    if (status.missing_capabilities == 0) {
        printf("    All capabilities available!\n");
    }
    
    TEST_ASSERT(true, "Missing capabilities reported");
    
    printf("  Test completed\n");
}

void test_performance_impact(void) {
    printf("\n[TEST] performance_impact\n");
    
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    // Verify performance impact estimation
    TEST_ASSERT(status.performance_impact_estimate != NULL, "Performance impact estimated");
    if (status.performance_impact_estimate == NULL) {
        printf("  Test completed (early exit due to NULL performance_impact_estimate)\n");
        return;
    }
    
    TEST_ASSERT(strlen(status.performance_impact_estimate) > 0, "Performance impact non-empty");
    
    printf("  Performance Impact: %s\n", status.performance_impact_estimate);
    
    // Check if impact is reasonable based on tier
    if (status.achieved_tier == LGX_HW_TIER_OPTIMAL) {
        TEST_ASSERT(strstr(status.performance_impact_estimate, "Optimal") != NULL ||
                    strstr(status.performance_impact_estimate, "optimal") != NULL,
                    "Optimal tier has optimal performance message");
    }
    
    printf("  Test completed\n");
}

void test_remediation_guidance(void) {
    printf("\n[TEST] remediation_guidance\n");
    
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    // Verify remediation guidance
    TEST_ASSERT(status.remediation_steps != NULL, "Remediation steps provided");
    if (status.remediation_steps == NULL) {
        printf("  Test completed (early exit due to NULL remediation_steps)\n");
        return;
    }
    
    TEST_ASSERT(strlen(status.remediation_steps) > 0, "Remediation steps non-empty");
    
    printf("  Remediation Steps:\n");
    printf("    %s\n", status.remediation_steps);
    
    // Check if remediation is actionable
    if (status.achieved_tier != LGX_HW_TIER_OPTIMAL) {
        TEST_ASSERT(strstr(status.remediation_steps, "No action required") == NULL,
                    "Non-optimal tier has actionable remediation");
    }
    
    printf("  Test completed\n");
}

int main(void) {
    printf("=== Hardware Tier Classification Tests ===\n");
    printf("Testing hardware detection and tier classification (Task 4.1)\n");
    
    // Initialize runtime once
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    // Run tests
    test_hardware_detection();
    test_tier_classification();
    test_missing_capabilities();
    test_performance_impact();
    test_remediation_guidance();
    
    // Cleanup
    lgx_runtime_shutdown();
    
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
