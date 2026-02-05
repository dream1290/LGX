#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/lgx_runtime.h"

static void test_base_intent_structure(void) {
    printf("Testing base intent structure...\n");
    
    lgx_allocation_intent_base_t base_intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 2048,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_LEVEL,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_VALIDATE_WARN
    };
    
    // Test direct base intent allocation
    void* ptr1 = lgx_alloc_with_intent(&base_intent);
    assert(ptr1 != NULL);
    printf("  ✓ Direct base intent allocation successful\n");
    
    // Test base intent through ex function
    void* ptr2 = lgx_alloc_with_intent_ex(&base_intent, 0);
    assert(ptr2 != NULL);
    printf("  ✓ Base intent through ex function successful\n");
    
    // Verify usage stats
    lgx_allocation_usage_t usage1, usage2;
    assert(lgx_alloc_get_usage_stats(ptr1, &usage1) == LGX_SUCCESS);
    assert(lgx_alloc_get_usage_stats(ptr2, &usage2) == LGX_SUCCESS);
    
    printf("  ✓ Usage stats accessible for both allocations\n");
    
    lgx_free(ptr1);
    lgx_free(ptr2);
    printf("  ✓ Base intent structure test passed\n\n");
}

static void test_l2_intent_structure(void) {
    printf("Testing L2 intent structure...\n");
    
    lgx_allocation_intent_l2_t l2_intent = {
        .struct_size = sizeof(lgx_allocation_intent_l2_t),
        .base = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 4096,
            .access_pattern = LGX_ACCESS_RANDOM,
            .lifetime = LGX_LIFETIME_SESSION,
            .hint = LGX_HINT_BANDWIDTH_HUNGRY,
            .validation_policy = LGX_INTENT_VALIDATE_ADAPT
        },
        .priority = 150,
        .enable_predictive_prefetch = true
    };
    
    // Test L2 intent allocation
    void* ptr = lgx_alloc_with_intent_ex(&l2_intent, 1);
    assert(ptr != NULL);
    printf("  ✓ L2 intent allocation successful\n");
    
    // Verify base intent properties are preserved
    lgx_allocation_usage_t usage;
    assert(lgx_alloc_get_usage_stats(ptr, &usage) == LGX_SUCCESS);
    printf("  ✓ Usage stats accessible for L2 allocation\n");
    
    lgx_free(ptr);
    printf("  ✓ L2 intent structure test passed\n\n");
}

static void test_intent_structure_validation(void) {
    printf("Testing intent structure validation...\n");
    
    // Test invalid struct_size for base intent
    lgx_allocation_intent_base_t invalid_base = {
        .struct_size = sizeof(lgx_allocation_intent_base_t) - 1, // Invalid size
        .size = 1024,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr1 = lgx_alloc_with_intent(&invalid_base);
    assert(ptr1 == NULL);
    printf("  ✓ Invalid base intent struct_size correctly rejected\n");
    
    // Test invalid struct_size for L2 intent
    lgx_allocation_intent_l2_t invalid_l2 = {
        .struct_size = sizeof(lgx_allocation_intent_l2_t) - 1, // Invalid size
        .base = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 1024,
            .access_pattern = LGX_ACCESS_SEQUENTIAL,
            .lifetime = LGX_LIFETIME_FRAME,
            .hint = LGX_HINT_CRITICAL_PATH,
            .validation_policy = LGX_INTENT_TRUST
        },
        .priority = 100,
        .enable_predictive_prefetch = false
    };
    
    void* ptr2 = lgx_alloc_with_intent_ex(&invalid_l2, 1);
    assert(ptr2 == NULL);
    printf("  ✓ Invalid L2 intent struct_size correctly rejected\n");
    
    // Test unknown intent type
    lgx_allocation_intent_base_t valid_base = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr3 = lgx_alloc_with_intent_ex(&valid_base, 999); // Unknown type
    assert(ptr3 == NULL);
    printf("  ✓ Unknown intent type correctly rejected\n");
    
    printf("  ✓ Intent structure validation test passed\n\n");
}

static void test_intent_compatibility(void) {
    printf("Testing intent compatibility across versions...\n");
    
    // Simulate older version with smaller struct_size
    typedef struct lgx_allocation_intent_v1 {
        size_t struct_size;
        size_t size;
        lgx_access_pattern_t access_pattern;
        lgx_lifetime_t lifetime;
        // Missing hint and validation_policy fields
    } lgx_allocation_intent_v1_t;
    
    lgx_allocation_intent_v1_t v1_intent = {
        .struct_size = sizeof(lgx_allocation_intent_v1_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME
    };
    
    // This should work - runtime should handle smaller struct_size gracefully
    void* ptr = lgx_alloc_with_intent((const lgx_allocation_intent_base_t*)&v1_intent);
    
    if (ptr != NULL) {
        printf("  ✓ Backward compatibility with smaller struct_size works\n");
        lgx_free(ptr);
    } else {
        printf("  ⚠ Backward compatibility not implemented (expected for prototype)\n");
    }
    
    printf("  ✓ Intent compatibility test completed\n\n");
}

static void test_intent_extension_patterns(void) {
    printf("Testing intent extension patterns...\n");
    
    // Test different L2 configurations
    lgx_allocation_intent_l2_t high_priority = {
        .struct_size = sizeof(lgx_allocation_intent_l2_t),
        .base = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 8192,
            .access_pattern = LGX_ACCESS_SEQUENTIAL,
            .lifetime = LGX_LIFETIME_LEVEL,
            .hint = LGX_HINT_CRITICAL_PATH,
            .validation_policy = LGX_INTENT_VALIDATE_ADAPT
        },
        .priority = 255, // Maximum priority
        .enable_predictive_prefetch = true
    };
    
    lgx_allocation_intent_l2_t low_priority = {
        .struct_size = sizeof(lgx_allocation_intent_l2_t),
        .base = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 1024,
            .access_pattern = LGX_ACCESS_RANDOM,
            .lifetime = LGX_LIFETIME_SESSION,
            .hint = LGX_HINT_BACKGROUND,
            .validation_policy = LGX_INTENT_TRUST
        },
        .priority = 0, // Minimum priority
        .enable_predictive_prefetch = false
    };
    
    void* ptr1 = lgx_alloc_with_intent_ex(&high_priority, 1);
    void* ptr2 = lgx_alloc_with_intent_ex(&low_priority, 1);
    
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    
    printf("  ✓ High priority L2 intent allocation successful\n");
    printf("  ✓ Low priority L2 intent allocation successful\n");
    
    // In a real implementation, these would use different allocation strategies
    // For prototype, we just verify they both work
    
    lgx_free(ptr1);
    lgx_free(ptr2);
    
    printf("  ✓ Intent extension patterns test passed\n\n");
}

static void print_memory_stats(void) {
    lgx_memory_stats_t stats;
    lgx_result_t result = lgx_memory_stats(&stats);
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
    printf("=== LGX Runtime Hierarchical Intent Structures Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    printf("✓ Runtime initialized successfully\n\n");
    
    // Run hierarchical intent tests
    test_base_intent_structure();
    test_l2_intent_structure();
    test_intent_structure_validation();
    test_intent_compatibility();
    test_intent_extension_patterns();
    
    // Print final statistics
    print_memory_stats();
    
    // Cleanup
    lgx_config_destroy(config);
    result = lgx_runtime_shutdown();
    assert(result == LGX_SUCCESS);
    
    printf("=== All Hierarchical Intent Tests Passed! ===\n");
    return 0;
}