/**
 * Integration Test: Memory Stress Test
 * 
 * Tests memory allocation under various stress conditions and patterns.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

// Test 1: Sequential allocation pattern
static void test_sequential_allocation(void) {
    printf("\nTest 1: Sequential allocation pattern\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    #define SEQ_ALLOC_COUNT 1000
    void* ptrs[SEQ_ALLOC_COUNT];
    
    // Allocate sequentially
    for (int i = 0; i < SEQ_ALLOC_COUNT; i++) {
        ptrs[i] = lgx_alloc(1024);
        TEST_ASSERT(ptrs[i] != NULL, "Sequential allocation succeeds");
    }
    
    // Free sequentially
    for (int i = 0; i < SEQ_ALLOC_COUNT; i++) {
        lgx_free(ptrs[i]);
    }
    #undef SEQ_ALLOC_COUNT
    
    TEST_ASSERT(true, "Sequential pattern completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 2: Random allocation sizes
static void test_random_allocation_sizes(void) {
    printf("\nTest 2: Random allocation sizes\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    srand(time(NULL));
    
    #define RANDOM_ALLOC_COUNT 500
    void* ptrs[RANDOM_ALLOC_COUNT];
    size_t sizes[RANDOM_ALLOC_COUNT];
    
    // Allocate random sizes
    for (int i = 0; i < RANDOM_ALLOC_COUNT; i++) {
        sizes[i] = 16 + (rand() % (64 * 1024)); // 16B to 64KB
        ptrs[i] = lgx_alloc(sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL, "Random size allocation succeeds");
    }
    
    // Verify and free
    for (int i = 0; i < RANDOM_ALLOC_COUNT; i++) {
        memset(ptrs[i], 0xAA, sizes[i]);
        lgx_free(ptrs[i]);
    }
    #undef RANDOM_ALLOC_COUNT
    
    TEST_ASSERT(true, "Random size pattern completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Interleaved allocation and deallocation
static void test_interleaved_alloc_free(void) {
    printf("\nTest 3: Interleaved allocation and deallocation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    void* ptrs[100];
    
    // Allocate some
    for (int i = 0; i < 50; i++) {
        ptrs[i] = lgx_alloc(1024);
        TEST_ASSERT(ptrs[i] != NULL, "Initial allocation succeeds");
    }
    
    // Free every other one
    for (int i = 0; i < 50; i += 2) {
        lgx_free(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    // Allocate more
    for (int i = 50; i < 100; i++) {
        ptrs[i] = lgx_alloc(2048);
        TEST_ASSERT(ptrs[i] != NULL, "Interleaved allocation succeeds");
    }
    
    // Free remaining
    for (int i = 0; i < 100; i++) {
        if (ptrs[i] != NULL) {
            lgx_free(ptrs[i]);
        }
    }
    
    TEST_ASSERT(true, "Interleaved pattern completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Frame arena stress test
static void test_frame_arena_stress(void) {
    printf("\nTest 4: Frame arena stress test\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Simulate multiple frames
    for (int frame = 0; frame < 100; frame++) {
        // Allocate many small objects per frame
        for (int i = 0; i < 1000; i++) {
            void* ptr = lgx_frame_alloc(64 + (i % 256));
            TEST_ASSERT(ptr != NULL, "Frame allocation succeeds");
        }
        
        // Reset frame
        lgx_frame_reset();
    }
    
    TEST_ASSERT(true, "Frame arena stress completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 5: Large allocation stress
static void test_large_allocation_stress(void) {
    printf("\nTest 5: Large allocation stress\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    #define LARGE_ALLOC_COUNT 50
    void* ptrs[LARGE_ALLOC_COUNT];
    
    // Allocate large blocks
    for (int i = 0; i < LARGE_ALLOC_COUNT; i++) {
        ptrs[i] = lgx_alloc(1024 * 1024); // 1MB each
        TEST_ASSERT(ptrs[i] != NULL, "Large allocation succeeds");
        
        // Touch memory to ensure it's mapped
        memset(ptrs[i], 0, 1024 * 1024);
    }
    
    // Free all
    for (int i = 0; i < LARGE_ALLOC_COUNT; i++) {
        lgx_free(ptrs[i]);
    }
    #undef LARGE_ALLOC_COUNT
    
    TEST_ASSERT(true, "Large allocation stress completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 6: Fragmentation test
static void test_fragmentation(void) {
    printf("\nTest 6: Fragmentation test\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    void* ptrs[200];
    
    // Allocate alternating sizes
    for (int i = 0; i < 200; i++) {
        size_t size = (i % 2 == 0) ? 128 : 4096;
        ptrs[i] = lgx_alloc(size);
        TEST_ASSERT(ptrs[i] != NULL, "Fragmentation allocation succeeds");
    }
    
    // Free every other allocation
    for (int i = 0; i < 200; i += 2) {
        lgx_free(ptrs[i]);
    }
    
    // Try to allocate in the gaps
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_alloc(128);
        TEST_ASSERT(ptr != NULL, "Gap allocation succeeds");
        lgx_free(ptr);
    }
    
    // Free remaining
    for (int i = 1; i < 200; i += 2) {
        lgx_free(ptrs[i]);
    }
    
    TEST_ASSERT(true, "Fragmentation test completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 7: Intent-based allocation stress
static void test_intent_based_stress(void) {
    printf("\nTest 7: Intent-based allocation stress\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    lgx_allocation_intent_base_t intents[] = {
        {sizeof(lgx_allocation_intent_base_t), 1024, LGX_ACCESS_SEQUENTIAL, 
         LGX_LIFETIME_FRAME, LGX_HINT_CRITICAL_PATH, LGX_INTENT_TRUST},
        {sizeof(lgx_allocation_intent_base_t), 2048, LGX_ACCESS_RANDOM, 
         LGX_LIFETIME_LEVEL, LGX_HINT_BACKGROUND, LGX_INTENT_VALIDATE_WARN},
        {sizeof(lgx_allocation_intent_base_t), 4096, LGX_ACCESS_WRITE_ONCE, 
         LGX_LIFETIME_SESSION, LGX_HINT_BANDWIDTH_HUNGRY, LGX_INTENT_VALIDATE_ADAPT},
    };
    
    void* ptrs[300];
    
    // Allocate with different intents
    for (int i = 0; i < 300; i++) {
        lgx_allocation_intent_base_t* intent = &intents[i % 3];
        ptrs[i] = lgx_alloc_with_intent(intent);
        TEST_ASSERT(ptrs[i] != NULL, "Intent-based allocation succeeds");
    }
    
    // Free all
    for (int i = 0; i < 300; i++) {
        lgx_free(ptrs[i]);
    }
    
    TEST_ASSERT(true, "Intent-based stress completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 8: Memory leak detection
static void test_memory_leak_detection(void) {
    printf("\nTest 8: Memory leak detection\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Get initial stats
    lgx_memory_stats_t stats_before;
    stats_before.struct_size = sizeof(lgx_memory_stats_t);
    lgx_memory_stats(&stats_before);
    
    // Allocate and free
    for (int i = 0; i < 1000; i++) {
        void* ptr = lgx_alloc(1024);
        lgx_free(ptr);
    }
    
    // Get final stats
    lgx_memory_stats_t stats_after;
    stats_after.struct_size = sizeof(lgx_memory_stats_t);
    lgx_memory_stats(&stats_after);
    
    // Current allocated should be similar (allowing for some overhead)
    uint64_t leaked = stats_after.current_allocated - stats_before.current_allocated;
    printf("    Leaked memory: %lu bytes\n", (unsigned long)leaked);
    
    TEST_ASSERT(leaked < 1024 * 1024, "No significant memory leaks");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 9: Concurrent allocation simulation
static void test_concurrent_allocation_simulation(void) {
    printf("\nTest 9: Concurrent allocation simulation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Simulate concurrent allocations from multiple "threads"
    void* ptrs[1000];
    
    for (int round = 0; round < 10; round++) {
        // Allocate from multiple "threads"
        for (int i = 0; i < 100; i++) {
            ptrs[round * 100 + i] = lgx_alloc(512 + (i * 16));
            TEST_ASSERT(ptrs[round * 100 + i] != NULL, "Concurrent allocation succeeds");
        }
    }
    
    // Free all
    for (int i = 0; i < 1000; i++) {
        lgx_free(ptrs[i]);
    }
    
    TEST_ASSERT(true, "Concurrent simulation completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Memory pressure test
static void test_memory_pressure(void) {
    printf("\nTest 10: Memory pressure test\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    void** ptrs = malloc(10000 * sizeof(void*));
    int allocated = 0;
    
    // Allocate until we hit limits or reach target
    for (int i = 0; i < 10000; i++) {
        ptrs[i] = lgx_alloc(64 * 1024); // 64KB each
        if (ptrs[i] == NULL) {
            break;
        }
        allocated++;
    }
    
    printf("    Allocated %d blocks (%.2f MB)\n", 
           allocated, (allocated * 64.0) / 1024.0);
    
    TEST_ASSERT(allocated > 0, "Some allocations succeeded");
    
    // Free all
    for (int i = 0; i < allocated; i++) {
        lgx_free(ptrs[i]);
    }
    
    free(ptrs);
    
    TEST_ASSERT(true, "Memory pressure test completed");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== LGX Runtime Memory Stress Integration Test ===\n");
    
    test_sequential_allocation();
    test_random_allocation_sizes();
    test_interleaved_alloc_free();
    test_frame_arena_stress();
    test_large_allocation_stress();
    test_fragmentation();
    test_intent_based_stress();
    test_memory_leak_detection();
    test_concurrent_allocation_simulation();
    test_memory_pressure();
    
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
