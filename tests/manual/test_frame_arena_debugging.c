/**
 * Manual test for frame arena debugging features (Task 3.4.5.1)
 * 
 * This test demonstrates the new debugging capabilities:
 * - Detailed logging for overflow
 * - Allocation histogram
 * - Call site tracking
 * - Frame reset verification
 * - Memory leak detection
 */

#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Frame Arena Debugging Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime: %d\n", result);
        return 1;
    }
    
    printf("Runtime initialized successfully.\n\n");
    
    // Test 1: Normal allocations with histogram tracking
    printf("Test 1: Normal allocations (histogram tracking)\n");
    for (int i = 0; i < 100; i++) {
        size_t size = 16 + (i % 10) * 64;  // Varying sizes: 16, 80, 144, ...
        void* ptr = lgx_frame_alloc_internal(size, __FILE__, __LINE__);
        if (!ptr) {
            fprintf(stderr, "Allocation failed at iteration %d\n", i);
            break;
        }
    }
    printf("Completed 100 allocations with varying sizes.\n\n");
    
    // Test 2: Frame reset verification
    printf("Test 2: Frame reset verification\n");
    size_t usage_before = lgx_frame_get_current_usage();
    printf("Usage before reset: %zu bytes\n", usage_before);
    
    lgx_frame_reset();
    
    size_t usage_after = lgx_frame_get_current_usage();
    printf("Usage after reset: %zu bytes\n", usage_after);
    printf("Reset successful: %s\n\n", (usage_after == 0) ? "YES" : "NO");
    
    // Test 3: Multiple frames with allocations
    printf("Test 3: Multiple frames with allocations\n");
    for (int frame = 0; frame < 5; frame++) {
        printf("Frame %d: ", frame);
        
        // Allocate some memory
        for (int i = 0; i < 50; i++) {
            void* ptr = lgx_frame_alloc_internal(128, __FILE__, __LINE__);
            if (!ptr) {
                fprintf(stderr, "Allocation failed\n");
                break;
            }
        }
        
        size_t usage = lgx_frame_get_current_usage();
        printf("allocated %zu bytes\n", usage);
        
        // Reset for next frame
        lgx_frame_reset();
    }
    printf("\n");
    
    // Test 4: Overflow detection
    printf("Test 4: Overflow detection\n");
    printf("Attempting to allocate 70MB (arena capacity is 64MB)...\n");
    
    // This should trigger overflow and fallback to persistent heap
    void* large_ptr = lgx_frame_alloc_internal(70 * 1024 * 1024, __FILE__, __LINE__);
    if (large_ptr) {
        printf("Large allocation succeeded (fell back to persistent heap)\n");
        lgx_heap_free(large_ptr);  // Free the fallback allocation
    } else {
        printf("Large allocation failed\n");
    }
    printf("\n");
    
    // Test 5: Dump detailed statistics
    printf("Test 5: Detailed statistics dump\n");
    printf("----------------------------------------\n");
    lgx_frame_arena_dump_stats(stdout);
    printf("----------------------------------------\n\n");
    
    // Shutdown
    printf("Shutting down runtime...\n");
    lgx_runtime_shutdown();
    printf("Test completed successfully.\n");
    
    return 0;
}
