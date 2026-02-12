/**
 * Test suite for persistent heap buddy allocator (Task 3.3.2)
 * 
 * Tests:
 * - Buddy allocator for large allocations (>4KB)
 * - Coalescing on free
 * - Fragmentation tracking
 * - Stress testing with mixed sizes
 */

#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  ✗ FAILED: %s\n", msg); \
        return false; \
    } \
    printf("  ✓ %s\n", msg); \
} while(0)

// Test buddy allocator initialization
static bool test_buddy_init(void) {
    printf("\n[TEST] buddy_allocator_init\n");
    
    lgx_result_t result = lgx_persistent_heap_init();
    TEST_ASSERT(result == LGX_SUCCESS, "Heap initialization");
    TEST_ASSERT(lgx_persistent_heap_is_initialized(), "Heap is initialized");
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
    return true;
}

// Test large allocations (>4KB)
static bool test_large_allocations(void) {
    printf("\n[TEST] large_allocations_buddy\n");
    
    lgx_persistent_heap_init();
    
    // Allocate various large sizes
    void* ptr1 = lgx_heap_alloc(8 * 1024);      // 8KB
    TEST_ASSERT(ptr1 != NULL, "8KB allocation");
    
    void* ptr2 = lgx_heap_alloc(16 * 1024);     // 16KB
    TEST_ASSERT(ptr2 != NULL, "16KB allocation");
    
    void* ptr3 = lgx_heap_alloc(64 * 1024);     // 64KB
    TEST_ASSERT(ptr3 != NULL, "64KB allocation");
    
    void* ptr4 = lgx_heap_alloc(1024 * 1024);   // 1MB
    TEST_ASSERT(ptr4 != NULL, "1MB allocation");
    
    // Free all
    lgx_heap_free(ptr1);
    lgx_heap_free(ptr2);
    lgx_heap_free(ptr3);
    lgx_heap_free(ptr4);
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
    return true;
}

// Test coalescing on free
static bool test_coalescing(void) {
    printf("\n[TEST] buddy_coalescing\n");
    
    lgx_persistent_heap_init();
    
    // Allocate adjacent blocks
    void* ptr1 = lgx_heap_alloc(8 * 1024);
    void* ptr2 = lgx_heap_alloc(8 * 1024);
    void* ptr3 = lgx_heap_alloc(8 * 1024);
    void* ptr4 = lgx_heap_alloc(8 * 1024);
    
    TEST_ASSERT(ptr1 && ptr2 && ptr3 && ptr4, "All allocations succeeded");
    
    // Free in order - should coalesce
    lgx_heap_free(ptr1);
    lgx_heap_free(ptr2);
    lgx_heap_free(ptr3);
    lgx_heap_free(ptr4);
    
    // Allocate larger block - should succeed if coalescing worked
    void* large = lgx_heap_alloc(32 * 1024);
    TEST_ASSERT(large != NULL, "Large allocation after coalescing");
    
    lgx_heap_free(large);
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
    return true;
}

// Test fragmentation tracking
static bool test_fragmentation_tracking(void) {
    printf("\n[TEST] fragmentation_tracking\n");
    
    lgx_persistent_heap_init();
    
    lgx_heap_stats_t stats;
    
    // Initial fragmentation should be low
    lgx_heap_get_stats(&stats);
    printf("  Initial fragmentation: %.2f%%\n", stats.fragmentation_ratio * 100.0f);
    TEST_ASSERT(stats.fragmentation_ratio < 0.1f, "Low initial fragmentation");
    
    // Allocate many blocks
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = lgx_heap_alloc(8 * 1024);
    }
    
    // Free every other block - creates fragmentation
    for (int i = 0; i < 100; i += 2) {
        lgx_heap_free(ptrs[i]);
    }
    
    lgx_heap_get_stats(&stats);
    printf("  Fragmentation after pattern: %.2f%%\n", stats.fragmentation_ratio * 100.0f);
    
    // Free remaining blocks
    for (int i = 1; i < 100; i += 2) {
        lgx_heap_free(ptrs[i]);
    }
    
    // After freeing all, fragmentation should be low again (coalescing)
    lgx_heap_get_stats(&stats);
    printf("  Final fragmentation: %.2f%%\n", stats.fragmentation_ratio * 100.0f);
    TEST_ASSERT(stats.fragmentation_ratio < 0.1f, "Low fragmentation after coalescing");
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
    return true;
}

// Test mixed small and large allocations
static bool test_mixed_allocations(void) {
    printf("\n[TEST] mixed_small_large_allocations\n");
    
    lgx_persistent_heap_init();
    
    // Mix of small (segregated fit) and large (buddy) allocations
    void* small1 = lgx_heap_alloc(64);          // Segregated fit
    void* large1 = lgx_heap_alloc(8 * 1024);    // Buddy
    void* small2 = lgx_heap_alloc(256);         // Segregated fit
    void* large2 = lgx_heap_alloc(16 * 1024);   // Buddy
    void* small3 = lgx_heap_alloc(1024);        // Segregated fit
    void* large3 = lgx_heap_alloc(64 * 1024);   // Buddy
    
    TEST_ASSERT(small1 && large1 && small2 && large2 && small3 && large3,
                "All mixed allocations succeeded");
    
    // Free in mixed order
    lgx_heap_free(large1);
    lgx_heap_free(small1);
    lgx_heap_free(large3);
    lgx_heap_free(small2);
    lgx_heap_free(large2);
    lgx_heap_free(small3);
    
    lgx_heap_stats_t stats;
    lgx_heap_get_stats(&stats);
    TEST_ASSERT(stats.active_allocations == 0, "All memory freed");
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
    return true;
}

// Test buddy allocator stress
static bool test_buddy_stress(void) {
    printf("\n[TEST] buddy_stress_test\n");
    
    lgx_persistent_heap_init();
    
    #define NUM_BUDDY_ITERATIONS 20
    #define ALLOCS_PER_ITERATION 50
    
    for (int iter = 0; iter < NUM_BUDDY_ITERATIONS; iter++) {
        void* ptrs[ALLOCS_PER_ITERATION];
        
        // Allocate random large sizes
        for (int i = 0; i < ALLOCS_PER_ITERATION; i++) {
            size_t size = (8 + (rand() % 120)) * 1024;  // 8KB - 128KB
            ptrs[i] = lgx_heap_alloc(size);
            
            if (!ptrs[i]) {
                printf("  Allocation failed at iteration %d, alloc %d\n", iter, i);
                break;
            }
        }
        
        // Free in random order
        for (int i = 0; i < ALLOCS_PER_ITERATION; i++) {
            int idx = rand() % ALLOCS_PER_ITERATION;
            if (ptrs[idx]) {
                lgx_heap_free(ptrs[idx]);
                ptrs[idx] = NULL;
            }
        }
        
        // Free any remaining
        for (int i = 0; i < ALLOCS_PER_ITERATION; i++) {
            if (ptrs[i]) {
                lgx_heap_free(ptrs[i]);
            }
        }
    }
    #undef ALLOCS_PER_ITERATION
    
    lgx_heap_stats_t stats;
    lgx_heap_get_stats(&stats);
    
    printf("  Completed %d iterations\n", NUM_BUDDY_ITERATIONS);
    #undef NUM_BUDDY_ITERATIONS
    printf("  Total allocations: %llu\n", (unsigned long long)stats.total_allocations);
    printf("  Total frees: %llu\n", (unsigned long long)stats.total_frees);
    printf("  Peak bytes: %llu\n", (unsigned long long)stats.peak_bytes);
    printf("  Fragmentation: %.2f%%\n", stats.fragmentation_ratio * 100.0f);
    
    TEST_ASSERT(stats.active_allocations == 0, "All memory freed");
    TEST_ASSERT(stats.fragmentation_ratio < 0.5f, "Fragmentation < 50%");
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
    return true;
}

// Test very large allocations
static bool test_very_large_allocations(void) {
    printf("\n[TEST] very_large_allocations\n");
    
    lgx_persistent_heap_init();
    
    // Try allocating very large blocks
    void* ptr1 = lgx_heap_alloc(4 * 1024 * 1024);   // 4MB
    TEST_ASSERT(ptr1 != NULL, "4MB allocation");
    
    void* ptr2 = lgx_heap_alloc(8 * 1024 * 1024);   // 8MB
    TEST_ASSERT(ptr2 != NULL, "8MB allocation");
    
    void* ptr3 = lgx_heap_alloc(16 * 1024 * 1024);  // 16MB
    TEST_ASSERT(ptr3 != NULL, "16MB allocation");
    
    lgx_heap_free(ptr1);
    lgx_heap_free(ptr2);
    lgx_heap_free(ptr3);
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
    return true;
}

int main(void) {
    printf("=== Persistent Heap Buddy Allocator Tests ===\n");
    printf("Testing buddy allocator for large allocations (Task 3.3.2)\n");
    
    int passed = 0;
    int failed = 0;
    
    if (test_buddy_init()) passed++; else failed++;
    if (test_large_allocations()) passed++; else failed++;
    if (test_coalescing()) passed++; else failed++;
    if (test_fragmentation_tracking()) passed++; else failed++;
    if (test_mixed_allocations()) passed++; else failed++;
    if (test_buddy_stress()) passed++; else failed++;
    if (test_very_large_allocations()) passed++; else failed++;
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    
    if (failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n");
        return 1;
    }
}
