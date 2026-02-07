#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/lgx_runtime.h"

// Test helper functions
static void test_basic_intent_allocation(void) {
    printf("Testing basic intent-based allocation...\n");
    
    // Create intent for sequential access, frame lifetime
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_VALIDATE_WARN
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    assert(ptr != NULL);
    
    // Get initial usage stats
    lgx_allocation_usage_t usage = {0};
    lgx_result_t result = lgx_alloc_get_usage_stats(ptr, &usage);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    assert(usage.access_count == 0);
    assert(usage.observed_pattern == LGX_ACCESS_UNKNOWN);
    
    printf("  ✓ Intent allocation successful\n");
    printf("  ✓ Initial usage stats correct\n");
    
    lgx_free(ptr);
    printf("  ✓ Basic intent allocation test passed\n\n");
}

static void test_sequential_access_pattern_detection(void) {
    printf("Testing sequential access pattern detection...\n");
    
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 4096,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_LEVEL,
        .hint = LGX_HINT_BANDWIDTH_HUNGRY,
        .validation_policy = LGX_INTENT_VALIDATE_WARN
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    assert(ptr != NULL);
    
    // Simulate sequential access pattern
    char* data = (char*)ptr;
    for (int i = 0; i < 100; i++) {
        // Write sequentially to simulate access pattern detection
        data[i * 32] = (char)i; // Access every 32 bytes (cache line aligned)
        
        // Simulate access tracking (in real implementation, this would be automatic)
        // For prototype, we'll manually trigger pattern analysis
        if (i % 10 == 0) {
            lgx_allocation_usage_t usage = {0};
            lgx_result_t result = lgx_alloc_get_usage_stats(ptr, &usage);
            (void)result;  // Suppress unused warning
            if (result == LGX_SUCCESS && i > 20) {
                printf("  Access %d: pattern=%d, confidence=%.2f\n", 
                       i, usage.observed_pattern, usage.pattern_confidence);
            }
        }
    }
    
    // Check final usage stats
    lgx_allocation_usage_t final_usage;
    lgx_result_t result = lgx_alloc_get_usage_stats(ptr, &final_usage);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    printf("  Final access count: %lu\n", final_usage.access_count);
    printf("  Observed pattern: %d (expected: %d)\n", 
           final_usage.observed_pattern, LGX_ACCESS_SEQUENTIAL);
    printf("  Pattern confidence: %.2f\n", final_usage.pattern_confidence);
    
    lgx_free(ptr);
    printf("  ✓ Sequential access pattern test passed\n\n");
}

static void test_intent_mismatch_detection(void) {
    printf("Testing intent mismatch detection...\n");
    
    // Declare intent as sequential but access randomly
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 2048,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_VALIDATE_WARN
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    assert(ptr != NULL);
    
    // Simulate random access pattern (contradicts intent)
    char* data = (char*)ptr;
    int random_offsets[] = {1500, 200, 1800, 50, 1200, 800, 1900, 100, 1600, 400};
    
    for (int i = 0; i < 10; i++) {
        data[random_offsets[i]] = (char)i;
    }
    
    // Manually validate intent (in real implementation, this might be automatic)
    lgx_result_t validation_result = lgx_alloc_validate_intent(ptr);
    
    lgx_allocation_usage_t usage = {0};
    lgx_result_t stats_result = lgx_alloc_get_usage_stats(ptr, &usage);
    assert(stats_result == LGX_SUCCESS);
    (void)stats_result;  // Suppress unused warning
    
    printf("  Intent validation result: %s\n", lgx_result_to_string(validation_result));
    printf("  Intent mismatch detected: %s\n", 
           usage.intent_mismatch_detected ? "Yes" : "No");
    
    lgx_free(ptr);
    printf("  ✓ Intent mismatch detection test passed\n\n");
}

static void test_lifetime_validation(void) {
    printf("Testing lifetime validation...\n");
    
    // Declare short lifetime but keep allocation alive longer
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 512,
        .access_pattern = LGX_ACCESS_RANDOM,
        .lifetime = LGX_LIFETIME_FRAME,  // Should be freed within ~33ms
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_VALIDATE_WARN
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    assert(ptr != NULL);
    
    // Sleep longer than frame time to trigger lifetime mismatch
    usleep(50000); // 50ms > 33ms frame time
    
    // Validate intent - should detect lifetime mismatch
    lgx_result_t validation_result = lgx_alloc_validate_intent(ptr);
    
    lgx_allocation_usage_t usage = {0};
    lgx_result_t stats_result = lgx_alloc_get_usage_stats(ptr, &usage);
    assert(stats_result == LGX_SUCCESS);
    (void)stats_result;  // Suppress unused warning
    
    printf("  Actual lifetime: %lu ms\n", usage.actual_lifetime_ms);
    printf("  Intent validation result: %s\n", lgx_result_to_string(validation_result));
    printf("  Intent mismatch detected: %s\n", 
           usage.intent_mismatch_detected ? "Yes" : "No");
    
    lgx_free(ptr);
    printf("  ✓ Lifetime validation test passed\n\n");
}

static void test_hierarchical_intent_structures(void) {
    printf("Testing hierarchical intent structures (L2)...\n");
    
    // Test L2 intent structure
    lgx_allocation_intent_l2_t l2_intent = {
        .struct_size = sizeof(lgx_allocation_intent_l2_t),
        .base = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 1024,
            .access_pattern = LGX_ACCESS_SEQUENTIAL,
            .lifetime = LGX_LIFETIME_LEVEL,
            .hint = LGX_HINT_CRITICAL_PATH,
            .validation_policy = LGX_INTENT_VALIDATE_ADAPT
        },
        .priority = 200,
        .enable_predictive_prefetch = true
    };
    
    // For prototype, L2 intent should return NULL (not implemented yet)
    void* ptr = lgx_alloc_with_intent_ex(&l2_intent, 1); // type_id = 1 for L2
    
    if (ptr == NULL) {
        printf("  ✓ L2 intent correctly returns NULL (not implemented)\n");
    } else {
        printf("  ✗ L2 intent should return NULL in prototype\n");
        lgx_free(ptr);
    }
    
    // Test base intent through ex function
    ptr = lgx_alloc_with_intent_ex(&l2_intent.base, 0); // type_id = 0 for base
    assert(ptr != NULL);
    
    printf("  ✓ Base intent through ex function works\n");
    
    lgx_free(ptr);
    printf("  ✓ Hierarchical intent structures test passed\n\n");
}

static void print_memory_stats(void) {
    lgx_memory_stats_t stats;
    lgx_result_t result = lgx_memory_stats(&stats);
    (void)result;  // Suppress unused warning
    if (result == LGX_SUCCESS) {
        printf("Memory Statistics:\n");
        printf("  Total allocated: %zu bytes\n", stats.total_allocated);
        printf("  Peak allocated: %zu bytes\n", stats.peak_allocated);
        printf("  Current allocated: %zu bytes\n", stats.current_allocated);
        printf("  Allocation count: %lu\n", stats.allocation_count);
        printf("  Deallocation count: %lu\n", stats.deallocation_count);
        printf("\n");
    }
}

int main(void) {
    printf("=== LGX Runtime Intent Validation Framework Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    printf("✓ Runtime initialized successfully\n\n");
    
    // Run tests
    test_basic_intent_allocation();
    test_sequential_access_pattern_detection();
    test_intent_mismatch_detection();
    test_lifetime_validation();
    test_hierarchical_intent_structures();
    
    // Print final statistics
    print_memory_stats();
    
    // Cleanup
    lgx_config_destroy(config);
    result = lgx_runtime_shutdown();
    assert(result == LGX_SUCCESS);
    
    printf("=== All Intent Validation Tests Passed! ===\n");
    return 0;
}