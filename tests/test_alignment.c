/**
 * Alignment Tests for lgx_arena.h
 * 
 * Tests memory alignment correctness
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#define LGX_ARENA_IMPLEMENTATION
#include "../lgx_arena.h"
#include "test_framework.h"

// Test: 16-byte alignment
int test_alignment_16(void) {
    TEST_BEGIN("16-byte alignment");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_frame_alloc(arena, 100, 16);
        TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
        TEST_ASSERT_EQ(((uintptr_t)ptr) % 16, 0, "Pointer should be 16-byte aligned");
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: 32-byte alignment
int test_alignment_32(void) {
    TEST_BEGIN("32-byte alignment");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_frame_alloc(arena, 100, 32);
        TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
        TEST_ASSERT_EQ(((uintptr_t)ptr) % 32, 0, "Pointer should be 32-byte aligned");
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: 64-byte alignment (cache line)
int test_alignment_64(void) {
    TEST_BEGIN("64-byte alignment (cache line)");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_frame_alloc(arena, 100, 64);
        TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
        TEST_ASSERT_EQ(((uintptr_t)ptr) % 64, 0, "Pointer should be 64-byte aligned");
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: 128-byte alignment
int test_alignment_128(void) {
    TEST_BEGIN("128-byte alignment");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_frame_alloc(arena, 100, 128);
        TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
        TEST_ASSERT_EQ(((uintptr_t)ptr) % 128, 0, "Pointer should be 128-byte aligned");
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: 256-byte alignment
int test_alignment_256(void) {
    TEST_BEGIN("256-byte alignment");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_frame_alloc(arena, 100, 256);
        TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
        TEST_ASSERT_EQ(((uintptr_t)ptr) % 256, 0, "Pointer should be 256-byte aligned");
    }
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Mixed alignments
int test_mixed_alignments(void) {
    TEST_BEGIN("Mixed alignments");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    void* ptr16 = lgx_frame_alloc(arena, 100, 16);
    void* ptr32 = lgx_frame_alloc(arena, 100, 32);
    void* ptr64 = lgx_frame_alloc(arena, 100, 64);
    void* ptr128 = lgx_frame_alloc(arena, 100, 128);
    
    TEST_ASSERT_EQ(((uintptr_t)ptr16) % 16, 0, "16-byte alignment");
    TEST_ASSERT_EQ(((uintptr_t)ptr32) % 32, 0, "32-byte alignment");
    TEST_ASSERT_EQ(((uintptr_t)ptr64) % 64, 0, "64-byte alignment");
    TEST_ASSERT_EQ(((uintptr_t)ptr128) % 128, 0, "128-byte alignment");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Alignment with various sizes
int test_alignment_various_sizes(void) {
    TEST_BEGIN("Alignment with various sizes");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Small sizes
    void* ptr1 = lgx_frame_alloc(arena, 1, 16);
    TEST_ASSERT_EQ(((uintptr_t)ptr1) % 16, 0, "1-byte allocation should be aligned");
    
    void* ptr7 = lgx_frame_alloc(arena, 7, 16);
    TEST_ASSERT_EQ(((uintptr_t)ptr7) % 16, 0, "7-byte allocation should be aligned");
    
    void* ptr15 = lgx_frame_alloc(arena, 15, 16);
    TEST_ASSERT_EQ(((uintptr_t)ptr15) % 16, 0, "15-byte allocation should be aligned");
    
    // Medium sizes
    void* ptr100 = lgx_frame_alloc(arena, 100, 32);
    TEST_ASSERT_EQ(((uintptr_t)ptr100) % 32, 0, "100-byte allocation should be aligned");
    
    void* ptr1000 = lgx_frame_alloc(arena, 1000, 64);
    TEST_ASSERT_EQ(((uintptr_t)ptr1000) % 64, 0, "1000-byte allocation should be aligned");
    
    // Large sizes
    void* ptr1mb = lgx_frame_alloc(arena, 1024 * 1024, 128);
    TEST_ASSERT_EQ(((uintptr_t)ptr1mb) % 128, 0, "1MB allocation should be aligned");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Alignment after reset
int test_alignment_after_reset(void) {
    TEST_BEGIN("Alignment after reset");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate with various alignments
    lgx_frame_alloc(arena, 100, 16);
    lgx_frame_alloc(arena, 100, 32);
    lgx_frame_alloc(arena, 100, 64);
    
    // Reset
    lgx_frame_reset(arena);
    
    // Allocate again - should still be properly aligned
    void* ptr16 = lgx_frame_alloc(arena, 100, 16);
    void* ptr32 = lgx_frame_alloc(arena, 100, 32);
    void* ptr64 = lgx_frame_alloc(arena, 100, 64);
    
    TEST_ASSERT_EQ(((uintptr_t)ptr16) % 16, 0, "16-byte alignment after reset");
    TEST_ASSERT_EQ(((uintptr_t)ptr32) % 32, 0, "32-byte alignment after reset");
    TEST_ASSERT_EQ(((uintptr_t)ptr64) % 64, 0, "64-byte alignment after reset");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Alignment with odd-sized allocations
int test_alignment_odd_sizes(void) {
    TEST_BEGIN("Alignment with odd-sized allocations");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // Allocate odd sizes, check next allocation is still aligned
    lgx_frame_alloc(arena, 13, 16);  // Odd size
    void* ptr1 = lgx_frame_alloc(arena, 100, 16);
    TEST_ASSERT_EQ(((uintptr_t)ptr1) % 16, 0, "Should be aligned after odd-sized allocation");
    
    lgx_frame_alloc(arena, 37, 32);  // Odd size
    void* ptr2 = lgx_frame_alloc(arena, 100, 32);
    TEST_ASSERT_EQ(((uintptr_t)ptr2) % 32, 0, "Should be aligned after odd-sized allocation");
    
    lgx_frame_alloc(arena, 99, 64);  // Odd size
    void* ptr3 = lgx_frame_alloc(arena, 100, 64);
    TEST_ASSERT_EQ(((uintptr_t)ptr3) % 64, 0, "Should be aligned after odd-sized allocation");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: Page alignment (4KB)
int test_page_alignment(void) {
    TEST_BEGIN("Page alignment (4KB)");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    void* ptr = lgx_frame_alloc(arena, 100, 4096);
    TEST_ASSERT_NOT_NULL(ptr, "Allocation should succeed");
    TEST_ASSERT_EQ(((uintptr_t)ptr) % 4096, 0, "Pointer should be page-aligned");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

// Test: SIMD alignment (16-byte for SSE, 32-byte for AVX)
int test_simd_alignment(void) {
    TEST_BEGIN("SIMD alignment");
    
    lgx_arena_t* arena = lgx_arena_create(64 * 1024 * 1024);
    TEST_ASSERT_NOT_NULL(arena, "Arena creation should succeed");
    
    // SSE requires 16-byte alignment
    float* sse_data = (float*)lgx_frame_alloc(arena, sizeof(float) * 4, 16);
    TEST_ASSERT_NOT_NULL(sse_data, "SSE allocation should succeed");
    TEST_ASSERT_EQ(((uintptr_t)sse_data) % 16, 0, "SSE data should be 16-byte aligned");
    
    // AVX requires 32-byte alignment
    float* avx_data = (float*)lgx_frame_alloc(arena, sizeof(float) * 8, 32);
    TEST_ASSERT_NOT_NULL(avx_data, "AVX allocation should succeed");
    TEST_ASSERT_EQ(((uintptr_t)avx_data) % 32, 0, "AVX data should be 32-byte aligned");
    
    lgx_arena_destroy(arena);
    
    TEST_END();
    return 0;
}

TEST_SUITE_BEGIN("Alignment")
    RUN_TEST(test_alignment_16);
    RUN_TEST(test_alignment_32);
    RUN_TEST(test_alignment_64);
    RUN_TEST(test_alignment_128);
    RUN_TEST(test_alignment_256);
    RUN_TEST(test_mixed_alignments);
    RUN_TEST(test_alignment_various_sizes);
    RUN_TEST(test_alignment_after_reset);
    RUN_TEST(test_alignment_odd_sizes);
    RUN_TEST(test_page_alignment);
    RUN_TEST(test_simd_alignment);
TEST_SUITE_END()
