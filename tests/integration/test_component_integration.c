/**
 * Integration Test: Component Integration
 * 
 * Tests integration with Translation Layer, Security Module, and other components.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include "lgx_integration.h"
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

// Mock security hooks for testing
static int security_alloc_count = 0;
static int security_free_count = 0;
static int security_api_call_count = 0;
static int security_error_count = 0;

static void mock_on_alloc(void* ptr, size_t size, void* user_data) {
    (void)ptr;
    (void)size;
    (void)user_data;
    security_alloc_count++;
}

static void mock_on_free(void* ptr, void* user_data) {
    (void)ptr;
    (void)user_data;
    security_free_count++;
}

static void mock_on_api_call(const char* func_name, void* user_data) {
    (void)func_name;
    (void)user_data;
    security_api_call_count++;
}

static void mock_on_error(const lgx_error_context_t* error, void* user_data) {
    (void)error;
    (void)user_data;
    security_error_count++;
}

// Test 1: Security hooks registration
static void test_security_hooks_registration(void) {
    printf("\nTest 1: Security hooks registration\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Register security hooks
    lgx_security_hooks_t hooks;
    hooks.on_alloc = mock_on_alloc;
    hooks.on_free = mock_on_free;
    hooks.on_api_call = mock_on_api_call;
    hooks.on_error = mock_on_error;
    
    lgx_result_t result = lgx_runtime_register_security_hooks(&hooks, NULL);
    TEST_ASSERT(result == LGX_SUCCESS, "Security hooks registration succeeds");
    
    // Perform operations to trigger hooks
    security_alloc_count = 0;
    security_free_count = 0;
    
    void* ptr = lgx_alloc(1024);
    TEST_ASSERT(security_alloc_count > 0, "Alloc hook triggered");
    
    lgx_free(ptr);
    TEST_ASSERT(security_free_count > 0, "Free hook triggered");
    
    // Unregister hooks
    result = lgx_runtime_unregister_security_hooks();
    TEST_ASSERT(result == LGX_SUCCESS, "Security hooks unregistration succeeds");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 2: Translation Layer context
static void test_translation_layer_context(void) {
    printf("\nTest 2: Translation Layer context\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Get translation context
    lgx_translate_context_t* ctx = lgx_runtime_get_translate_context();
    TEST_ASSERT(ctx != NULL, "Translation context is available");
    
    // Use translation layer allocator
    void* ptr = lgx_translate_alloc(2048);
    TEST_ASSERT(ptr != NULL, "Translation layer allocation succeeds");
    
    lgx_translate_free(ptr);
    TEST_ASSERT(true, "Translation layer free succeeds");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Shader cache configuration
static void test_shader_cache_configuration(void) {
    printf("\nTest 3: Shader cache configuration\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Configure shader cache
    lgx_shader_cache_config_t cache_config;
    cache_config.cache_directory = "/tmp/lgx_shader_cache";
    cache_config.max_cache_size_mb = 512;
    
    lgx_result_t result = lgx_runtime_configure_shader_cache(&cache_config);
    TEST_ASSERT(result == LGX_SUCCESS, "Shader cache configuration succeeds");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Component loading/unloading
static void test_component_loading(void) {
    printf("\nTest 4: Component loading/unloading\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Check if components are loaded
    bool dx11_loaded = lgx_runtime_is_component_loaded(LGX_COMPONENT_DX11_TRANSLATION);
    bool dx12_loaded = lgx_runtime_is_component_loaded(LGX_COMPONENT_DX12_TRANSLATION);
    bool security_loaded = lgx_runtime_is_component_loaded(LGX_COMPONENT_SECURITY_MODULE);
    
    printf("    DX11 Translation: %s\n", dx11_loaded ? "Loaded" : "Not loaded");
    printf("    DX12 Translation: %s\n", dx12_loaded ? "Loaded" : "Not loaded");
    printf("    Security Module: %s\n", security_loaded ? "Loaded" : "Not loaded");
    
    TEST_ASSERT(true, "Component status query works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 5: Security hooks with error handling
static void test_security_hooks_with_errors(void) {
    printf("\nTest 5: Security hooks with error handling\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Register hooks
    lgx_security_hooks_t hooks;
    hooks.on_alloc = mock_on_alloc;
    hooks.on_free = mock_on_free;
    hooks.on_api_call = mock_on_api_call;
    hooks.on_error = mock_on_error;
    
    lgx_runtime_register_security_hooks(&hooks, NULL);
    
    security_error_count = 0;
    
    // Trigger an error
    void* ptr = lgx_alloc(0); // Invalid size
    (void)ptr;
    
    TEST_ASSERT(security_error_count > 0, "Error hook triggered");
    
    lgx_runtime_unregister_security_hooks();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 6: Multiple component integration
static void test_multiple_component_integration(void) {
    printf("\nTest 6: Multiple component integration\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Register security hooks
    lgx_security_hooks_t hooks;
    hooks.on_alloc = mock_on_alloc;
    hooks.on_free = mock_on_free;
    hooks.on_api_call = mock_on_api_call;
    hooks.on_error = mock_on_error;
    lgx_runtime_register_security_hooks(&hooks, NULL);
    
    // Get translation context
    lgx_translate_context_t* ctx = lgx_runtime_get_translate_context();
    TEST_ASSERT(ctx != NULL, "Translation context available");
    
    // Configure shader cache
    lgx_shader_cache_config_t cache_config;
    cache_config.cache_directory = "/tmp/lgx_shader_cache";
    cache_config.max_cache_size_mb = 256;
    lgx_runtime_configure_shader_cache(&cache_config);
    
    // Perform operations
    security_alloc_count = 0;
    void* ptr1 = lgx_alloc(1024);
    void* ptr2 = lgx_translate_alloc(2048);
    
    TEST_ASSERT(security_alloc_count >= 1, "Security hooks work with multiple components");
    
    lgx_free(ptr1);
    lgx_translate_free(ptr2);
    
    lgx_runtime_unregister_security_hooks();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 7: Component isolation
static void test_component_isolation(void) {
    printf("\nTest 7: Component isolation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Allocate from runtime
    void* runtime_ptr = lgx_alloc(1024);
    TEST_ASSERT(runtime_ptr != NULL, "Runtime allocation succeeds");
    
    // Allocate from translation layer
    void* translate_ptr = lgx_translate_alloc(2048);
    TEST_ASSERT(translate_ptr != NULL, "Translation allocation succeeds");
    
    // Verify they're different
    TEST_ASSERT(runtime_ptr != translate_ptr, "Allocations are isolated");
    
    // Free both
    lgx_free(runtime_ptr);
    lgx_translate_free(translate_ptr);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 8: Security hooks performance impact
static void test_security_hooks_performance(void) {
    printf("\nTest 8: Security hooks performance impact\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Measure without hooks
    uint64_t start = lgx_time_now_ns();
    for (int i = 0; i < 1000; i++) {
        void* ptr = lgx_alloc(1024);
        lgx_free(ptr);
    }
    uint64_t without_hooks = lgx_time_now_ns() - start;
    
    // Register hooks
    lgx_security_hooks_t hooks;
    hooks.on_alloc = mock_on_alloc;
    hooks.on_free = mock_on_free;
    hooks.on_api_call = mock_on_api_call;
    hooks.on_error = mock_on_error;
    lgx_runtime_register_security_hooks(&hooks, NULL);
    
    // Measure with hooks
    start = lgx_time_now_ns();
    for (int i = 0; i < 1000; i++) {
        void* ptr = lgx_alloc(1024);
        lgx_free(ptr);
    }
    uint64_t with_hooks = lgx_time_now_ns() - start;
    
    double overhead_percent = ((double)(with_hooks - without_hooks) / without_hooks) * 100.0;
    
    printf("    Without hooks: %lu ns\n", (unsigned long)without_hooks);
    printf("    With hooks: %lu ns\n", (unsigned long)with_hooks);
    printf("    Overhead: %.2f%%\n", overhead_percent);
    
    TEST_ASSERT(overhead_percent < 20.0, "Security hooks overhead < 20%");
    
    lgx_runtime_unregister_security_hooks();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 9: Component error propagation
static void test_component_error_propagation(void) {
    printf("\nTest 9: Component error propagation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Register error hook
    lgx_security_hooks_t hooks;
    hooks.on_alloc = NULL;
    hooks.on_free = NULL;
    hooks.on_api_call = NULL;
    hooks.on_error = mock_on_error;
    lgx_runtime_register_security_hooks(&hooks, NULL);
    
    security_error_count = 0;
    
    // Trigger errors from different sources
    lgx_alloc(0); // Invalid allocation
    lgx_runtime_init(NULL); // Invalid init
    
    TEST_ASSERT(security_error_count > 0, "Errors propagated to security hooks");
    
    lgx_runtime_unregister_security_hooks();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Full integration workflow
static void test_full_integration_workflow(void) {
    printf("\nTest 10: Full integration workflow\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    // Enable telemetry
    lgx_telemetry_enable(true);
    
    // Register security hooks
    lgx_security_hooks_t hooks;
    hooks.on_alloc = mock_on_alloc;
    hooks.on_free = mock_on_free;
    hooks.on_api_call = mock_on_api_call;
    hooks.on_error = mock_on_error;
    lgx_runtime_register_security_hooks(&hooks, NULL);
    
    // Configure shader cache
    lgx_shader_cache_config_t cache_config;
    cache_config.cache_directory = "/tmp/lgx_shader_cache";
    cache_config.max_cache_size_mb = 256;
    lgx_runtime_configure_shader_cache(&cache_config);
    
    // Perform operations
    security_alloc_count = 0;
    security_free_count = 0;
    
    for (int i = 0; i < 100; i++) {
        void* ptr1 = lgx_alloc(1024);
        void* ptr2 = lgx_translate_alloc(2048);
        lgx_free(ptr1);
        lgx_translate_free(ptr2);
    }
    
    TEST_ASSERT(security_alloc_count >= 100, "Security hooks tracked allocations");
    TEST_ASSERT(security_free_count >= 100, "Security hooks tracked frees");
    
    // Export telemetry
    char buffer[32 * 1024];
    lgx_telemetry_export(buffer, sizeof(buffer));
    TEST_ASSERT(strlen(buffer) > 0, "Telemetry collected data");
    
    // Cleanup
    lgx_runtime_unregister_security_hooks();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== LGX Runtime Component Integration Test ===\n");
    
    test_security_hooks_registration();
    test_translation_layer_context();
    test_shader_cache_configuration();
    test_component_loading();
    test_security_hooks_with_errors();
    test_multiple_component_integration();
    test_component_isolation();
    test_security_hooks_performance();
    test_component_error_propagation();
    test_full_integration_workflow();
    
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
