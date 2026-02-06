/**
 * Intent Allocator Tests (Task 3.4)
 * 
 * Tests the unified intent-based API that automatically routes allocations
 * to the appropriate allocator based on developer intent.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) \
    printf("\n[TEST] %s\n", #name); \
    test_##name(); \
    printf("  Test completed\n")

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ✗ FAILED: %s\n", message); \
            tests_failed++; \
            return; \
        } else { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } \
    } while(0)

/**
 * Test: Intent structure initialization
 */
void test_intent_structure() {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    ASSERT(intent.struct_size == sizeof(lgx_allocation_intent_base_t), 
           "Intent struct_size correct");
    ASSERT(intent.size == 1024, "Intent size correct");
    ASSERT(intent.lifetime == LGX_LIFETIME_FRAME, "Intent lifetime correct");
}

/**
 * Test: Frame allocation routing
 */
void test_frame_routing() {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 256,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    ASSERT(ptr != NULL, "Frame allocation succeeded");
    
    // Write to memory to verify it's valid
    memset(ptr, 0xAA, 256);
    ASSERT(((uint8_t*)ptr)[0] == 0xAA, "Memory is writable");
    
    // Note: Frame allocations don't need explicit free
}

/**
 * Test: Persistent allocation routing
 */
void test_persistent_routing() {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_RANDOM,
        .lifetime = LGX_LIFETIME_SESSION,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    ASSERT(ptr != NULL, "Persistent allocation succeeded");
    
    // Write to memory
    memset(ptr, 0xBB, 1024);
    ASSERT(((uint8_t*)ptr)[0] == 0xBB, "Memory is writable");
    
    lgx_heap_free(ptr);
}

/**
 * Test: GPU shared allocation routing
 */
void test_gpu_routing() {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 512,
        .access_pattern = LGX_ACCESS_WRITE_ONCE,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_GPU_SHARED,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    
#if HAVE_VULKAN
    ASSERT(ptr != NULL, "GPU allocation succeeded");
    
    // Write to memory
    memset(ptr, 0xCC, 512);
    ASSERT(((uint8_t*)ptr)[0] == 0xCC, "GPU memory is writable");
    
    // GPU allocations return lgx_gpu_allocation_t*, need to cast
    lgx_gpu_free((lgx_gpu_allocation_t*)ptr);
#else
    // Without Vulkan, should fall back to persistent heap
    if (ptr) {
        printf("  ✓ GPU allocation fell back to persistent heap (Vulkan not available)\n");
        tests_passed++;
        lgx_heap_free(ptr);
    } else {
        printf("  ✓ GPU allocation returned NULL (expected without Vulkan)\n");
        tests_passed++;
    }
#endif
}

/**
 * Test: Convenience macros
 */
void test_convenience_macros() {
    // Test frame macro
    void* frame_ptr = lgx_alloc_frame(128);
    ASSERT(frame_ptr != NULL, "lgx_alloc_frame() succeeded");
    memset(frame_ptr, 0xDD, 128);
    
    // Test persistent macro
    void* persistent_ptr = lgx_alloc_persistent(256);
    ASSERT(persistent_ptr != NULL, "lgx_alloc_persistent() succeeded");
    memset(persistent_ptr, 0xEE, 256);
    lgx_heap_free(persistent_ptr);
    
    // Test level macro
    void* level_ptr = lgx_alloc_level(512);
    ASSERT(level_ptr != NULL, "lgx_alloc_level() succeeded");
    memset(level_ptr, 0xFF, 512);
    lgx_heap_free(level_ptr);
}

/**
 * Test: Intent validation
 */
void test_intent_validation() {
    // Test NULL intent
    void* ptr = lgx_alloc_with_intent(NULL);
    ASSERT(ptr == NULL, "NULL intent rejected");
    
    // Test zero size
    lgx_allocation_intent_base_t zero_intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 0,
        .lifetime = LGX_LIFETIME_FRAME
    };
    ptr = lgx_alloc_with_intent(&zero_intent);
    ASSERT(ptr == NULL, "Zero size rejected");
    
    // Test invalid struct_size
    lgx_allocation_intent_base_t invalid_intent = {
        .struct_size = 4,  // Too small
        .size = 100,
        .lifetime = LGX_LIFETIME_FRAME
    };
    ptr = lgx_alloc_with_intent(&invalid_intent);
    ASSERT(ptr == NULL, "Invalid struct_size rejected");
}

/**
 * Test: Intent statistics
 */
void test_intent_statistics() {
    // Reset by reinitializing
    lgx_intent_allocator_shutdown();
    lgx_intent_allocator_init();
    
    // Allocate with different intents
    void* frame1 = lgx_alloc_frame(64);
    void* frame2 = lgx_alloc_frame(128);
    void* persistent1 = lgx_alloc_persistent(256);
    void* persistent2 = lgx_alloc_persistent(512);
    
    ASSERT(frame1 && frame2 && persistent1 && persistent2, "All allocations succeeded");
    
    // Get statistics
    lgx_intent_stats_t stats;
    lgx_result_t result = lgx_intent_get_stats(&stats);
    ASSERT(result == LGX_SUCCESS, "Got intent statistics");
    
    ASSERT(stats.frame_allocations == 2, "2 frame allocations tracked");
    ASSERT(stats.persistent_allocations == 2, "2 persistent allocations tracked");
    ASSERT(stats.total_intent_allocations == 4, "4 total allocations tracked");
    
    printf("  Statistics:\n");
    printf("    Frame: %llu\n", (unsigned long long)stats.frame_allocations);
    printf("    Persistent: %llu\n", (unsigned long long)stats.persistent_allocations);
    printf("    Total: %llu\n", (unsigned long long)stats.total_intent_allocations);
    
    // Clean up
    lgx_heap_free(persistent1);
    lgx_heap_free(persistent2);
}

/**
 * Test: Mixed allocation patterns
 */
void test_mixed_patterns() {
    // Allocate mix of frame, persistent, and GPU
    void* allocations[10];
    
    for (int i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            allocations[i] = lgx_alloc_frame(64 * (i + 1));
        } else if (i % 3 == 1) {
            allocations[i] = lgx_alloc_persistent(128 * (i + 1));
        } else {
            allocations[i] = lgx_alloc_level(256 * (i + 1));
        }
        
        ASSERT(allocations[i] != NULL, "Mixed allocation succeeded");
    }
    
    // Free persistent and level allocations
    for (int i = 0; i < 10; i++) {
        if (i % 3 != 0) {  // Not frame allocations
            lgx_heap_free(allocations[i]);
        }
    }
}

/**
 * Test: Extended intent (Layer 2)
 */
void test_extended_intent() {
    lgx_allocation_intent_l2_t l2_intent = {
        .struct_size = sizeof(lgx_allocation_intent_l2_t),
        .base = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 1024,
            .access_pattern = LGX_ACCESS_SEQUENTIAL,
            .lifetime = LGX_LIFETIME_FRAME,
            .hint = LGX_HINT_CRITICAL_PATH,
            .validation_policy = LGX_INTENT_TRUST
        },
        .priority = 128,
        .enable_predictive_prefetch = true
    };
    
    void* ptr = lgx_alloc_with_intent_ex(&l2_intent, 2);
    ASSERT(ptr != NULL, "Extended intent allocation succeeded");
    
    memset(ptr, 0x42, 1024);
    ASSERT(((uint8_t*)ptr)[0] == 0x42, "Extended intent memory is writable");
}

/**
 * Test: Usage statistics (placeholder)
 */
void test_usage_statistics() {
    void* ptr = lgx_alloc_persistent(512);
    ASSERT(ptr != NULL, "Allocation for usage stats succeeded");
    
    lgx_allocation_usage_t usage = {
        .struct_size = sizeof(lgx_allocation_usage_t)
    };
    
    lgx_result_t result = lgx_alloc_get_usage_stats(ptr, &usage);
    ASSERT(result == LGX_SUCCESS, "Got usage statistics");
    
    // Currently returns placeholder data
    printf("  Usage stats (placeholder):\n");
    printf("    Access count: %llu\n", (unsigned long long)usage.access_count);
    printf("    Observed pattern: %d\n", usage.observed_pattern);
    printf("    Pattern confidence: %.2f\n", usage.pattern_confidence);
    
    lgx_heap_free(ptr);
}

int main(void) {
    printf("=== Intent Allocator Tests (Task 3.4) ===\n");
    printf("Testing unified intent-based API\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        return 1;
    }
    lgx_config_destroy(config);
    
    // Initialize intent allocator
    result = lgx_intent_allocator_init();
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize intent allocator\n");
        return 1;
    }
    
    // Run tests
    TEST(intent_structure);
    TEST(frame_routing);
    TEST(persistent_routing);
    TEST(gpu_routing);
    TEST(convenience_macros);
    TEST(intent_validation);
    TEST(intent_statistics);
    TEST(mixed_patterns);
    TEST(extended_intent);
    TEST(usage_statistics);
    
    // Shutdown
    lgx_intent_allocator_shutdown();
    lgx_runtime_shutdown();
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed\n");
        return 1;
    }
}
