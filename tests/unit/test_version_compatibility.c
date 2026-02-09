/**
 * Unit Tests: Version and Compatibility
 * 
 * Tests version checking and compatibility validation.
 */

#include "lgx_runtime.h"
#include "lgx_version.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
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

// Test 1: Get runtime version
static void test_get_version(void) {
    printf("\nTest 1: Get runtime version\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    
    lgx_result_t result = lgx_runtime_get_version(&version);
    TEST_ASSERT(result == LGX_SUCCESS, "Get version succeeds");
    
    TEST_ASSERT(version.major >= 1, "Major version is valid");
    TEST_ASSERT(version.minor >= 0, "Minor version is valid");
    TEST_ASSERT(version.patch >= 0, "Patch version is valid");
    
    printf("    Runtime version: %u.%u.%u\n", version.major, version.minor, version.patch);
}

// Test 2: Version string format
static void test_version_string(void) {
    printf("\nTest 2: Version string format\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    TEST_ASSERT(version.version_string != NULL, "Version string is not NULL");
    TEST_ASSERT(strlen(version.version_string) > 0, "Version string is not empty");
    
    // Should contain dots
    TEST_ASSERT(strchr(version.version_string, '.') != NULL, "Version string contains dots");
    
    printf("    Version string: %s\n", version.version_string);
}

// Test 3: Build information
static void test_build_info(void) {
    printf("\nTest 3: Build information\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    TEST_ASSERT(version.build_date != NULL, "Build date is not NULL");
    TEST_ASSERT(version.build_type != NULL, "Build type is not NULL");
    
    printf("    Build date: %s\n", version.build_date);
    printf("    Build type: %s\n", version.build_type);
}

// Test 4: Compatible version check (same version)
static void test_compatible_same_version(void) {
    printf("\nTest 4: Compatible version check (same version)\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    // Check compatibility with same version
    lgx_result_t result = lgx_runtime_check_compatibility(
        version.major, version.minor, version.patch);
    
    TEST_ASSERT(result == LGX_SUCCESS, "Same version is compatible");
}

// Test 5: Compatible version check (older minor)
static void test_compatible_older_minor(void) {
    printf("\nTest 5: Compatible version check (older minor)\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    // Older minor version should be compatible (if minor > 0)
    if (version.minor > 0) {
        lgx_result_t result = lgx_runtime_check_compatibility(
            version.major, version.minor - 1, 0);
        
        TEST_ASSERT(result == LGX_SUCCESS, "Older minor version is compatible");
    } else {
        TEST_ASSERT(true, "Skipped (minor version is 0)");
    }
}

// Test 6: Incompatible version check (different major)
static void test_incompatible_major(void) {
    printf("\nTest 6: Incompatible version check (different major)\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    // Different major version should be incompatible
    lgx_result_t result = lgx_runtime_check_compatibility(
        version.major + 1, 0, 0);
    
    TEST_ASSERT(result != LGX_SUCCESS, "Different major version is incompatible");
}

// Test 7: Incompatible version check (newer minor)
static void test_incompatible_newer_minor(void) {
    printf("\nTest 7: Incompatible version check (newer minor)\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    // Newer minor version should be incompatible
    lgx_result_t result = lgx_runtime_check_compatibility(
        version.major, version.minor + 1, 0);
    
    TEST_ASSERT(result != LGX_SUCCESS, "Newer minor version is incompatible");
}

// Test 8: Patch version compatibility
static void test_patch_compatibility(void) {
    printf("\nTest 8: Patch version compatibility\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    // Different patch version should be compatible
    lgx_result_t result1 = lgx_runtime_check_compatibility(
        version.major, version.minor, version.patch + 1);
    
    lgx_result_t result2 = lgx_runtime_check_compatibility(
        version.major, version.minor, version.patch > 0 ? version.patch - 1 : 0);
    
    TEST_ASSERT(result1 == LGX_SUCCESS, "Newer patch version is compatible");
    TEST_ASSERT(result2 == LGX_SUCCESS, "Older patch version is compatible");
}

// Test 9: NULL parameter handling
static void test_null_parameters(void) {
    printf("\nTest 9: NULL parameter handling\n");
    
    lgx_result_t result = lgx_runtime_get_version(NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Get version with NULL fails");
}

// Test 10: Struct size validation
static void test_struct_size_validation(void) {
    printf("\nTest 10: Struct size validation\n");
    
    lgx_version_info_t version;
    
    // Wrong struct size
    version.struct_size = 0;
    lgx_result_t result = lgx_runtime_get_version(&version);
    TEST_ASSERT(result != LGX_SUCCESS, "Wrong struct size fails");
    
    // Correct struct size
    version.struct_size = sizeof(lgx_version_info_t);
    result = lgx_runtime_get_version(&version);
    TEST_ASSERT(result == LGX_SUCCESS, "Correct struct size succeeds");
}

// Test 11: Version macros
static void test_version_macros(void) {
    printf("\nTest 11: Version macros\n");
    
    TEST_ASSERT(LGX_VERSION_MAJOR >= 1, "LGX_VERSION_MAJOR is defined");
    TEST_ASSERT(LGX_VERSION_MINOR >= 0, "LGX_VERSION_MINOR is defined");
    TEST_ASSERT(LGX_VERSION_PATCH >= 0, "LGX_VERSION_PATCH is defined");
    
    printf("    Compile-time version: %d.%d.%d\n",
           LGX_VERSION_MAJOR, LGX_VERSION_MINOR, LGX_VERSION_PATCH);
    
    // Runtime version should match compile-time version
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    TEST_ASSERT(version.major == LGX_VERSION_MAJOR, "Runtime major matches compile-time");
    TEST_ASSERT(version.minor == LGX_VERSION_MINOR, "Runtime minor matches compile-time");
    TEST_ASSERT(version.patch == LGX_VERSION_PATCH, "Runtime patch matches compile-time");
}

// Test 12: ABI version
static void test_abi_version(void) {
    printf("\nTest 12: ABI version\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    TEST_ASSERT(version.abi_version > 0, "ABI version is set");
    
    printf("    ABI version: %u\n", version.abi_version);
}

// Test 13: Feature flags
static void test_feature_flags(void) {
    printf("\nTest 13: Feature flags\n");
    
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    lgx_runtime_get_version(&version);
    
    // Feature flags should be set
    printf("    Feature flags: 0x%08X\n", version.features);
    
    // Check for expected features
    bool has_frame_arena = (version.features & LGX_FEATURE_FRAME_ARENA) != 0;
    bool has_gpu_pool = (version.features & LGX_FEATURE_GPU_POOL) != 0;
    bool has_persistent_heap = (version.features & LGX_FEATURE_PERSISTENT_HEAP) != 0;
    
    TEST_ASSERT(has_frame_arena, "Frame arena feature flag set");
    TEST_ASSERT(has_persistent_heap, "Persistent heap feature flag set");
    
    printf("    Frame arena: %s\n", has_frame_arena ? "Yes" : "No");
    printf("    GPU pool: %s\n", has_gpu_pool ? "Yes" : "No");
    printf("    Persistent heap: %s\n", has_persistent_heap ? "Yes" : "No");
}

// Test 14: Compatibility with old struct size
static void test_old_struct_size_compatibility(void) {
    printf("\nTest 14: Compatibility with old struct size\n");
    
    // Simulate old version with smaller struct
    typedef struct {
        size_t struct_size;
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        const char* version_string;
    } lgx_version_info_old_t;
    
    lgx_version_info_old_t old_version;
    old_version.struct_size = sizeof(lgx_version_info_old_t);
    
    lgx_result_t result = lgx_runtime_get_version((lgx_version_info_t*)&old_version);
    
    // Should handle gracefully (either succeed with partial data or fail safely)
    TEST_ASSERT(result == LGX_SUCCESS || result == LGX_ERROR_VERSION_MISMATCH,
                "Old struct size handled gracefully");
}

int main(void) {
    printf("=== LGX Runtime Version and Compatibility Unit Tests ===\n");
    
    test_get_version();
    test_version_string();
    test_build_info();
    test_compatible_same_version();
    test_compatible_older_minor();
    test_incompatible_major();
    test_incompatible_newer_minor();
    test_patch_compatibility();
    test_null_parameters();
    test_struct_size_validation();
    test_version_macros();
    test_abi_version();
    test_feature_flags();
    test_old_struct_size_compatibility();
    
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
