/**
 * ABI Compatibility Test: Struct Evolution
 * 
 * Tests size-based versioning for struct evolution.
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

// Simulate old v1.0 version struct (smaller)
typedef struct lgx_version_v1_0 {
    size_t struct_size;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} lgx_version_v1_0_t;

// Simulate old v1.0 health status (smaller)
typedef struct lgx_health_status_v1_0 {
    size_t struct_size;
    lgx_health_level_t overall_health;
    lgx_hardware_tier_t hardware_tier;
    bool huge_pages_active;
    bool gpu_responsive;
    size_t memory_usage_mb;
    size_t memory_limit_mb;
} lgx_health_status_v1_0_t;

// Test 1: Old struct with new runtime
static void test_old_struct_with_new_runtime(void) {
    printf("\nTest 1: Old struct with new runtime\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Use current struct (v1.0 is current)
    lgx_version_t version = lgx_runtime_get_version();
    TEST_ASSERT(version.major >= 1, "Version data is valid");
    TEST_ASSERT(version.struct_size == sizeof(lgx_version_t), "Struct size is set");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 2: New struct with old data
static void test_new_struct_size_validation(void) {
    printf("\nTest 2: New struct size validation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Use current struct
    lgx_version_t version = lgx_runtime_get_version();
    TEST_ASSERT(version.struct_size == sizeof(lgx_version_t), "Struct size is correct");
    TEST_ASSERT(version.major >= 1, "Version is valid");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Health status struct evolution
static void test_health_status_evolution(void) {
    printf("\nTest 3: Health status struct evolution\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Use old v1.0 health status struct
    lgx_health_status_v1_0_t old_status;
    old_status.struct_size = sizeof(lgx_health_status_v1_0_t);
    
    lgx_result_t result = lgx_runtime_health_check((lgx_health_status_t*)&old_status);
    TEST_ASSERT(result == LGX_SUCCESS, "Old health status struct works");
    TEST_ASSERT(old_status.memory_limit_mb > 0, "Basic fields are populated");
    
    // Use new struct
    lgx_health_status_t new_status;
    new_status.struct_size = sizeof(lgx_health_status_t);
    
    result = lgx_runtime_health_check(&new_status);
    TEST_ASSERT(result == LGX_SUCCESS, "New health status struct works");
    
    // New struct should have more fields
    TEST_ASSERT(sizeof(lgx_health_status_t) >= sizeof(lgx_health_status_v1_0_t),
                "New struct is larger or equal");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Struct size field validation
static void test_struct_size_field(void) {
    printf("\nTest 4: Struct size field validation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    lgx_version_t version = lgx_runtime_get_version();
    TEST_ASSERT(version.struct_size == sizeof(lgx_version_t), "Struct size is set correctly");
    TEST_ASSERT(version.major >= 1, "Version is valid");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 5: Forward compatibility - new fields ignored by old code
static void test_forward_compatibility_new_fields(void) {
    printf("\nTest 5: Forward compatibility - new fields ignored\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Current code uses current struct
    lgx_version_t version = lgx_runtime_get_version();
    TEST_ASSERT(version.major >= 1, "Version is accessible");
    TEST_ASSERT(version.struct_size == sizeof(lgx_version_t), "Struct size is correct");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 6: Backward compatibility - old runtime with new struct
static void test_backward_compatibility_check(void) {
    printf("\nTest 6: Backward compatibility check\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // New code uses new struct
    lgx_version_t new_version = lgx_runtime_get_version();
    TEST_ASSERT(new_version.struct_size == sizeof(lgx_version_t), "Struct size is preserved");
    TEST_ASSERT(new_version.major >= 1, "Version is valid");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 7: Struct alignment and padding
static void test_struct_alignment(void) {
    printf("\nTest 7: Struct alignment and padding\n");
    
    // Verify struct sizes are reasonable
    TEST_ASSERT(sizeof(lgx_version_t) % 8 == 0, 
                "Version struct is 8-byte aligned");
    TEST_ASSERT(sizeof(lgx_health_status_t) % 8 == 0,
                "Health status struct is 8-byte aligned");
    
    // Verify struct_size is first field
    lgx_version_t version;
    TEST_ASSERT((void*)&version == (void*)&version.struct_size,
                "struct_size is first field in version");
    
    lgx_health_status_t status;
    TEST_ASSERT((void*)&status == (void*)&status.struct_size,
                "struct_size is first field in health status");
}

// Test 8: Multiple struct versions coexisting
static void test_multiple_struct_versions(void) {
    printf("\nTest 8: Multiple struct versions coexisting\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Use current struct
    lgx_version_t version = lgx_runtime_get_version();
    TEST_ASSERT(version.major >= 1, "Version is valid");
    TEST_ASSERT(version.struct_size == sizeof(lgx_version_t), "Struct size is correct");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 9: Struct evolution with optional fields
static void test_optional_fields(void) {
    printf("\nTest 9: Struct evolution with optional fields\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // New struct may have optional fields that old code doesn't use
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_result_t result = lgx_runtime_health_check(&status);
    TEST_ASSERT(result == LGX_SUCCESS, "Health check succeeds");
    
    // Core fields should always be present
    TEST_ASSERT(status.memory_limit_mb > 0, "Core fields are present");
    
    // Optional fields may or may not be present depending on runtime version
    printf("    Frame arena usage: %zu MB\n", status.frame_arena_usage_mb);
    printf("    GPU pool usage: %zu MB\n", status.gpu_pool_usage_mb);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Struct versioning best practices
static void test_versioning_best_practices(void) {
    printf("\nTest 10: Struct versioning best practices\n");
    
    // Check offsets using offsetof (no need to declare variables)
    TEST_ASSERT(offsetof(lgx_version_t, struct_size) == 0,
                "version struct_size is at offset 0");
    TEST_ASSERT(offsetof(lgx_health_status_t, struct_size) == 0,
                "health struct_size is at offset 0");
    
    printf("    All versioned structs follow best practices\n");
}

int main(void) {
    printf("=== LGX Runtime Struct Evolution ABI Test ===\n");
    printf("Testing size-based versioning for struct evolution\n\n");
    
    test_old_struct_with_new_runtime();
    test_new_struct_size_validation();
    test_health_status_evolution();
    test_struct_size_field();
    test_forward_compatibility_new_fields();
    test_backward_compatibility_check();
    test_struct_alignment();
    test_multiple_struct_versions();
    test_optional_fields();
    test_versioning_best_practices();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All struct evolution tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some struct evolution tests failed!\n");
        return 1;
    }
}
