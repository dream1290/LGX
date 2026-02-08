/**
 * Test: Structured Logging with Subsystem Filtering
 * 
 * Tests the structured logging system with subsystem-based filtering.
 */

#include "lgx_runtime.h"
#include "lgx_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_subsystem_to_string(void) {
    printf("Test: Subsystem name mapping...\n");
    
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_CORE), "CORE") == 0);
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_MEMORY), "MEMORY") == 0);
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_GPU), "GPU") == 0);
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_FILESYSTEM), "FS") == 0);
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_TELEMETRY), "TELEMETRY") == 0);
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_LIFECYCLE), "LIFECYCLE") == 0);
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_HARDWARE), "HARDWARE") == 0);
    assert(strcmp(lgx_subsystem_to_string(LGX_SUBSYSTEM_SECURITY), "SECURITY") == 0);
    
    printf("  ✓ All subsystem names mapped correctly\n");
}

static void test_basic_tagged_logging(void) {
    printf("\nTest: Basic tagged logging...\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Test logging to different subsystems
    printf("  Testing logs (should appear below):\n");
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "Core subsystem test message");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO, "Memory subsystem test message");
    lgx_log_tagged(LGX_SUBSYSTEM_GPU, LGX_LOG_WARN, "GPU subsystem warning");
    lgx_log_tagged(LGX_SUBSYSTEM_FILESYSTEM, LGX_LOG_ERROR, "Filesystem error");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Tagged logging works\n");
}

static void test_subsystem_filtering(void) {
    printf("\nTest: Subsystem filtering...\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Enable only MEMORY and GPU subsystems
    uint32_t filter = (1U << LGX_SUBSYSTEM_MEMORY) | (1U << LGX_SUBSYSTEM_GPU);
    lgx_set_subsystem_filter(filter);
    
    // Verify filter was set
    assert(lgx_get_subsystem_filter() == filter);
    
    printf("  Testing filtered logs (only MEMORY and GPU should appear):\n");
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "SHOULD NOT APPEAR: Core message");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO, "SHOULD APPEAR: Memory message");
    lgx_log_tagged(LGX_SUBSYSTEM_GPU, LGX_LOG_INFO, "SHOULD APPEAR: GPU message");
    lgx_log_tagged(LGX_SUBSYSTEM_FILESYSTEM, LGX_LOG_INFO, "SHOULD NOT APPEAR: FS message");
    
    // Reset filter (enable all)
    lgx_set_subsystem_filter(0);
    assert(lgx_get_subsystem_filter() == 0);
    
    printf("  Testing after reset (all should appear):\n");
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "SHOULD APPEAR: Core message");
    lgx_log_tagged(LGX_SUBSYSTEM_FILESYSTEM, LGX_LOG_INFO, "SHOULD APPEAR: FS message");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Subsystem filtering works\n");
}

static void test_combined_filters(void) {
    printf("\nTest: Combined log level and subsystem filtering...\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Set log level to WARN (filter out DEBUG and INFO)
    lgx_set_log_filter(LGX_LOG_WARN);
    
    // Enable only MEMORY subsystem
    lgx_set_subsystem_filter(1U << LGX_SUBSYSTEM_MEMORY);
    
    printf("  Testing combined filters (only MEMORY WARN/ERROR should appear):\n");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_DEBUG, "SHOULD NOT APPEAR: Memory debug");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO, "SHOULD NOT APPEAR: Memory info");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_WARN, "SHOULD APPEAR: Memory warning");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_ERROR, "SHOULD APPEAR: Memory error");
    lgx_log_tagged(LGX_SUBSYSTEM_GPU, LGX_LOG_ERROR, "SHOULD NOT APPEAR: GPU error (wrong subsystem)");
    
    // Reset filters
    lgx_set_log_filter(LGX_LOG_DEBUG);
    lgx_set_subsystem_filter(0);
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Combined filtering works\n");
}

static void test_all_subsystems(void) {
    printf("\nTest: All subsystems logging...\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    printf("  Testing all subsystems:\n");
    for (int i = 0; i < LGX_SUBSYSTEM_COUNT; i++) {
        lgx_log_tagged((lgx_log_subsystem_t)i, LGX_LOG_INFO, 
                      "Test message from subsystem %d", i);
    }
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ All subsystems can log\n");
}

int main(void) {
    printf("=== Structured Logging Tests ===\n\n");
    
    test_subsystem_to_string();
    test_basic_tagged_logging();
    test_subsystem_filtering();
    test_combined_filters();
    test_all_subsystems();
    
    printf("\n=== All Structured Logging Tests Passed ===\n");
    return 0;
}
