/**
 * Basic Functionality Tests for lgx_arena.h
 * 
 * Tests core allocation, reset, and statistics
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#define LGX_ARENA_IMPLEMENTATION
#include "../lgx_arena.h"
#include "test_framework.h"

// Test: Arena creation and destruction
int test_create_destroy(void) {
    TEST_BEGIN("Arena creation and destruction");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Basic allocation
int test_basic_allocation(void) {
    TEST_BEGIN("Basic allocation");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    void* ptr = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    
    // Verify we can write to the memory
    memset(ptr, 0xAB, 1024);
    TEST_ASSERT_EQ(((uint8_t*)ptr)[0], 0xAB, "Memory should be writable");
    TEST_ASSERT_EQ(((uint8_t*)ptr)[1023], 0xAB, "Memory should be writable");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Multiple allocations
int test_multiple_allocations(void) {
    TEST_BEGIN("Multiple allocations");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    void* ptr1 = lgx_frame_alloc(arena, 256, 16);
    void* ptr2 = lgx_frame_alloc(arena, 512, 16);
    void* ptr3 = lgx_frame_alloc(arena, 1024, 16);
    
    TEST_ASSERT_NOT_NULL(ptr1, "First allocation should succeed");
    TEST_ASSERT_NOT_NULL(ptr2, "Second allocation should succeed");
    TEST_ASSERT_NOT_NULL(ptr3, "Third allocation should succeed");
    
    // Pointers should be different
    TEST_ASSERT_NE(ptr1, ptr2, "Pointers should be different");
    TEST_ASSERT_NE(ptr2, ptr3, "Pointers should be different");
    TEST_ASSERT_NE(ptr1, ptr3, "Pointers should be different");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Frame reset
int test_frame_reset(void) {
    TEST_BEGIN("Frame reset");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate some memory
    void* ptr1 = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr1, "Allocation should succeed");
    
    // Get stats before reset
    lgx_arena_stats_t stats_before;
    lgx_arena_get_stats(arena, &stats_before);
    TEST_ASSERT_EQ(stats_before.current_frame, 0, "Should be frame 0");
    
    // Reset frame
    lgx_frame_reset(arena);
    
    // Get stats after reset
    lgx_arena_stats_t stats_after;
    lgx_arena_get_stats(arena, &stats_after);
    TEST_ASSERT_EQ(stats_after.current_frame, 1, "Should be frame 1");
    
    // Allocate again (should reuse memory after triple-buffering)
    void* ptr2 = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr2, "Allocation after reset should succeed");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Statistics tracking
int test_statistics(void) {
    TEST_BEGIN("Statistics tracking");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Initial stats
    lgx_arena_stats_t stats;
    lgx_arena_get_stats(arena, &stats);
    TEST_ASSERT_EQ(stats.total_allocations, 0, "Should have 0 allocations");
    TEST_ASSERT_EQ(stats.total_bytes_allocated, 0, "Should have 0 bytes allocated");
    
    // Allocate some memory
    lgx_frame_alloc(arena, 1024, 16);
    lgx_frame_alloc(arena, 2048, 16);
    
    // Check stats
    lgx_arena_get_stats(arena, &stats);
    TEST_ASSERT_EQ(stats.total_allocations, 2, "Should have 2 allocations");
    TEST_ASSERT(stats.total_bytes_allocated >= 3072, "Should have at least 3072 bytes allocated");
    TEST_ASSERT(stats.peak_usage_bytes >= 3072, "Peak usage should be at least 3072 bytes");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Alignment
int test_alignment(void) {
    TEST_BEGIN("Alignment");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Test various alignments
    void* ptr16 = lgx_frame_alloc(arena, 100, 16);
    TEST_ASSERT_NOT_NULL(ptr16, "16-byte aligned allocation should succeed");
    TEST_ASSERT_EQ(((uintptr_t)ptr16) % 16, 0, "Pointer should be 16-byte aligned");
    
    void* ptr32 = lgx_frame_alloc(arena, 100, 32);
    TEST_ASSERT_NOT_NULL(ptr32, "32-byte aligned allocation should succeed");
    TEST_ASSERT_EQ(((uintptr_t)ptr32) % 32, 0, "Pointer should be 32-byte aligned");
    
    void* ptr64 = lgx_frame_alloc(arena, 100, 64);
    TEST_ASSERT_NOT_NULL(ptr64, "64-byte aligned allocation should succeed");
    TEST_ASSERT_EQ(((uintptr_t)ptr64) % 64, 0, "Pointer should be 64-byte aligned");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Triple buffering
int test_triple_buffering(void) {
    TEST_BEGIN("Triple buffering");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate in frame 0
    void* ptr0 = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr0, "Frame 0 allocation should succeed");
    memset(ptr0, 0xAA, 1024);
    
    // Move to frame 1
    lgx_frame_reset(arena);
    void* ptr1 = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr1, "Frame 1 allocation should succeed");
    memset(ptr1, 0xBB, 1024);
    
    // Move to frame 2
    lgx_frame_reset(arena);
    void* ptr2 = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr2, "Frame 2 allocation should succeed");
    memset(ptr2, 0xCC, 1024);
    
    // All three pointers should still be valid (triple-buffering)
    TEST_ASSERT_EQ(((uint8_t*)ptr0)[0], 0xAA, "Frame 0 data should still be valid");
    TEST_ASSERT_EQ(((uint8_t*)ptr1)[0], 0xBB, "Frame 1 data should still be valid");
    TEST_ASSERT_EQ(((uint8_t*)ptr2)[0], 0xCC, "Frame 2 data should still be valid");
    
    // Move to frame 3 (should reuse frame 0's arena)
    lgx_frame_reset(arena);
    void* ptr3 = lgx_frame_alloc(arena, 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr3, "Frame 3 allocation should succeed");
    
    // Frame 1 and 2 should still be valid
    TEST_ASSERT_EQ(((uint8_t*)ptr1)[0], 0xBB, "Frame 1 data should still be valid");
    TEST_ASSERT_EQ(((uint8_t*)ptr2)[0], 0xCC, "Frame 2 data should still be valid");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Zero-size allocation
int test_zero_size(void) {
    TEST_BEGIN("Zero-size allocation");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    void* ptr = lgx_frame_alloc(arena, 0, 16);
    TEST_ASSERT_NULL(ptr, "Zero-size allocation should return NULL");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: NULL arena
int test_null_arena(void) {
    TEST_BEGIN("NULL arena handling");
    
    void* ptr = lgx_frame_alloc(NULL, 1024, 16);
    TEST_ASSERT_NULL(ptr, "Allocation with NULL arena should return NULL");
    
    lgx_frame_reset(NULL);  // Should not crash
    
    lgx_arena_get_stats(NULL, NULL);  // Should not crash
    
    lgx_arena_destroy(NULL);  // Should not crash
    
    TEST_END();
    return 0;
}

// Test: Large allocation
int test_large_allocation(void) {
    TEST_BEGIN("Large allocation");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate 32MB (half the arena)
    void* ptr = lgx_frame_alloc(arena, 32 * 1024 * 1024, 16);
    TEST_ASSERT_NOT_NULL(ptr, "Large allocation should succeed");
    
    // Verify we can write to it
    memset(ptr, 0x42, 1024);  // Just write first 1KB to avoid slowdown
    TEST_ASSERT_EQ(((uint8_t*)ptr)[0], 0x42, "Memory should be writable");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

TEST_SUITE_BEGIN("Basic Functionality")
    RUN_TEST(test_create_destroy);
    RUN_TEST(test_basic_allocation);
    RUN_TEST(test_multiple_allocations);
    RUN_TEST(test_frame_reset);
    RUN_TEST(test_statistics);
    RUN_TEST(test_alignment);
    RUN_TEST(test_triple_buffering);
    RUN_TEST(test_zero_size);
    RUN_TEST(test_null_arena);
    RUN_TEST(test_large_allocation);
TEST_SUITE_END()
