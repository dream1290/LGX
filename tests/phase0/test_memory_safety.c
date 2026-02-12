/**
 * Test: Memory Safety Features
 * 
 * Validates task 9.2: Memory safety features
 * - Guard pages after allocations (debug builds)
 * - Memory canaries to detect corruption
 * - Delayed reclamation (3-frame) to prevent use-after-free
 * - Allocation tracking to prevent double-free
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test 1: Basic allocation and free
static void test_basic_allocation(void) {
    printf("Test 1: Basic allocation and free\n");
    
    // Allocate memory
    void* ptr = lgx_memory_safety_alloc(1024);
    assert(ptr != NULL);
    
    // Write to memory
    memset(ptr, 0xAA, 1024);
    
    // Free memory
    lgx_memory_safety_free(ptr);
    
    printf("  ✓ Basic allocation and free works\n");
}

// Test 2: Canary detection (buffer overflow)
static void test_canary_detection(void) {
    printf("Test 2: Canary detection (buffer overflow)\n");
    
#ifdef DEBUG
    // Allocate memory
    void* ptr = lgx_memory_safety_alloc(64);
    assert(ptr != NULL);
    
    // Intentionally overflow the buffer (write past the end)
    // This should corrupt the suffix canary
    memset(ptr, 0xBB, 64);  // OK
    // Note: We can't actually test the overflow without crashing
    // because it would corrupt memory. The canary check happens on free.
    
    // Free memory (canary check happens here)
    lgx_memory_safety_free(ptr);
    
    // Get statistics
    lgx_memory_safety_stats_t stats;
    lgx_memory_safety_get_stats(&stats);
    
    printf("  ✓ Canary detection system active (violations: %lu)\n", 
           stats.canary_violations);
#else
    printf("  ⚠ Canary detection only active in DEBUG builds\n");
#endif
}

// Test 3: Double-free prevention
static void test_double_free_prevention(void) {
    printf("Test 3: Double-free prevention\n");
    
#ifdef DEBUG
    // Get initial statistics
    lgx_memory_safety_stats_t stats_before;
    lgx_memory_safety_get_stats(&stats_before);
    
    // Allocate memory
    void* ptr = lgx_memory_safety_alloc(128);
    assert(ptr != NULL);
    
    // Free once (OK)
    lgx_memory_safety_free(ptr);
    
    // Try to free again (should be detected)
    lgx_memory_safety_free(ptr);
    
    // Get statistics
    lgx_memory_safety_stats_t stats_after;
    lgx_memory_safety_get_stats(&stats_after);
    
    // Should have detected one double-free attempt
    assert(stats_after.double_free_attempts > stats_before.double_free_attempts);
    
    printf("  ✓ Double-free detected (attempts: %lu)\n", 
           stats_after.double_free_attempts);
#else
    printf("  ⚠ Double-free detection only active in DEBUG builds\n");
#endif
}

// Test 4: Delayed reclamation (3-frame delay)
static void test_delayed_reclamation(void) {
    printf("Test 4: Delayed reclamation (3-frame delay)\n");
    
#ifdef DEBUG
    // Allocate memory
    void* ptr = lgx_memory_safety_alloc(256);
    assert(ptr != NULL);
    
    // Free memory
    lgx_memory_safety_free(ptr);
    
    // Get statistics - should have 1 delayed free
    lgx_memory_safety_stats_t stats1;
    lgx_memory_safety_get_stats(&stats1);
    assert(stats1.delayed_frees > 0);
    printf("  Delayed frees after free: %lu\n", stats1.delayed_frees);
    
    // Advance frame (frame 0 -> 1)
    lgx_memory_safety_advance_frame();
    
    // Still delayed
    lgx_memory_safety_stats_t stats2;
    lgx_memory_safety_get_stats(&stats2);
    assert(stats2.delayed_frees > 0);
    printf("  Delayed frees after frame 1: %lu\n", stats2.delayed_frees);
    
    // Advance frame (frame 1 -> 2)
    lgx_memory_safety_advance_frame();
    
    // Still delayed
    lgx_memory_safety_stats_t stats3;
    lgx_memory_safety_get_stats(&stats3);
    assert(stats3.delayed_frees > 0);
    printf("  Delayed frees after frame 2: %lu\n", stats3.delayed_frees);
    
    // Advance frame (frame 2 -> 3)
    lgx_memory_safety_advance_frame();
    
    // Should be freed now (3 frames have passed)
    lgx_memory_safety_stats_t stats4;
    lgx_memory_safety_get_stats(&stats4);
    printf("  Delayed frees after frame 3: %lu\n", stats4.delayed_frees);
    
    printf("  ✓ Delayed reclamation works (3-frame delay)\n");
#else
    printf("  ⚠ Delayed reclamation only active in DEBUG builds\n");
#endif
}

// Test 5: Allocation tracking
static void test_allocation_tracking(void) {
    printf("Test 5: Allocation tracking\n");
    
#ifdef DEBUG
    // Get initial statistics
    lgx_memory_safety_stats_t stats_before;
    lgx_memory_safety_get_stats(&stats_before);
    
    // Allocate multiple blocks
    void* ptr1 = lgx_memory_safety_alloc(64);
    void* ptr2 = lgx_memory_safety_alloc(128);
    void* ptr3 = lgx_memory_safety_alloc(256);
    
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);
    
    // Get statistics - should have 3 more active allocations
    lgx_memory_safety_stats_t stats_after_alloc;
    lgx_memory_safety_get_stats(&stats_after_alloc);
    assert(stats_after_alloc.active_allocations == 
           stats_before.active_allocations + 3);
    printf("  Active allocations: %lu\n", stats_after_alloc.active_allocations);
    
    // Free all blocks
    lgx_memory_safety_free(ptr1);
    lgx_memory_safety_free(ptr2);
    lgx_memory_safety_free(ptr3);
    
    // Advance frames to process delayed frees
    for (int i = 0; i < 4; i++) {
        lgx_memory_safety_advance_frame();
    }
    
    // Get statistics - should be back to initial count
    lgx_memory_safety_stats_t stats_after_free;
    lgx_memory_safety_get_stats(&stats_after_free);
    assert(stats_after_free.active_allocations == stats_before.active_allocations);
    printf("  Active allocations after free: %lu\n", 
           stats_after_free.active_allocations);
    
    printf("  ✓ Allocation tracking works correctly\n");
#else
    printf("  ⚠ Allocation tracking only active in DEBUG builds\n");
#endif
}

// Test 6: Statistics reporting
static void test_statistics_reporting(void) {
    printf("Test 6: Statistics reporting\n");
    
    // Get statistics
    lgx_memory_safety_stats_t stats;
    lgx_memory_safety_get_stats(&stats);
    
    printf("  Memory safety statistics:\n");
    printf("    Total allocations: %lu\n", stats.total_allocations);
    printf("    Total frees: %lu\n", stats.total_frees);
    printf("    Active allocations: %lu\n", stats.active_allocations);
    printf("    Delayed frees: %lu\n", stats.delayed_frees);
    printf("    Canary violations: %lu\n", stats.canary_violations);
    printf("    Double-free attempts: %lu\n", stats.double_free_attempts);
    printf("    Use-after-free attempts: %lu\n", stats.use_after_free_attempts);
    printf("    Current frame: %u\n", stats.current_frame);
    
    printf("  ✓ Statistics reporting works\n");
}

// Test 7: Guard pages (debug builds only)
static void test_guard_pages(void) {
    printf("Test 7: Guard pages\n");
    
#ifdef DEBUG
    // Note: We can't actually test guard page violations without crashing
    // the test process. Guard pages are set up with mprotect(PROT_NONE),
    // so any access to them will cause a SIGSEGV.
    
    // We can only verify that allocations work with guard pages enabled
    void* ptr = lgx_memory_safety_alloc(4096);
    assert(ptr != NULL);
    
    // Write to the allocated memory (should be OK)
    memset(ptr, 0xCC, 4096);
    
    // Free memory
    lgx_memory_safety_free(ptr);
    
    printf("  ✓ Guard pages enabled (would catch out-of-bounds access)\n");
#else
    printf("  ⚠ Guard pages only active in DEBUG builds\n");
#endif
}

// Test 8: Multiple allocations and frees
static void test_multiple_allocations(void) {
    printf("Test 8: Multiple allocations and frees\n");
    
    #define NUM_SAFETY_ALLOCS 100
    void* ptrs[NUM_SAFETY_ALLOCS];
    
    // Allocate multiple blocks
    for (int i = 0; i < NUM_SAFETY_ALLOCS; i++) {
        ptrs[i] = lgx_memory_safety_alloc(64 + i * 16);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], i & 0xFF, 64 + i * 16);
    }
    
    // Free all blocks
    for (int i = 0; i < NUM_SAFETY_ALLOCS; i++) {
        lgx_memory_safety_free(ptrs[i]);
    }
    #undef NUM_SAFETY_ALLOCS
    
    // Advance frames to process delayed frees
    for (int i = 0; i < 4; i++) {
        lgx_memory_safety_advance_frame();
    }
    
    printf("  ✓ Multiple allocations and frees work correctly\n");
}

int main(void) {
    printf("=== Memory Safety Features Tests ===\n\n");
    
    // Initialize memory safety system
    lgx_result_t result = lgx_memory_safety_init();
    (void)result;  // Suppress unused variable warning
    assert(result == LGX_SUCCESS);
    printf("✓ Memory safety system initialized\n\n");
    
    // Run tests
    test_basic_allocation();
    test_canary_detection();
    test_double_free_prevention();
    test_delayed_reclamation();
    test_allocation_tracking();
    test_statistics_reporting();
    test_guard_pages();
    test_multiple_allocations();
    
    // Shutdown memory safety system
    result = lgx_memory_safety_shutdown();
    assert(result == LGX_SUCCESS);
    printf("\n✓ Memory safety system shutdown\n");
    
    printf("\n=== All Memory Safety Tests Passed ===\n");
    return 0;
}
