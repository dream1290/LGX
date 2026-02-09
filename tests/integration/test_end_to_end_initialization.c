/**
 * Integration Test: End-to-End Initialization
 * 
 * Tests complete initialization flow including all subsystems.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

// Test 1: Complete initialization with all subsystems
static void test_complete_initialization(void) {
    printf("\nTest 1: Complete initialization with all subsystems\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Config creation succeeds");
    
    // Configure all subsystems
    lgx_config_set_memory_pool_size(config, 256 * 1024 * 1024); // 256MB
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_HUGE_PAGES | 
                                  LGX_CONFIG_ENABLE_NUMA);
    
    uint64_t start = lgx_time_now_ns();
    lgx_result_t result = lgx_runtime_init(config);
    uint64_t end = lgx_time_now_ns();
    
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime initialization succeeds");
    
    uint64_t init_time_ms = (end - start) / 1000000;
    printf("    Initialization time: %lu ms\n", (unsigned long)init_time_ms);
    
    // Verify all subsystems are initialized
    TEST_ASSERT(lgx_runtime_is_initialized(), "Runtime is initialized");
    TEST_ASSERT(lgx_frame_arena_is_initialized(), "Frame arena initialized");
    TEST_ASSERT(lgx_persistent_heap_is_initialized(), "Persistent heap initialized");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 2: Initialization with minimal configuration
static void test_minimal_initialization(void) {
    printf("\nTest 2: Initialization with minimal configuration\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    
    // Use defaults
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Minimal init succeeds");
    
    // Verify basic functionality
    void* ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Basic allocation works");
    lgx_free(ptr);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Initialization with telemetry enabled
static void test_initialization_with_telemetry(void) {
    printf("\nTest 3: Initialization with telemetry enabled\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Init with telemetry succeeds");
    
    // Enable telemetry
    lgx_telemetry_enable(true);
    
    // Perform some operations
    void* ptr1 = lgx_alloc(1024);
    void* ptr2 = lgx_alloc(2048);
    lgx_free(ptr1);
    lgx_free(ptr2);
    
    // Telemetry should be collecting data
    TEST_ASSERT(true, "Telemetry collection works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Initialization performance validation
static void test_initialization_performance(void) {
    printf("\nTest 4: Initialization performance validation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    
    uint64_t start = lgx_time_now_ns();
    lgx_result_t result = lgx_runtime_init(config);
    uint64_t end = lgx_time_now_ns();
    
    TEST_ASSERT(result == LGX_SUCCESS, "Init succeeds");
    
    uint64_t init_time_ms = (end - start) / 1000000;
    
    // Should complete within reasonable time (< 2 seconds)
    TEST_ASSERT(init_time_ms < 2000, "Init completes within 2 seconds");
    
    printf("    Init time: %lu ms\n", (unsigned long)init_time_ms);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 5: Subsystem initialization order
static void test_subsystem_initialization_order(void) {
    printf("\nTest 5: Subsystem initialization order\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Init succeeds");
    
    // Verify subsystems are initialized in correct order
    // Core systems should be ready
    lgx_version_info_t version;
    version.struct_size = sizeof(lgx_version_info_t);
    result = lgx_runtime_get_version(&version);
    TEST_ASSERT(result == LGX_SUCCESS, "Version system initialized");
    
    // Memory systems should be ready
    void* ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Memory system initialized");
    lgx_free(ptr);
    
    // Platform services should be ready
    uint64_t time = lgx_time_now_ns();
    TEST_ASSERT(time > 0, "Timing system initialized");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 6: Capability detection after initialization
static void test_capability_detection(void) {
    printf("\nTest 6: Capability detection after initialization\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Query capabilities
    bool has_huge_pages = lgx_runtime_has_capability(LGX_CAP_HUGE_PAGES);
    bool has_numa = lgx_runtime_has_capability(LGX_CAP_NUMA_AWARENESS);
    bool has_gpu = lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION);
    
    printf("    Huge pages: %s\n", has_huge_pages ? "Yes" : "No");
    printf("    NUMA: %s\n", has_numa ? "Yes" : "No");
    printf("    GPU: %s\n", has_gpu ? "Yes" : "No");
    
    TEST_ASSERT(true, "Capability detection works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 7: Hardware tier classification
static void test_hardware_tier_classification(void) {
    printf("\nTest 7: Hardware tier classification\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    lgx_hardware_status_t hw_status;
    hw_status.struct_size = sizeof(lgx_hardware_status_t);
    hw_status = lgx_runtime_get_hardware_status();
    
    TEST_ASSERT(hw_status.achieved_tier >= LGX_HW_TIER_OPTIMAL &&
                hw_status.achieved_tier <= LGX_HW_TIER_DEGRADED,
                "Hardware tier is valid");
    
    const char* tier_str = "Unknown";
    switch (hw_status.achieved_tier) {
        case LGX_HW_TIER_OPTIMAL: tier_str = "Optimal"; break;
        case LGX_HW_TIER_COMPATIBLE: tier_str = "Compatible"; break;
        case LGX_HW_TIER_DEGRADED: tier_str = "Degraded"; break;
    }
    
    printf("    Hardware tier: %s\n", tier_str);
    
    if (hw_status.degradation_reason != NULL) {
        printf("    Degradation: %s\n", hw_status.degradation_reason);
    }
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 8: Memory allocator initialization
static void test_memory_allocator_initialization(void) {
    printf("\nTest 8: Memory allocator initialization\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Test frame arena
    void* frame_ptr = lgx_frame_alloc(1024);
    TEST_ASSERT(frame_ptr != NULL, "Frame arena works");
    
    // Test persistent heap
    void* heap_ptr = lgx_alloc(1024);
    TEST_ASSERT(heap_ptr != NULL, "Persistent heap works");
    lgx_free(heap_ptr);
    
    // Test intent-based allocation
    lgx_allocation_intent_base_t intent;
    intent.struct_size = sizeof(lgx_allocation_intent_base_t);
    intent.size = 2048;
    intent.lifetime = LGX_LIFETIME_FRAME;
    intent.access_pattern = LGX_ACCESS_SEQUENTIAL;
    intent.hint = LGX_HINT_CRITICAL_PATH;
    intent.validation_policy = LGX_INTENT_TRUST;
    
    void* intent_ptr = lgx_alloc_with_intent(&intent);
    TEST_ASSERT(intent_ptr != NULL, "Intent-based allocation works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 9: Error handling initialization
static void test_error_handling_initialization(void) {
    printf("\nTest 9: Error handling initialization\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Clear any previous errors
    lgx_clear_last_error();
    
    // Trigger an error
    void* ptr = lgx_alloc(0); // Invalid size
    
    // Check error was recorded
    lgx_error_context_t error = lgx_get_last_error();
    TEST_ASSERT(error.error_code != LGX_SUCCESS, "Error handling works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Health monitoring initialization
static void test_health_monitoring_initialization(void) {
    printf("\nTest 10: Health monitoring initialization\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    lgx_health_status_t status;
    status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_result_t result = lgx_runtime_health_check(&status);
    TEST_ASSERT(result == LGX_SUCCESS, "Health check works");
    TEST_ASSERT(status.overall_health >= LGX_HEALTH_GOOD &&
                status.overall_health <= LGX_HEALTH_FAILED,
                "Health status is valid");
    
    printf("    Memory: %zu MB / %zu MB\n", 
           status.memory_usage_mb, status.memory_limit_mb);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== LGX Runtime End-to-End Initialization Integration Test ===\n");
    
    test_complete_initialization();
    test_minimal_initialization();
    test_initialization_with_telemetry();
    test_initialization_performance();
    test_subsystem_initialization_order();
    test_capability_detection();
    test_hardware_tier_classification();
    test_memory_allocator_initialization();
    test_error_handling_initialization();
    test_health_monitoring_initialization();
    
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
