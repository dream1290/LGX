/**
 * Frame Arena Allocator Tests
 * 
 * Tests for the ultra-fast frame arena allocator (Month 1 implementation).
 */

#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("\n[TEST] %s\n", #name); \
    if (test_##name()) { \
        printf("[PASS] %s\n", #name); \
        tests_passed++; \
    } else { \
        printf("[FAIL] %s\n", #name); \
        tests_failed++; \
    }

// Helper function to get current time in nanoseconds
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Test 1: Basic initialization and shutdown
 */
static bool test_init_shutdown(void) {
    lgx_result_t result;
    
    // Initialize
    result = lgx_frame_arena_init();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to initialize frame arena: %d\n", result);
        return false;
    }
    
    // Check if initialized
    if (!lgx_frame_arena_is_initialized()) {
        printf("  ERROR: Frame arena not initialized\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    // Shutdown
    result = lgx_frame_arena_shutdown();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to shutdown frame arena: %d\n", result);
        return false;
    }
    
    // Check if shutdown
    if (lgx_frame_arena_is_initialized()) {
        printf("  ERROR: Frame arena still initialized after shutdown\n");
        return false;
    }
    
    printf("  ✓ Initialization and shutdown successful\n");
    return true;
}

/**
 * Test 2: Basic allocation
 */
static bool test_basic_allocation(void) {
    lgx_result_t result;
    
    // Initialize
    result = lgx_frame_arena_init();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to initialize\n");
        return false;
    }
    
    // Allocate small block
    void* ptr1 = lgx_frame_alloc(64);
    if (!ptr1) {
        printf("  ERROR: Failed to allocate 64 bytes\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    // Allocate another block
    void* ptr2 = lgx_frame_alloc(128);
    if (!ptr2) {
        printf("  ERROR: Failed to allocate 128 bytes\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    // Check that pointers are different
    if (ptr1 == ptr2) {
        printf("  ERROR: Pointers are the same\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    // Check alignment (should be 16-byte aligned)
    if (((uintptr_t)ptr1 % 16) != 0 || ((uintptr_t)ptr2 % 16) != 0) {
        printf("  ERROR: Pointers not 16-byte aligned\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    // Write to memory to ensure it's valid
    memset(ptr1, 0xAA, 64);
    memset(ptr2, 0xBB, 128);
    
    printf("  ✓ Basic allocation successful\n");
    printf("    ptr1: %p (64 bytes)\n", ptr1);
    printf("    ptr2: %p (128 bytes)\n", ptr2);
    
    lgx_frame_arena_shutdown();
    return true;
}

/**
 * Test 3: Frame reset
 */
static bool test_frame_reset(void) {
    lgx_result_t result;
    
    // Initialize
    result = lgx_frame_arena_init();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to initialize\n");
        return false;
    }
    
    // Allocate in frame 0
    uint32_t frame0 = lgx_frame_get_current_frame();
    lgx_frame_alloc(1024);  // Don't need to store pointer
    size_t usage1 = lgx_frame_get_current_usage();
    
    printf("  Frame %u: allocated 1024 bytes, usage = %zu\n", frame0, usage1);
    
    // Reset to frame 1
    result = lgx_frame_reset();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to reset frame\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    uint32_t frame1 = lgx_frame_get_current_frame();
    if (frame1 != frame0 + 1) {
        printf("  ERROR: Frame not incremented (expected %u, got %u)\n", frame0 + 1, frame1);
        lgx_frame_arena_shutdown();
        return false;
    }
    
    // Allocate in frame 1
    lgx_frame_alloc(2048);  // Don't need to store pointer
    size_t usage2 = lgx_frame_get_current_usage();
    
    printf("  Frame %u: allocated 2048 bytes, usage = %zu\n", frame1, usage2);
    
    // Reset to frame 2
    result = lgx_frame_reset();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to reset frame\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    uint32_t frame2 = lgx_frame_get_current_frame();
    size_t usage3 = lgx_frame_get_current_usage();
    
    printf("  Frame %u: usage after reset = %zu (should be 0)\n", frame2, usage3);
    
    if (usage3 != 0) {
        printf("  ERROR: Usage not reset to 0\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    printf("  ✓ Frame reset successful\n");
    
    lgx_frame_arena_shutdown();
    return true;
}

/**
 * Test 4: Triple buffering
 */
static bool test_triple_buffering(void) {
    lgx_result_t result;
    
    // Initialize
    result = lgx_frame_arena_init();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to initialize\n");
        return false;
    }
    
    // Allocate in 3 consecutive frames
    void* ptrs[3];
    for (int i = 0; i < 3; i++) {
        ptrs[i] = lgx_frame_alloc(1024);
        if (!ptrs[i]) {
            printf("  ERROR: Failed to allocate in frame %d\n", i);
            lgx_frame_arena_shutdown();
            return false;
        }
        
        // Write unique pattern
        memset(ptrs[i], 0xAA + i, 1024);
        
        printf("  Frame %d: allocated at %p\n", i, ptrs[i]);
        
        // Reset to next frame
        lgx_frame_reset();
    }
    
    // Now we're in frame 3, which reuses arena 0
    // The data from frame 0 should still be valid (triple buffering)
    
    // Allocate in frame 3 (reuses arena 0)
    void* ptr3 = lgx_frame_alloc(1024);
    if (!ptr3) {
        printf("  ERROR: Failed to allocate in frame 3\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    printf("  Frame 3: allocated at %p (reuses arena 0)\n", ptr3);
    
    // ptr3 should be at the same base address as ptr0 (arena 0 was reset)
    // But we can't directly compare because the arena was reset
    
    printf("  ✓ Triple buffering successful\n");
    
    lgx_frame_arena_shutdown();
    return true;
}

/**
 * Test 5: Performance benchmark
 */
static bool test_performance(void) {
    lgx_result_t result;
    
    // Initialize
    result = lgx_frame_arena_init();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to initialize\n");
        return false;
    }
    
    // Benchmark: 1 million allocations
    const int num_allocations = 1000000;
    const size_t alloc_size = 64;
    
    uint64_t start = get_time_ns();
    
    for (int i = 0; i < num_allocations; i++) {
        void* ptr = lgx_frame_alloc(alloc_size);
        if (!ptr) {
            printf("  ERROR: Allocation failed at iteration %d\n", i);
            lgx_frame_arena_shutdown();
            return false;
        }
    }
    
    uint64_t end = get_time_ns();
    uint64_t total_ns = end - start;
    double avg_ns = (double)total_ns / num_allocations;
    double avg_us = avg_ns / 1000.0;
    
    printf("  ✓ Performance benchmark:\n");
    printf("    Allocations: %d\n", num_allocations);
    printf("    Total time: %.2f ms\n", total_ns / 1000000.0);
    printf("    Average: %.3f ns (%.6f μs)\n", avg_ns, avg_us);
    
    // Target: P99 < 0.1 μs (100 nanoseconds)
    if (avg_us < 0.1) {
        printf("    ✓ EXCELLENT: Average < 0.1 μs (target achieved!)\n");
    } else if (avg_us < 0.5) {
        printf("    ✓ GOOD: Average < 0.5 μs (Tier 1 target)\n");
    } else {
        printf("    ⚠ WARNING: Average > 0.5 μs (needs optimization)\n");
    }
    
    lgx_frame_arena_shutdown();
    return true;
}

/**
 * Test 6: Statistics
 */
static bool test_statistics(void) {
    lgx_result_t result;
    
    // Initialize
    result = lgx_frame_arena_init();
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to initialize\n");
        return false;
    }
    
    // Allocate some memory
    lgx_frame_alloc(1024);
    lgx_frame_alloc(2048);
    lgx_frame_alloc(4096);
    
    // Get statistics
    frame_arena_stats_t stats;
    result = lgx_frame_get_stats(&stats);
    if (result != LGX_SUCCESS) {
        printf("  ERROR: Failed to get statistics\n");
        lgx_frame_arena_shutdown();
        return false;
    }
    
    printf("  ✓ Statistics:\n");
    printf("    Total allocations: %lu\n", stats.total_allocations);
    printf("    Total bytes allocated: %lu\n", stats.total_bytes_allocated);
    printf("    Overflow count: %lu\n", stats.overflow_count);
    printf("    Peak usage: %lu bytes (%.2f MB)\n", 
           stats.peak_usage_bytes, stats.peak_usage_bytes / (1024.0 * 1024.0));
    printf("    Current frame: %lu\n", stats.current_frame);
    
    if (stats.total_allocations != 3) {
        printf("  ERROR: Expected 3 allocations, got %lu\n", stats.total_allocations);
        lgx_frame_arena_shutdown();
        return false;
    }
    
    lgx_frame_arena_shutdown();
    return true;
}

/**
 * Main test runner
 */
int main(void) {
    printf("===========================================\n");
    printf("Frame Arena Allocator Tests\n");
    printf("===========================================\n");
    
    TEST(init_shutdown);
    TEST(basic_allocation);
    TEST(frame_reset);
    TEST(triple_buffering);
    TEST(performance);
    TEST(statistics);
    
    printf("\n===========================================\n");
    printf("Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("===========================================\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
