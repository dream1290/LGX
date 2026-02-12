/**
 * Frame Arena Overflow Diagnosis Tool (Task 3.4.5.1)
 * 
 * This test helps diagnose why the frame arena is overflowing.
 * It checks:
 * 1. If lgx_frame_reset() is being called
 * 2. Allocation patterns and sizes
 * 3. Memory leaks (allocations not being reset)
 * 4. Peak usage per frame
 */

#include "../../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// External function to dump stats (declared in lgx_runtime_internal.h)
extern void lgx_frame_arena_dump_stats(FILE* output);

int main(void) {
    printf("=== Frame Arena Overflow Diagnosis ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    
    printf("Task 3.4.5.1.4: Verify lgx_frame_reset() is being called\n");
    printf("-------------------------------------------------------\n\n");
    
    // Test 1: Simulate frames WITHOUT reset (reproduces the bug)
    printf("Test 1: Allocating without frame reset (SHOULD OVERFLOW)\n");
    size_t total_allocated = 0;
    int allocation_count = 0;
    
    while (total_allocated < 64 * 1024 * 1024) {  // 64MB
        void* ptr = lgx_alloc_frame(1024);  // 1KB allocations
        if (!ptr) {
            printf("  ERROR: Allocation failed at %zu MB\n", total_allocated / (1024 * 1024));
            break;
        }
        total_allocated += 1024;
        allocation_count++;
        
        // Print progress every 10MB
        if (total_allocated % (10 * 1024 * 1024) == 0) {
            printf("  Allocated: %zu MB (%d allocations)\n", 
                   total_allocated / (1024 * 1024), allocation_count);
        }
    }
    
    printf("  Final: %zu MB allocated (%d allocations)\n", 
           total_allocated / (1024 * 1024), allocation_count);
    printf("  Result: Arena should be full or overflowing\n\n");
    
    // Dump statistics
    printf("Statistics after overflow:\n");
    lgx_frame_arena_dump_stats(stdout);
    printf("\n");
    
    // Test 2: Reset and verify arena is cleared
    printf("Test 2: Reset frame and verify arena is cleared\n");
    result = lgx_frame_reset();
    assert(result == LGX_SUCCESS);
    
    size_t current_usage = lgx_frame_get_current_usage();
    printf("  Current usage after reset: %zu bytes\n", current_usage);
    
    if (current_usage == 0) {
        printf("  ✅ PASS: Arena was properly reset\n\n");
    } else {
        printf("  ❌ FAIL: Arena was NOT reset (memory leak!)\n\n");
    }
    
    // Test 3: Simulate proper frame loop WITH resets
    printf("Test 3: Proper frame loop with resets (SHOULD NOT OVERFLOW)\n");
    const int num_frames = 100;
    const int allocs_per_frame = 1000;
    
    for (int frame = 0; frame < num_frames; frame++) {
        // Allocate for this frame
        for (int i = 0; i < allocs_per_frame; i++) {
            size_t size = (i % 10 == 0) ? 4096 : ((i % 3 == 0) ? 512 : 64);
            void* ptr = lgx_alloc_frame(size);
            assert(ptr != NULL);
        }
        
        // Reset at frame boundary (THIS IS CRITICAL!)
        result = lgx_frame_reset();
        assert(result == LGX_SUCCESS);
        
        if (frame % 10 == 0) {
            size_t usage = lgx_frame_get_current_usage();
            printf("  Frame %d: usage after reset = %zu bytes\n", frame, usage);
        }
    }
    
    printf("  ✅ PASS: Completed %d frames without overflow\n\n", num_frames);
    
    // Test 4: Check peak usage
    printf("Test 4: Peak usage analysis\n");
    size_t peak_usage = lgx_frame_get_peak_usage();
    printf("  Peak usage across all frames: %zu bytes (%.2f MB, %.1f%% of 64MB)\n",
           peak_usage,
           peak_usage / (1024.0 * 1024.0),
           (peak_usage * 100.0) / (64 * 1024 * 1024));
    
    if (peak_usage > 64 * 1024 * 1024 * 0.8) {
        printf("  ⚠️  WARNING: Peak usage > 80%% of capacity\n");
        printf("  Recommendation: Increase arena size to %zu MB\n",
               (peak_usage * 2) / (1024 * 1024));
    } else {
        printf("  ✅ OK: Peak usage is within safe limits\n");
    }
    printf("\n");
    
    // Final statistics
    printf("Final Statistics:\n");
    lgx_frame_arena_dump_stats(stdout);
    
    // Diagnosis summary
    printf("\n=== Diagnosis Summary ===\n");
    printf("Root Cause Analysis:\n");
    printf("1. The overflow occurs when lgx_frame_reset() is NOT called\n");
    printf("2. Frame arena is designed for per-frame allocations\n");
    printf("3. Without reset, arena fills up and never clears\n");
    printf("4. Solution: Call lgx_frame_reset() at frame boundaries\n\n");
    
    printf("Affected Test: tests/performance/test_frame_time_contribution.c\n");
    printf("Fix: Add lgx_frame_reset() call after each frame simulation\n\n");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return 0;
}
