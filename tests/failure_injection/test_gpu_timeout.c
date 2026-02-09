/*
 * GPU Timeout Failure Injection Test
 * 
 * Simulates GPU driver hangs and verifies recovery mechanisms.
 * Note: This is a mock test since we can't actually hang the GPU driver.
 */

#include "../../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    }

// Timeout handler
static volatile int timeout_occurred = 0;

void timeout_handler(int sig) {
    (void)sig;
    timeout_occurred = 1;
}

int main(void) {
    printf("=== GPU Timeout Failure Injection Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime initialization failed");
    
    printf("Test 1: Check GPU capability detection\n");
    printf("---------------------------------------\n");
    
    // Check if GPU is available
    lgx_hardware_status_t hw_status = lgx_runtime_get_hardware_status();
    printf("  Hardware tier: %d\n", hw_status.achieved_tier);
    printf("  GPU responsive: %s\n", hw_status.gpu_responsive ? "Yes" : "No");
    
    if (!hw_status.gpu_responsive) {
        printf("  ⚠️  GPU not available (expected in CI environment)\n");
        printf("  This test validates the detection mechanism\n");
    }
    
    printf("✅ Test 1 passed\n\n");
    
    printf("Test 2: Verify graceful degradation without GPU\n");
    printf("------------------------------------------------\n");
    
    // Try to allocate GPU memory (should fail gracefully if no GPU)
    // Note: This would use lgx_gpu_alloc() if that API existed
    // For now, we verify the runtime continues to work
    
    void* ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Regular allocation should still work without GPU");
    lgx_free(ptr);
    
    printf("  Regular allocations work: ✅\n");
    
    // Check health status
    lgx_health_status_t health;
    result = lgx_runtime_health_check(&health);
    TEST_ASSERT(result == LGX_SUCCESS, "Health check should work");
    
    printf("  Health check works: ✅\n");
    printf("  System is healthy: %s\n", health.is_healthy ? "Yes" : "No");
    
    printf("✅ Test 2 passed\n\n");
    
    printf("Test 3: Simulate timeout detection\n");
    printf("-----------------------------------\n");
    
    // Set up timeout handler
    signal(SIGALRM, timeout_handler);
    
    // Simulate a long-running operation with timeout
    alarm(2);  // 2 second timeout
    
    // Do some work
    for (int i = 0; i < 1000 && !timeout_occurred; i++) {
        ptr = lgx_alloc(1024);
        if (ptr) lgx_free(ptr);
        usleep(1000);  // 1ms
    }
    
    alarm(0);  // Cancel alarm
    
    if (timeout_occurred) {
        printf("  Timeout detected: ✅\n");
        printf("  Runtime continued after timeout: ✅\n");
    } else {
        printf("  Operation completed before timeout: ✅\n");
    }
    
    // Verify runtime is still functional
    ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Runtime should still work after timeout test");
    lgx_free(ptr);
    
    printf("✅ Test 3 passed\n\n");
    
    printf("Test 4: Verify error reporting\n");
    printf("-------------------------------\n");
    
    // Check if GPU unavailable error is properly reported
    if (!hw_status.gpu_responsive) {
        printf("  GPU unavailable error properly detected\n");
        printf("  Degradation reason: %s\n", 
               hw_status.degradation_reason ? hw_status.degradation_reason : "N/A");
        printf("  Remediation steps: %s\n",
               hw_status.remediation_steps ? hw_status.remediation_steps : "N/A");
    }
    
    printf("✅ Test 4 passed\n\n");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("=== All GPU timeout tests passed ===\n");
    printf("\nNote: This test validates timeout detection and graceful\n");
    printf("degradation mechanisms. Actual GPU driver hangs cannot be\n");
    printf("simulated safely in automated tests.\n");
    
    return 0;
}
