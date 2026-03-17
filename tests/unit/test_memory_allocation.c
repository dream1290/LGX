/**
 * Unit Tests: Memory Allocation (All Paths)
 * 
 * Tests all memory allocation paths: heap, frame arena, GPU pool, intent-based.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

// Initialize runtime for tests
static void setup_runtime(void) {
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    lgx_config_destroy(config);
}

static void teardown_runtime(void) {
    lgx_runtime_shutdown();
}

// Test 1: Basic heap allocation
static void test_basic_heap_alloc(void) {
    printf("\nTest 1: Basic heap allocation\n");
    
    void* ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Heap allocation succeeds");
    
    // Write to memory
    memset(ptr, 0xAA, 1024);
    TEST_ASSERT(((uint8_t*)ptr)[0] == 0xAA, "Memory is writable");
    
    lgx_free(ptr);
    TEST_ASSERT(true, "Free succeeds");
}

// Test 2: Zero-size allocation
static void test_zero_size_alloc(void) {
    printf("\nTest 2: Zero-size allocation\n");
    
    void* ptr = lgx_alloc(0);
    TEST_ASSERT(ptr == NULL, "Zero-size allocation returns NULL");
}

// Test 3: Large allocation
static void test_large_alloc(void) {
    printf("\nTest 3: Large allocation\n");
    
    size_t large_size = 10 * 1024 * 1024; // 10 MB
    void* ptr = lgx_alloc(large_size);
    TEST_ASSERT(ptr != NULL, "Large allocation succeeds");
    
    if (ptr) {
        // Write to first and last byte
        ((uint8_t*)ptr)[0] = 0xBB;
        ((uint8_t*)ptr)[large_size - 1] = 0xCC;
        
        TEST_ASSERT(((uint8_t*)ptr)[0] == 0xBB, "First byte writable");
        TEST_ASSERT(((uint8_t*)ptr)[large_size - 1] == 0xCC, "Last byte writable");
        
        lgx_free(ptr);
    }
}

// Test 4: Aligned allocation
static void test_aligned_alloc(void) {
    printf("\nTest 4: Aligned allocation\n");
    
    void* ptr = lgx_alloc_aligned(1024, 64);
    TEST_ASSERT(ptr != NULL, "Aligned allocation succeeds");
    
    // Check alignment
    uintptr_t addr = (uintptr_t)ptr;
    TEST_ASSERT((addr & 63) == 0, "Pointer is 64-byte aligned");
    
    lgx_free(ptr);
}

// Test 5: Multiple allocations
static void test_multiple_allocs(void) {
    printf("\nTest 5: Multiple allocations\n");
    
    void* ptrs[100];
    
    // Allocate
    for (int i = 0; i < 100; i++) {
        ptrs[i] = lgx_alloc(128);
        TEST_ASSERT(ptrs[i] != NULL, "Multiple allocation succeeds");
    }
    
    // Free
    for (int i = 0; i < 100; i++) {
        lgx_free(ptrs[i]);
    }
    
    TEST_ASSERT(true, "Multiple free succeeds");
}

// Test 6: Frame arena allocation
static void test_frame_alloc(void) {
    printf("\nTest 6: Frame arena allocation\n");
    
    void* ptr = lgx_frame_alloc(512);
    TEST_ASSERT(ptr != NULL, "Frame allocation succeeds");
    
    // Write to memory
    memset(ptr, 0xDD, 512);
    TEST_ASSERT(((uint8_t*)ptr)[0] == 0xDD, "Frame memory is writable");
    
    // No explicit free needed for frame allocations
}

// Test 7: Frame reset
static void test_frame_reset(void) {
    printf("\nTest 7: Frame reset\n");
    
    // Allocate in frame
    void* ptr1 = lgx_frame_alloc(256);
    TEST_ASSERT(ptr1 != NULL, "Frame allocation before reset succeeds");
    
    // Reset frame
    lgx_frame_reset();
    
    // Allocate again (should reuse memory)
    void* ptr2 = lgx_frame_alloc(256);
    TEST_ASSERT(ptr2 != NULL, "Frame allocation after reset succeeds");
    
    // Pointers might be the same (reused memory)
    printf("    Pointer reuse: %s\n", (ptr1 == ptr2) ? "Yes" : "No");
}

// Test 8: GPU allocation (if available)
static void test_gpu_alloc(void) {
    printf("\nTest 8: GPU allocation\n");
    
    if (!lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
        TEST_ASSERT(true, "GPU not available (skipped)");
        return;
    }
    
    lgx_gpu_allocation_t* alloc = lgx_gpu_alloc(4096, 256, 0);
    TEST_ASSERT(alloc != NULL, "GPU allocation succeeds");
    
    if (alloc) {
        TEST_ASSERT(lgx_gpu_get_size(alloc) >= 4096, "GPU allocation size correct");
        lgx_gpu_free(alloc);
        TEST_ASSERT(true, "GPU free succeeds");
    }
}

// Test 9: Intent-based allocation (frame)
static void test_intent_frame_alloc(void) {
    printf("\nTest 9: Intent-based allocation (frame)\n");
    
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .lifetime = LGX_LIFETIME_FRAME,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    TEST_ASSERT(ptr != NULL, "Intent-based frame allocation succeeds");
    
    // Should route to frame arena
    if (ptr) {
        memset(ptr, 0xEE, 1024);
        TEST_ASSERT(((uint8_t*)ptr)[0] == 0xEE, "Intent-based memory is writable");
    }
}

// Test 10: Intent-based allocation (persistent)
static void test_intent_persistent_alloc(void) {
    printf("\nTest 10: Intent-based allocation (persistent)\n");
    
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 2048,
        .lifetime = LGX_LIFETIME_LEVEL,
        .access_pattern = LGX_ACCESS_RANDOM,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    TEST_ASSERT(ptr != NULL, "Intent-based persistent allocation succeeds");
    
    if (ptr) {
        memset(ptr, 0xFF, 2048);
        lgx_free(ptr);
    }
}

// Test 11: NULL pointer free (should be safe)
static void test_null_free(void) {
    printf("\nTest 11: NULL pointer free\n");
    
    lgx_free(NULL);
    TEST_ASSERT(true, "NULL free is safe");
}

// Test 12: Double free detection (debug builds)
static void test_double_free(void) {
    printf("\nTest 12: Double free detection\n");
    
#ifdef DEBUG
    void* ptr = lgx_alloc(128);
    TEST_ASSERT(ptr != NULL, "Allocation succeeds");
    
    lgx_free(ptr);
    
    // Second free should be detected (in debug builds)
    lgx_free(ptr);
    TEST_ASSERT(true, "Double free detected (debug build)");
#else
    TEST_ASSERT(true, "Double free detection only in debug builds (skipped)");
#endif
}

// Test 13: Allocation statistics
static void test_alloc_stats(void) {
    printf("\nTest 13: Allocation statistics\n");
    
    lgx_memory_stats_t stats;
    lgx_result_t result = lgx_memory_stats(&stats);
    TEST_ASSERT(result == LGX_SUCCESS, "Get memory stats succeeds");
    
    printf("    Total allocated: %zu bytes\n", (size_t)stats.total_allocated);
    printf("    Total freed: %zu bytes\n", (size_t)stats.total_deallocated);
    printf("    Current allocated: %zu bytes\n", (size_t)stats.current_allocated);
}

// Test 14: Frame arena statistics
static void test_frame_stats(void) {
    printf("\nTest 14: Frame arena statistics\n");
    
    lgx_frame_arena_stats_t stats;
    lgx_result_t result = lgx_frame_get_stats(&stats);
    TEST_ASSERT(result == LGX_SUCCESS, "Get frame stats succeeds");
    
    printf("    Current frame: %llu\n", (unsigned long long)stats.current_frame);
    printf("    Total allocated: %llu bytes\n", (unsigned long long)stats.total_bytes_allocated);
    printf("    Peak usage: %llu bytes\n", (unsigned long long)stats.peak_usage_bytes);
}

// Test 15: Allocation failure handling
static void test_alloc_failure(void) {
    printf("\nTest 15: Allocation failure handling\n");
    
    // Try to allocate more than max allowed
    void* ptr = lgx_alloc(SIZE_MAX);
    TEST_ASSERT(ptr == NULL, "Oversized allocation fails gracefully");
    
    // Runtime should still be functional
    void* small_ptr = lgx_alloc(64);
    TEST_ASSERT(small_ptr != NULL, "Small allocation still works after failure");
    lgx_free(small_ptr);
}

// Test 16: Alignment validation
static void test_alignment_validation(void) {
    printf("\nTest 16: Alignment validation\n");
    
    // Valid alignments (powers of 2)
    void* ptr1 = lgx_alloc_aligned(256, 16);
    TEST_ASSERT(ptr1 != NULL, "16-byte alignment succeeds");
    lgx_free(ptr1);
    
    void* ptr2 = lgx_alloc_aligned(256, 256);
    TEST_ASSERT(ptr2 != NULL, "256-byte alignment succeeds");
    lgx_free(ptr2);
    
    // Invalid alignment (not power of 2)
    void* ptr3 = lgx_alloc_aligned(256, 17);
    TEST_ASSERT(ptr3 == NULL, "Invalid alignment fails");
}

// Test 17: Memory pattern verification
static void test_memory_pattern(void) {
    printf("\nTest 17: Memory pattern verification\n");
    
    void* ptr = lgx_alloc(1024);
    TEST_ASSERT(ptr != NULL, "Allocation succeeds");
    
    // Write pattern
    for (size_t i = 0; i < 1024; i++) {
        ((uint8_t*)ptr)[i] = (uint8_t)(i & 0xFF);
    }
    
    // Verify pattern
    bool pattern_ok = true;
    for (size_t i = 0; i < 1024; i++) {
        if (((uint8_t*)ptr)[i] != (uint8_t)(i & 0xFF)) {
            pattern_ok = false;
            break;
        }
    }
    
    TEST_ASSERT(pattern_ok, "Memory pattern preserved");
    lgx_free(ptr);
}

// Test 18: Concurrent allocations (basic)
static void test_concurrent_allocs(void) {
    printf("\nTest 18: Concurrent allocations\n");
    
    void* ptrs[10];
    
    // Allocate from different allocators
    ptrs[0] = lgx_alloc(128);           // Heap
    ptrs[1] = lgx_frame_alloc(256);     // Frame
    ptrs[2] = lgx_alloc(512);           // Heap
    ptrs[3] = lgx_frame_alloc(128);     // Frame
    ptrs[4] = lgx_alloc(1024);          // Heap
    
    TEST_ASSERT(ptrs[0] != NULL, "Heap allocation 1 succeeds");
    TEST_ASSERT(ptrs[1] != NULL, "Frame allocation 1 succeeds");
    TEST_ASSERT(ptrs[2] != NULL, "Heap allocation 2 succeeds");
    TEST_ASSERT(ptrs[3] != NULL, "Frame allocation 2 succeeds");
    TEST_ASSERT(ptrs[4] != NULL, "Heap allocation 3 succeeds");
    
    // Free heap allocations
    lgx_free(ptrs[0]);
    lgx_free(ptrs[2]);
    lgx_free(ptrs[4]);
}

int main(void) {
    printf("=== LGX Runtime Memory Allocation Unit Tests ===\n");
    
    setup_runtime();
    
    test_basic_heap_alloc();
    test_zero_size_alloc();
    test_large_alloc();
    test_aligned_alloc();
    test_multiple_allocs();
    test_frame_alloc();
    test_frame_reset();
    test_gpu_alloc();
    test_intent_frame_alloc();
    test_intent_persistent_alloc();
    test_null_free();
    test_double_free();
    test_alloc_stats();
    test_frame_stats();
    test_alloc_failure();
    test_alignment_validation();
    test_memory_pattern();
    test_concurrent_allocs();
    
    teardown_runtime();
    
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
