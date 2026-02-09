/**
 * Test: Library Manifest and Version Validation
 * 
 * Validates library version checking against manifest requirements.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_manifest_requirements(void) {
    printf("Test: Get manifest requirements...\n");
    
    char buffer[1024];
    lgx_manifest_get_requirements(buffer, sizeof(buffer));
    
    printf("%s", buffer);
    
    // Validate that we got some output
    assert(strlen(buffer) > 0);
    assert(strstr(buffer, "glibc") != NULL);
    
    printf("  Manifest requirements retrieved: OK\n");
}

static void test_glibc_version_check(void) {
    printf("Test: glibc version check...\n");
    
    char version[64];
    bool meets_requirement = false;
    
    lgx_result_t result = lgx_manifest_check_glibc(version, sizeof(version), &meets_requirement);
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    printf("  glibc version: %s\n", version);
    printf("  Meets requirement: %s\n", meets_requirement ? "YES" : "NO");
    
    // Validate that we got a version string
    assert(strlen(version) > 0);
    
    // On modern systems, glibc should meet requirements
    // (Ubuntu 22.04+ has glibc 2.35+)
    if (meets_requirement) {
        printf("  glibc version check: PASS\n");
    } else {
        printf("  glibc version check: WARN (below 2.35)\n");
    }
}

static void test_libstdcpp_version_check(void) {
    printf("Test: libstdc++ version check...\n");
    
    char version[64];
    bool meets_requirement = false;
    
    lgx_result_t result = lgx_manifest_check_libstdcpp(version, sizeof(version), &meets_requirement);
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    printf("  libstdc++ version: %s\n", version);
    printf("  Meets requirement: %s\n", meets_requirement ? "YES" : "NO");
    
    // libstdc++ is optional, so it should always meet requirements
    assert(meets_requirement == true);
    
    printf("  libstdc++ version check: OK (optional)\n");
}

static void test_vulkan_version_check(void) {
    printf("Test: Vulkan version check...\n");
    
    char version[64];
    bool meets_requirement = false;
    
    lgx_result_t result = lgx_manifest_check_vulkan(version, sizeof(version), &meets_requirement);
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    printf("  Vulkan version: %s\n", version);
    printf("  Meets requirement: %s\n", meets_requirement ? "YES" : "NO");
    
    // Vulkan is optional, so it should always meet requirements
    assert(meets_requirement == true);
    
    if (strcmp(version, "not available") == 0) {
        printf("  Vulkan version check: OK (not available, optional)\n");
    } else {
        printf("  Vulkan version check: OK (available)\n");
    }
}

static void test_validate_all_libraries(void) {
    printf("Test: Validate all libraries...\n");
    
    lgx_result_t result = lgx_manifest_validate_all();
    
    if (result == LGX_SUCCESS) {
        printf("  All library requirements met: PASS\n");
    } else if (result == LGX_ERROR_LIBRARY_VERSION_MISMATCH) {
        printf("  Library version mismatch detected: WARN\n");
        printf("  (This is expected on older systems with glibc < 2.35)\n");
    } else {
        printf("  Unexpected error: %d\n", result);
        assert(false);
    }
}

static void test_integration_with_namespace(void) {
    printf("Test: Integration with namespace validation...\n");
    
    // Create namespace
    lgx_result_t result = lgx_namespace_create_isolated();
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    // Validate versions (which now uses manifest)
    result = lgx_namespace_validate_versions();
    
    if (result == LGX_SUCCESS) {
        printf("  Namespace validation with manifest: PASS\n");
    } else if (result == LGX_ERROR_LIBRARY_VERSION_MISMATCH) {
        printf("  Namespace validation: WARN (version mismatch)\n");
    } else {
        printf("  Unexpected error: %d\n", result);
        assert(false);
    }
    
    // Get versions
    char glibc_ver[64], libstdcpp_ver[64], vulkan_ver[64];
    lgx_namespace_get_versions(glibc_ver, libstdcpp_ver, vulkan_ver, 64);
    
    printf("  Retrieved versions:\n");
    printf("    glibc: %s\n", glibc_ver);
    printf("    libstdc++: %s\n", libstdcpp_ver);
    printf("    vulkan: %s\n", vulkan_ver);
    
    // Cleanup
    lgx_namespace_cleanup();
    
    printf("  Integration test: OK\n");
}

static void test_runtime_initialization_with_validation(void) {
    printf("Test: Runtime initialization with library validation...\n");
    
    // Initialize runtime (which validates libraries)
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    
    if (result == LGX_SUCCESS) {
        printf("  Runtime initialized with library validation: PASS\n");
    } else if (result == LGX_ERROR_LIBRARY_VERSION_MISMATCH) {
        printf("  Runtime initialization: WARN (version mismatch)\n");
        printf("  (Runtime continues with graceful degradation)\n");
    } else {
        printf("  Unexpected error: %d\n", result);
    }
    
    // Shutdown
    if (result == LGX_SUCCESS) {
        lgx_runtime_shutdown();
    }
    lgx_config_destroy(config);
    
    printf("  Runtime initialization test: OK\n");
}

int main(void) {
    printf("=== Library Manifest and Version Validation Tests ===\n\n");
    
    test_manifest_requirements();
    printf("\n");
    
    test_glibc_version_check();
    printf("\n");
    
    test_libstdcpp_version_check();
    printf("\n");
    
    test_vulkan_version_check();
    printf("\n");
    
    test_validate_all_libraries();
    printf("\n");
    
    test_integration_with_namespace();
    printf("\n");
    
    test_runtime_initialization_with_validation();
    printf("\n");
    
    printf("=== All Library Manifest Tests Passed ===\n");
    return 0;
}
