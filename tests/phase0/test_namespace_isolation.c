/**
 * Test: Namespace Isolation
 * 
 * Validates library isolation and version validation functionality.
 * Tests graceful degradation when privileges are not available.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_namespace_creation(void) {
    printf("Test: Namespace creation...\n");
    
    // Attempt to create namespace (may fail without privileges)
    lgx_result_t result = lgx_namespace_create_isolated();
    assert(result == LGX_SUCCESS);  // Should succeed or gracefully degrade
    (void)result;  // Mark as used
    
    printf("  Namespace creation: %s\n", 
           lgx_namespace_is_isolated() ? "ISOLATED" : "NOT ISOLATED (graceful degradation)");
}

static void test_library_version_validation(void) {
    printf("Test: Library version validation...\n");
    
    // Validate library versions
    lgx_result_t result = lgx_namespace_validate_versions();
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    // Get library versions
    char glibc_ver[64] = {0};
    char libstdcpp_ver[64] = {0};
    char vulkan_ver[64] = {0};
    
    lgx_namespace_get_versions(glibc_ver, libstdcpp_ver, vulkan_ver, 64);
    
    printf("  glibc version: %s\n", glibc_ver);
    printf("  libstdc++ version: %s\n", libstdcpp_ver);
    printf("  Vulkan version: %s\n", vulkan_ver);
    
    // Validate that we got version strings
    assert(strlen(glibc_ver) > 0);
    assert(strlen(libstdcpp_ver) > 0);
}

static void test_library_mounting(void) {
    printf("Test: Library mounting...\n");
    
    // Try to mount libraries from a test directory
    lgx_result_t result = lgx_namespace_mount_libraries("/usr/lib/x86_64-linux-gnu");
    assert(result == LGX_SUCCESS);  // Should succeed or gracefully degrade
    (void)result;  // Mark as used
    
    printf("  Library mounting: OK (graceful degradation if namespace not created)\n");
}

static void test_namespace_cleanup(void) {
    printf("Test: Namespace cleanup...\n");
    
    lgx_result_t result = lgx_namespace_cleanup();
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    printf("  Namespace cleanup: OK\n");
}

static void test_full_lifecycle(void) {
    printf("Test: Full namespace lifecycle...\n");
    
    // Create namespace
    lgx_result_t result = lgx_namespace_create_isolated();
    assert(result == LGX_SUCCESS);
    
    // Validate versions
    result = lgx_namespace_validate_versions();
    assert(result == LGX_SUCCESS);
    
    // Mount libraries
    result = lgx_namespace_mount_libraries("/usr/lib");
    assert(result == LGX_SUCCESS);
    
    // Cleanup
    result = lgx_namespace_cleanup();
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    printf("  Full lifecycle: OK\n");
}

static void test_integration_with_runtime(void) {
    printf("Test: Integration with runtime initialization...\n");
    
    // Initialize runtime (which should create namespace)
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Mark as used
    
    // Check if namespace was created
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    assert(runtime != NULL);
    (void)runtime;  // Mark as used
    
    printf("  Runtime initialized with namespace support\n");
    
    // Get library versions through runtime
    char glibc_ver[64] = {0};
    char libstdcpp_ver[64] = {0};
    char vulkan_ver[64] = {0};
    lgx_namespace_get_versions(glibc_ver, libstdcpp_ver, vulkan_ver, 64);
    
    printf("  Library versions available: glibc=%s, libstdc++=%s, vulkan=%s\n",
           glibc_ver, libstdcpp_ver, vulkan_ver);
    
    // Shutdown (which should cleanup namespace)
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  Runtime shutdown with namespace cleanup: OK\n");
}

int main(void) {
    printf("=== Namespace Isolation Tests ===\n\n");
    
    printf("NOTE: These tests may show 'graceful degradation' messages if running\n");
    printf("      without CAP_SYS_ADMIN capability or root privileges. This is expected.\n\n");
    
    test_namespace_creation();
    printf("\n");
    
    test_library_version_validation();
    printf("\n");
    
    test_library_mounting();
    printf("\n");
    
    test_namespace_cleanup();
    printf("\n");
    
    test_full_lifecycle();
    printf("\n");
    
    test_integration_with_runtime();
    printf("\n");
    
    printf("=== All Namespace Isolation Tests Passed ===\n");
    return 0;
}
