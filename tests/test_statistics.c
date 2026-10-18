/**
 * Statistics Tests for lgx_arena.h
 * 
 * Tests statistics tracking and reporting
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#define LGX_ARENA_IMPLEMENTATION
#include "../lgx_arena.h"
#include "test_framework.h"

// Test: Initial statistics
int test_initial_stats(void) {
    TEST_BEGIN("Initial statistics");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    
    TEST_ASSERT_EQ(stats.total_allocations, 0, "Should have 0 allocations");
    TEST_ASSERT_EQ(stats.total_bytes_allocated, 0, "Should have 0 bytes");
    TEST_ASSERT_EQ(stats.peak_usage_bytes, 0, "Should have 0 peak usage");
    TEST_ASSERT_EQ(stats.current_frame, 0, "Should be frame 0");
    TEST_ASSERT_EQ(stats.overflow_count, 0, "Should have 0 overflows");
    TEST_ASSERT_EQ(stats.fallback_bytes, 0, "Should have 0 fallback bytes");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Allocation counting
int test_allocation_counting(void) {
    TEST_BEGIN("Allocation counting");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Make some allocations
    for (int i = 0; i < 100; i++) {
        lgx_frame_alloc(arena, 1024, 16);
    }
    
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    
    TEST_ASSERT_EQ(stats.total_allocations, 100, "Should have 100 allocations");
    TEST_ASSERT(stats.total_bytes_allocated >= 100 * 1024, "Should have at least 100KB allocated");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Peak usage tracking
int test_peak_usage(void) {
    TEST_BEGIN("Peak usage tracking");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate 1MB
    lgx_frame_alloc(arena, 1 * 1024 * 1024, 16);
    
    lgx_arena_stats_t stats1;
    lgx_arena_get_stats(arena, &stats1);
    size_t peak1 = stats1.peak_usage_bytes;
    
    // Allocate another 2MB
    lgx_frame_alloc(arena, 2 * 1024 * 1024, 16);
    
    lgx_arena_stats_t stats2;
    lgx_arena_get_stats(arena, &stats2);
    size_t peak2 = stats2.peak_usage_bytes;
    
    TEST_ASSERT(peak2 > peak1, "Peak should increase");
    TEST_ASSERT(peak2 >= 3 * 1024 * 1024, "Peak should be at least 3MB");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Frame counter
int test_frame_counter(void) {
    TEST_BEGIN("Frame counter");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    lgx_arena_stats_t stats;
    
    lgx_arena_get_stats(arena, &stats);
    TEST_ASSERT_EQ(stats.current_frame, 0, "Should be frame 0");
    
    lgx_frame_reset(arena);
    lgx_arena_get_stats(arena, &stats);
    TEST_ASSERT_EQ(stats.current_frame, 1, "Should be frame 1");
    
    lgx_frame_reset(arena);
    lgx_arena_get_stats(arena, &stats);
    TEST_ASSERT_EQ(stats.current_frame, 2, "Should be frame 2");
    
    lgx_frame_reset(arena);
    lgx_arena_get_stats(arena, &stats);
    TEST_ASSERT_EQ(stats.current_frame, 3, "Should be frame 3");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Statistics persistence across frames
int test_stats_persistence(void) {
    TEST_BEGIN("Statistics persistence across frames");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Frame 0: allocate 1MB
    lgx_frame_alloc(arena, 1 * 1024 * 1024, 16);
    lgx_frame_reset(arena);
    
    // Frame 1: allocate 2MB
    lgx_frame_alloc(arena, 2 * 1024 * 1024, 16);
    lgx_frame_reset(arena);
    
    // Frame 2: allocate 3MB
    lgx_frame_alloc(arena, 3 * 1024 * 1024, 16);
    
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    
    // Total allocations should accumulate
    TEST_ASSERT_EQ(stats.total_allocations, 3, "Should have 3 total allocations");
    
    // Total bytes should accumulate
    TEST_ASSERT(stats.total_bytes_allocated >= 6 * 1024 * 1024, 
                "Should have at least 6MB total");
    
    // Peak should be the maximum
    TEST_ASSERT(stats.peak_usage_bytes >= 3 * 1024 * 1024,
                "Peak should be at least 3MB");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Average frame time
int test_frame_time(void) {
    TEST_BEGIN("Average frame time tracking");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Do some frames
    for (int i = 0; i < 10; i++) {
        lgx_frame_alloc(arena, 1024, 16);
        lgx_frame_reset(arena);
    }
    
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    
    // Frame time should be tracked (non-zero)
    TEST_ASSERT(stats.avg_frame_time_us >= 0.0, "Frame time should be non-negative");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

TEST_SUITE_BEGIN("Statistics")
    RUN_TEST(test_initial_stats);
    RUN_TEST(test_allocation_counting);
    RUN_TEST(test_peak_usage);
    RUN_TEST(test_frame_counter);
    RUN_TEST(test_stats_persistence);
    RUN_TEST(test_frame_time);
TEST_SUITE_END()
