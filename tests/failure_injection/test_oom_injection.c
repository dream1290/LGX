/*
 * Out-of-Memory (OOM) Failure Injection Test
 * 
 * Simulates OOM conditions mid-frame and verifies graceful degradation.
 */

#include "../../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    }

int main(void) {
    printf("=== OOM Failure Injection Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Runtime initialization failed");
    
    printf("Test 1: Allocate until pool exhaustion\n");
    printf("---------------------------------------\n");
    
    // Allocate until we hit the limit
    void** ptrs = malloc(sizeof(void*) * 10000);
    int alloc_count = 0;
    int failed_count = 0;
    
    for (int i = 0; i < 10000; i++) {
        ptrs[i] = lgx_alloc(1024 * 1024);  // 1MB allocations
        if (ptrs[i] == NULL) {
            failed_count++;
            if (failed_count > 10) {
                // Stop after 10 consecutive failures
                break;
            }
        } else {
            alloc_count++;
            failed_count = 0;
        }
    }
    
    printf("  Allocated: %d blocks\n", alloc_count);
    printf("  Failed: %d attempts\n", failed_count);
    
    TEST_ASSERT(alloc_count > 0, "Should allocate at least some blocks");
    TEST_ASSERT(failed_count > 0, "Should eventually fail allocations");
    
    // Check that runtime is still functional
    lgx_health_status_t health;
    result = lgx_runtime_health_check(&health);
    TEST_ASSERT(result == LGX_SUCCESS, "Health check should still work");
    
    printf("  Health check: %s\n", health.is_healthy ? "Healthy" : "Degraded");
    printf("  Allocation failures: %u\n", health.allocation_failures);
    
    printf("✅ Test 1 passed\n\n");
    
    printf("Test 2: Verify graceful degradation\n");
    printf("------------------------------------\n");
    
    // Try to allocate after OOM
    void* ptr = lgx_alloc(1024);
    if (ptr == NULL) {
        printf("  Allocation failed (expected)\n");
        
        // Check error context
        lgx_error_context_t error = lgx_get_last_error();
        printf("  Error code: %d (%s)\n", error.error_code, lgx_result_to_string(error.error_code));
        printf("  Error message: %s\n", error.error_message);
        
        TEST_ASSERT(error.error_code == LGX_ERROR_OUT_OF_MEMORY, 
                   "Should report OOM error");
    } else {
        printf("  Allocation succeeded (pool recovered)\n");
        lgx_free(ptr);
    }
    
    printf("✅ Test 2 passed\n\n");
    
    printf("Test 3: Verify runtime continues after OOM\n");
    printf("-------------------------------------------\n");
    
    // Free some allocations
    int freed = 0;
    for (int i = 0; i < alloc_count && freed < 100; i++) {
        if (ptrs[i]) {
            lgx_free(ptrs[i]);
            ptrs[i] = NULL;
            freed++;
        }
    }
    
    printf("  Freed: %d blocks\n", freed);
    
    // Try allocating again
    ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Should be able to allocate after freeing");
    lgx_free(ptr);
    
    printf("  New allocation succeeded\n");
    printf("✅ Test 3 passed\n\n");
    
    // Cleanup
    for (int i = 0; i < alloc_count; i++) {
        if (ptrs[i]) {
            lgx_free(ptrs[i]);
        }
    }
    free(ptrs);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("=== All OOM tests passed ===\n");
    return 0;
}
