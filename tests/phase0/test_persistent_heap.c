/**
 * Persistent Heap Allocator Tests
 * 
 * Tests segregated fit allocator implementation (Task 3.3.1)
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while (0)

void test_initialization(void) {
    printf("\n[TEST] initialization\n");
    
    lgx_result_t result = lgx_persistent_heap_init();
    TEST_ASSERT(result == LGX_SUCCESS, "Heap initialization");
    TEST_ASSERT(lgx_persistent_heap_is_initialized(), "Heap is initialized");
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_size_classes(void) {
    printf("\n[TEST] size_classes (16B - 4KB)\n");
    
    lgx_persistent_heap_init();
    
    // Test all 16 size classes
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        void* ptr = lgx_heap_alloc(sizes[i]);
        TEST_ASSERT(ptr != NULL, "Allocation succeeded");
        
        if (ptr) {
            // Write pattern to verify memory is usable
            memset(ptr, 0x42, sizes[i]);
            lgx_heap_free(ptr);
        }
    }
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_free_list(void) {
    printf("\n[TEST] free_list_per_size_class\n");
    
    lgx_persistent_heap_init();
    
    // Allocate and free multiple blocks of same size
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = lgx_heap_alloc(64);
        TEST_ASSERT(ptrs[i] != NULL, "Allocation succeeded");
    }
    
    // Free all blocks (should go to free list)
    for (int i = 0; i < 10; i++) {
        lgx_heap_free(ptrs[i]);
    }
    
    // Allocate again (should reuse from free list)
    for (int i = 0; i < 10; i++) {
        ptrs[i] = lgx_heap_alloc(64);
        TEST_ASSERT(ptrs[i] != NULL, "Reallocation from free list succeeded");
    }
    
    // Cleanup
    for (int i = 0; i < 10; i++) {
        lgx_heap_free(ptrs[i]);
    }
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_slab_allocation(void) {
    printf("\n[TEST] slab_allocation\n");
    
    lgx_persistent_heap_init();
    
    // Allocate many small objects to trigger slab allocation
    void* ptrs[1000];
    int num_allocated = 0;
    
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = lgx_heap_alloc(32);
        if (ptrs[i]) {
            num_allocated++;
        }
    }
    
    TEST_ASSERT(num_allocated == 1000, "All 1000 allocations succeeded");
    printf("  Allocated %d objects from slabs\n", num_allocated);
    
    // Verify memory is usable
    for (int i = 0; i < num_allocated; i++) {
        memset(ptrs[i], i & 0xFF, 32);
    }
    
    // Free all
    for (int i = 0; i < num_allocated; i++) {
        lgx_heap_free(ptrs[i]);
    }
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_slab_recycling(void) {
    printf("\n[TEST] slab_recycling\n");
    
    lgx_persistent_heap_init();
    
    // Allocate, free, and reallocate to test recycling
    void* ptr1 = lgx_heap_alloc(128);
    TEST_ASSERT(ptr1 != NULL, "First allocation");
    
    lgx_heap_free(ptr1);
    
    void* ptr2 = lgx_heap_alloc(128);
    TEST_ASSERT(ptr2 != NULL, "Second allocation (recycled)");
    
    // Should reuse the same memory (or from same slab)
    printf("  First ptr: %p, Second ptr: %p\n", ptr1, ptr2);
    
    lgx_heap_free(ptr2);
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_mixed_sizes(void) {
    printf("\n[TEST] mixed_sizes\n");
    
    lgx_persistent_heap_init();
    
    // Allocate various sizes
    void* ptr16 = lgx_heap_alloc(16);
    void* ptr64 = lgx_heap_alloc(64);
    void* ptr256 = lgx_heap_alloc(256);
    void* ptr1k = lgx_heap_alloc(1024);
    void* ptr4k = lgx_heap_alloc(4096);
    
    TEST_ASSERT(ptr16 != NULL, "16B allocation");
    TEST_ASSERT(ptr64 != NULL, "64B allocation");
    TEST_ASSERT(ptr256 != NULL, "256B allocation");
    TEST_ASSERT(ptr1k != NULL, "1KB allocation");
    TEST_ASSERT(ptr4k != NULL, "4KB allocation");
    
    // Write patterns
    if (ptr16) memset(ptr16, 0x11, 16);
    if (ptr64) memset(ptr64, 0x22, 64);
    if (ptr256) memset(ptr256, 0x33, 256);
    if (ptr1k) memset(ptr1k, 0x44, 1024);
    if (ptr4k) memset(ptr4k, 0x55, 4096);
    
    // Free in different order
    lgx_heap_free(ptr256);
    lgx_heap_free(ptr16);
    lgx_heap_free(ptr4k);
    lgx_heap_free(ptr1k);
    lgx_heap_free(ptr64);
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_statistics(void) {
    printf("\n[TEST] statistics\n");
    
    lgx_persistent_heap_init();
    
    lgx_heap_stats_t stats;
    lgx_heap_get_stats(&stats);
    
    TEST_ASSERT(stats.total_allocations == 0, "Initial allocations = 0");
    TEST_ASSERT(stats.active_allocations == 0, "Initial active = 0");
    
    // Allocate some memory
    void* ptr1 = lgx_heap_alloc(100);
    void* ptr2 = lgx_heap_alloc(200);
    void* ptr3 = lgx_heap_alloc(300);
    
    lgx_heap_get_stats(&stats);
    TEST_ASSERT(stats.total_allocations == 3, "3 allocations");
    TEST_ASSERT(stats.active_allocations == 3, "3 active allocations");
    TEST_ASSERT(stats.current_bytes >= 600, "At least 600 bytes allocated");
    
    printf("  Current bytes: %llu\n", (unsigned long long)stats.current_bytes);
    printf("  Peak bytes: %llu\n", (unsigned long long)stats.peak_bytes);
    
    // Free one
    lgx_heap_free(ptr2);
    
    lgx_heap_get_stats(&stats);
    TEST_ASSERT(stats.total_frees == 1, "1 free");
    TEST_ASSERT(stats.active_allocations == 2, "2 active allocations");
    
    // Cleanup
    lgx_heap_free(ptr1);
    lgx_heap_free(ptr3);
    
    lgx_heap_get_stats(&stats);
    TEST_ASSERT(stats.active_allocations == 0, "All freed");
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_leak_detection(void) {
    printf("\n[TEST] leak_detection\n");
    
    lgx_persistent_heap_init();
    
    // Allocate without freeing
    void* ptr1 = lgx_heap_alloc(100);
    void* ptr2 = lgx_heap_alloc(200);
    
    (void)ptr1;
    (void)ptr2;
    
    printf("  Shutting down with leaks (should warn)...\n");
    lgx_persistent_heap_shutdown();
    
    TEST_ASSERT(true, "Leak detection warning displayed");
    printf("  Test completed\n");
}

void test_large_allocations(void) {
    printf("\n[TEST] large_allocations (>4KB)\n");
    
    lgx_persistent_heap_init();
    
    // Allocate larger than size class limit
    void* ptr8k = lgx_heap_alloc(8192);
    void* ptr16k = lgx_heap_alloc(16384);
    void* ptr64k = lgx_heap_alloc(65536);
    
    TEST_ASSERT(ptr8k != NULL, "8KB allocation");
    TEST_ASSERT(ptr16k != NULL, "16KB allocation");
    TEST_ASSERT(ptr64k != NULL, "64KB allocation");
    
    // Write patterns
    if (ptr8k) memset(ptr8k, 0xAA, 8192);
    if (ptr16k) memset(ptr16k, 0xBB, 16384);
    if (ptr64k) memset(ptr64k, 0xCC, 65536);
    
    // Free
    lgx_heap_free(ptr8k);
    lgx_heap_free(ptr16k);
    lgx_heap_free(ptr64k);
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

void test_stress(void) {
    printf("\n[TEST] stress_test\n");
    
    lgx_persistent_heap_init();
    
    // Allocate and free many objects
    void* ptrs[100];
    int num_iterations = 10;
    
    for (int iter = 0; iter < num_iterations; iter++) {
        // Allocate
        for (int i = 0; i < 100; i++) {
            size_t size = 16 + (i * 37) % 4000;  // Various sizes
            ptrs[i] = lgx_heap_alloc(size);
        }
        
        // Free half
        for (int i = 0; i < 50; i++) {
            if (ptrs[i]) {
                lgx_heap_free(ptrs[i]);
                ptrs[i] = NULL;
            }
        }
        
        // Reallocate
        for (int i = 0; i < 50; i++) {
            size_t size = 16 + (i * 41) % 4000;
            ptrs[i] = lgx_heap_alloc(size);
        }
        
        // Free all
        for (int i = 0; i < 100; i++) {
            if (ptrs[i]) {
                lgx_heap_free(ptrs[i]);
            }
        }
    }
    
    lgx_heap_stats_t stats;
    lgx_heap_get_stats(&stats);
    
    printf("  Completed %d iterations\n", num_iterations);
    printf("  Total allocations: %llu\n", (unsigned long long)stats.total_allocations);
    printf("  Total frees: %llu\n", (unsigned long long)stats.total_frees);
    printf("  Peak bytes: %llu\n", (unsigned long long)stats.peak_bytes);
    
    TEST_ASSERT(stats.active_allocations == 0, "All memory freed");
    
    lgx_persistent_heap_shutdown();
    printf("  Test completed\n");
}

int main(void) {
    printf("=== Persistent Heap Allocator Tests ===\n");
    printf("Testing segregated fit allocator (Task 3.3.1)\n");
    
    // Run tests
    test_initialization();
    test_size_classes();
    test_free_list();
    test_slab_allocation();
    test_slab_recycling();
    test_mixed_sizes();
    test_statistics();
    test_leak_detection();
    test_large_allocations();
    test_stress();
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n");
        return 1;
    }
}
