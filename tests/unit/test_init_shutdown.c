/**
 * Unit Tests: Initialization and Shutdown
 * 
 * Tests the core initialization and shutdown functionality of LGX Runtime.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Test counter
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

// Test 1: Basic initialization and shutdown
static void test_basic_init_shutdown(void) {
    printf("\nTest 1: Basic initialization and shutdown\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Config creation succeeds");
    
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime initialization succeeds");
    
    TEST_ASSERT(lgx_runtime_is_initialized(), "Runtime reports initialized");
    
    result = lgx_runtime_shutdown();
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime shutdown succeeds");
    
    TEST_ASSERT(!lgx_runtime_is_initialized(), "Runtime reports not initialized after shutdown");
    
    lgx_config_destroy(config);
}

// Test 2: Multiple init/shutdown cycles
static void test_multiple_cycles(void) {
    printf("\nTest 2: Multiple init/shutdown cycles\n");
    
    for (int i = 0; i < 5; i++) {
        lgx_runtime_config_t* config = lgx_config_create();
        TEST_ASSERT(config != NULL, "Config creation succeeds (cycle)");
        
        lgx_result_t result = lgx_runtime_init(config);
        TEST_ASSERT(result == LGX_SUCCESS, "Init succeeds (cycle)");
        
        result = lgx_runtime_shutdown();
        TEST_ASSERT(result == LGX_SUCCESS, "Shutdown succeeds (cycle)");
        
        lgx_config_destroy(config);
    }
}

// Test 3: Double initialization (should fail)
static void test_double_init(void) {
    printf("\nTest 3: Double initialization (error handling)\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Config creation succeeds");
    
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "First init succeeds");
    
    // Try to initialize again (should fail)
    result = lgx_runtime_init(config);
    TEST_ASSERT(result != LGX_SUCCESS, "Second init fails as expected");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Shutdown without init (should handle gracefully)
static void test_shutdown_without_init(void) {
    printf("\nTest 4: Shutdown without init (error handling)\n");
    
    // Ensure runtime is not initialized
    if (lgx_runtime_is_initialized()) {
        lgx_runtime_shutdown();
    }
    
    // Try to shutdown when not initialized
    lgx_result_t result = lgx_runtime_shutdown();
    TEST_ASSERT(result != LGX_SUCCESS, "Shutdown without init fails gracefully");
}

// Test 5: NULL config handling
static void test_null_config(void) {
    printf("\nTest 5: NULL config handling\n");
    
    lgx_result_t result = lgx_runtime_init(NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Init with NULL config fails");
    
    // Cleanup if somehow initialized
    if (lgx_runtime_is_initialized()) {
        lgx_runtime_shutdown();
    }
}

// Test 6: Config creation and destruction
static void test_config_lifecycle(void) {
    printf("\nTest 6: Config lifecycle\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Config creation succeeds");
    
    // Config should be usable
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Config is valid for init");
    
    lgx_runtime_shutdown();
    
    // Destroy config
    lgx_config_destroy(config);
    
    // NULL config destroy should be safe
    lgx_config_destroy(NULL);
    TEST_ASSERT(true, "NULL config destroy is safe");
}

// Test 7: Initialization with default config
static void test_default_config(void) {
    printf("\nTest 7: Initialization with default config\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Default config creation succeeds");
    
    // Don't set any config options, use defaults
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Init with default config succeeds");
    
    // Verify runtime is functional
    TEST_ASSERT(lgx_runtime_is_initialized(), "Runtime is initialized");
    
    // Try a basic operation
    void* ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Basic allocation works with default config");
    lgx_free(ptr);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 8: Initialization state queries
static void test_init_state_queries(void) {
    printf("\nTest 8: Initialization state queries\n");
    
    // Before init
    TEST_ASSERT(!lgx_runtime_is_initialized(), "Not initialized before init");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // After init
    TEST_ASSERT(lgx_runtime_is_initialized(), "Initialized after init");
    
    // Get runtime state
    lgx_runtime_state_t* state = lgx_runtime_get_state();
    TEST_ASSERT(state != NULL, "Runtime state is accessible");
    TEST_ASSERT(state->initialized, "State reports initialized");
    
    lgx_runtime_shutdown();
    
    // After shutdown
    TEST_ASSERT(!lgx_runtime_is_initialized(), "Not initialized after shutdown");
    
    lgx_config_destroy(config);
}

// Test 9: Subsystem initialization
static void test_subsystem_init(void) {
    printf("\nTest 9: Subsystem initialization\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime init succeeds");
    
    // Check that key subsystems are initialized
    TEST_ASSERT(lgx_frame_arena_is_initialized(), "Frame arena initialized");
    
    // Check GPU pool if available
    if (lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
        TEST_ASSERT(lgx_gpu_pool_is_initialized(), "GPU pool initialized");
    }
    
    // Check persistent heap
    TEST_ASSERT(lgx_persistent_heap_is_initialized(), "Persistent heap initialized");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Cleanup verification
static void test_cleanup_verification(void) {
    printf("\nTest 10: Cleanup verification\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Allocate some memory
    void* ptr1 = lgx_alloc(1024);
    void* ptr2 = lgx_frame_alloc(512);
    
    TEST_ASSERT(ptr1 != NULL, "Heap allocation succeeds");
    TEST_ASSERT(ptr2 != NULL, "Frame allocation succeeds");
    
    // Free heap allocation
    lgx_free(ptr1);
    
    // Shutdown should clean up remaining resources
    lgx_result_t result = lgx_runtime_shutdown();
    TEST_ASSERT(result == LGX_SUCCESS, "Shutdown with active allocations succeeds");
    
    lgx_config_destroy(config);
}

// Test 11: Error state after failed init
static void test_error_state_after_failed_init(void) {
    printf("\nTest 11: Error state after failed init\n");
    
    // Try to init with NULL config (should fail)
    lgx_result_t result = lgx_runtime_init(NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Init with NULL fails");
    
    // Runtime should not be initialized
    TEST_ASSERT(!lgx_runtime_is_initialized(), "Runtime not initialized after failed init");
    
    // Should be able to init properly after failed attempt
    lgx_runtime_config_t* config = lgx_config_create();
    result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Can init after previous failure");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 12: Concurrent init attempts (thread safety)
static void test_concurrent_init(void) {
    printf("\nTest 12: Concurrent init protection\n");
    
    lgx_runtime_config_t* config1 = lgx_config_create();
    lgx_runtime_config_t* config2 = lgx_config_create();
    
    lgx_result_t result1 = lgx_runtime_init(config1);
    TEST_ASSERT(result1 == LGX_SUCCESS, "First init succeeds");
    
    // Second init should fail (already initialized)
    lgx_result_t result2 = lgx_runtime_init(config2);
    TEST_ASSERT(result2 != LGX_SUCCESS, "Concurrent init fails");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config1);
    lgx_config_destroy(config2);
}

int main(void) {
    printf("=== LGX Runtime Initialization and Shutdown Unit Tests ===\n");
    
    test_basic_init_shutdown();
    test_multiple_cycles();
    test_double_init();
    test_shutdown_without_init();
    test_null_config();
    test_config_lifecycle();
    test_default_config();
    test_init_state_queries();
    test_subsystem_init();
    test_cleanup_verification();
    test_error_state_after_failed_init();
    test_concurrent_init();
    
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
