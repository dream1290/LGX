/**
 * Overflow Handling Tests for lgx_arena.h
 * 
 * Tests arena overflow detection and fallback behavior
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#define LGX_ARENA_IMPLEMENTATION
#include "../lgx_arena.h"
#include "test_framework.h"

// Test: Arena overflow with fallback
int test_overflow_fallback(void) {
    TEST_BEGIN("Arena overflow with fallback");
    
    // Create arena at minimum size (16MB)
    lgx_arena_t* arena = lgx_arena_create(16 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate most of the arena
    void* ptr1 = lgx_frame_alloc(arena, 15 * 1024 * 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr1, "First allocation should succeed");
    
    // This should overflow
    void* ptr2 = lgx_frame_alloc(arena, 2 * 1024 * 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr2, "Overflow allocation should succeed");
    
    // Check statistics
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    
    // If we overflowed, free the malloc'd pointer
    if (stats.overflow_count > 0) {
        TEST_ASSERT(stats.fallback_bytes > 0, "Should have fallback bytes > 0");
        free(ptr2);
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Multiple overflows
int test_multiple_overflows(void) {
    TEST_BEGIN("Multiple overflows");
    
    lgx_arena_t* arena = lgx_arena_create(16 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Fill the arena
    lgx_frame_alloc(arena, 15 * 1024 * 1024, 16);
    
    // Trigger multiple overflows
    void* ptr1 = lgx_frame_alloc(arena, 1 * 1024 * 1024, 16);
    void* ptr2 = lgx_frame_alloc(arena, 1 * 1024 * 1024, 16);
    void* ptr3 = lgx_frame_alloc(arena, 1 * 1024 * 1024, 16);
    
    TEST_ASSERT_NOT_NULL(ptr1, "First overflow should succeed");
    TEST_ASSERT_NOT_NULL(ptr2, "Second overflow should succeed");
    TEST_ASSERT_NOT_NULL(ptr3, "Third overflow should succeed");
    
    // Check statistics
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    
    // Clean up if overflowed
    if (stats.overflow_count >= 1) free(ptr1);
    if (stats.overflow_count >= 2) free(ptr2);
    if (stats.overflow_count >= 3) free(ptr3);
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Overflow recovery after reset
int test_overflow_recovery(void) {
    TEST_BEGIN("Overflow recovery after reset");
    
    lgx_arena_t* arena = lgx_arena_create(16 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Fill arena and overflow
    lgx_frame_alloc(arena, 15 * 1024 * 1024, 16);
    void* overflow_ptr = lgx_frame_alloc(arena, 2 * 1024 * 1024, 16);
    TEST_ASSERT_NOT_NULL(overflow_ptr, "Overflow should succeed");
    
    lgx_arena_stats_t stats_before;
    lgx_arena_get_stats(arena, &stats_before);
    
    // Reset frame
    lgx_frame_reset(arena);
    
    // After reset, we should be able to allocate normally again
    void* ptr = lgx_frame_alloc(arena, 8 * 1024 * 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation after reset should succeed");
    
    // Cleanup overflow allocation if it happened
    if (stats_before.overflow_count > 0) {
        free(overflow_ptr);
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Exact capacity allocation
int test_exact_capacity(void) {
    TEST_BEGIN("Exact capacity allocation");
    
    size_t capacity = 16 * 1024 * 1024;  // 16MB
    lgx_arena_t* arena = lgx_arena_create(capacity);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate almost the entire arena (leave room for alignment)
    void* ptr = lgx_frame_alloc(arena, capacity - 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr, "Large allocation should succeed");
    
    // Small allocation should still work
    void* ptr2 = lgx_frame_alloc(arena, 512, 16);
    TEST_ASSERT_NOT_NULL(ptr2, "Small allocation should succeed");
    
    // This should overflow
    void* ptr3 = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr3, "Overflow allocation should fallback");
    
    free(ptr3);
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Overflow with different sizes
int test_overflow_various_sizes(void) {
    TEST_BEGIN("Overflow with various sizes");
    
    lgx_arena_t* arena = lgx_arena_create(16 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Fill arena
    lgx_frame_alloc(arena, 15 * 1024 * 1024, 16);
    
    // Try various overflow sizes
    void* ptr_small = lgx_frame_alloc(arena, 64, 16);
    void* ptr_medium = lgx_frame_alloc(arena, 4096, 16);
    void* ptr_large = lgx_frame_alloc(arena, 1024 * 1024, 16);
    
    TEST_ASSERT_NOT_NULL(ptr_small, "Small overflow should succeed");
    TEST_ASSERT_NOT_NULL(ptr_medium, "Medium overflow should succeed");
    TEST_ASSERT_NOT_NULL(ptr_large, "Large overflow should succeed");
    
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    
    // Cleanup if overflowed
    if (stats.overflow_count > 0) {
        free(ptr_small);
        free(ptr_medium);
        free(ptr_large);
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Peak usage tracking with overflow
int test_peak_usage_with_overflow(void) {
    TEST_BEGIN("Peak usage tracking with overflow");
    
    lgx_arena_t* arena = lgx_arena_create(1 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate some memory
    lgx_frame_alloc(arena, 512 * 1024, 16);
    
    lgx_arena_stats_t stats1;
    lgx_arena_get_stats(arena, &stats1);
    size_t peak1 = stats1.peak_usage_bytes;
    
    // Allocate more
    lgx_frame_alloc(arena, 512 * 1024, 16);
    
    lgx_arena_stats_t stats2;
    lgx_arena_get_stats(arena, &stats2);
    size_t peak2 = stats2.peak_usage_bytes;
    
    TEST_ASSERT(peak2 > peak1, "Peak usage should increase");
    
    // Overflow
    void* overflow_ptr = lgx_frame_alloc(arena, 256 * 1024, 16);
    TEST_ASSERT_NOT_NULL(overflow_ptr, "Overflow should succeed");
    
    // Peak should still be tracked
    lgx_arena_stats_t stats3;
    lgx_arena_get_stats(arena, &stats3);
    TEST_ASSERT(stats3.peak_usage_bytes >= peak2, "Peak should not decrease");
    
    free(overflow_ptr);
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Minimum arena size
int test_minimum_arena_size(void) {
    TEST_BEGIN("Minimum arena size");
    
    // Try to create arena smaller than minimum (should be clamped)
    lgx_arena_t* arena = lgx_arena_create(1024);  // 1KB (too small)
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed (clamped to minimum)");
    
    // Should be able to allocate at least 1MB (minimum size)
    void* ptr = lgx_frame_alloc(arena, 1 * 1024 * 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr, "Should be able to allocate 1MB");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Maximum arena size
int test_maximum_arena_size(void) {
    TEST_BEGIN("Maximum arena size");
    
    // Try to create arena larger than maximum (should be clamped)
    lgx_arena_t* arena = lgx_arena_create(1024ULL * 1024 * 1024);  // 1GB (too large)
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed (clamped to maximum)");
    
    // Should be able to allocate up to 256MB (maximum size)
    void* ptr = lgx_frame_alloc(arena, 128 * 1024 * 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr, "Should be able to allocate 128MB");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

TEST_SUITE_BEGIN("Overflow Handling")
    RUN_TEST(test_overflow_fallback);
    RUN_TEST(test_multiple_overflows);
    RUN_TEST(test_overflow_recovery);
    RUN_TEST(test_exact_capacity);
    RUN_TEST(test_overflow_various_sizes);
    RUN_TEST(test_peak_usage_with_overflow);
    RUN_TEST(test_minimum_arena_size);
    RUN_TEST(test_maximum_arena_size);
TEST_SUITE_END()
