/**
 * Simple manual test for frame arena debugging features (Task 3.4.5.1)
 * 
 * This test uses only public API to verify the debugging features work.
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Forward declare internal functions for testing
extern void* lgx_frame_alloc_internal(size_t size, const char* file, int line);
extern void lgx_frame_arena_dump_stats(FILE* output);
extern size_t lgx_frame_get_current_usage(void);
extern uint32_t lgx_frame_get_current_frame(void);

int main(void) {
    printf("=== Frame Arena Debugging Test (Simple) ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime: %d\n", result);
        return 1;
    }
    
    printf("Runtime initialized successfully.\n\n");
    
    // Test 1: Normal allocations
    printf("Test 1: Normal allocations\n");
    for (int i = 0; i < 100; i++) {
        size_t size = 16 + (i % 10) * 64;
        void* ptr = lgx_alloc(size);  // Use public API
        if (!ptr) {
            fprintf(stderr, "Allocation failed at iteration %d\n", i);
            break;
        }
    }
    printf("Completed 100 allocations.\n\n");
    
    // Test 2: Check current usage
    printf("Test 2: Current usage\n");
    size_t usage = lgx_frame_get_current_usage();
    printf("Current frame arena usage: %zu bytes (%.2f MB)\n", usage, usage / (1024.0 * 1024.0));
    printf("\n");
    
    // Test 3: Overflow detection
    printf("Test 3: Overflow detection\n");
    printf("Attempting to allocate 70MB (arena capacity is 64MB)...\n");
    
    // This should trigger overflow warning
    void* large_ptr = lgx_alloc(70 * 1024 * 1024);
    if (large_ptr) {
        printf("Large allocation succeeded (fell back to persistent heap)\n");
        lgx_free(large_ptr);
    } else {
        printf("Large allocation failed\n");
    }
    printf("\n");
    
    // Test 4: Dump statistics
    printf("Test 4: Statistics dump\n");
    printf("Calling lgx_frame_arena_dump_stats()...\n");
    printf("----------------------------------------\n");
    lgx_frame_arena_dump_stats(stdout);
    printf("----------------------------------------\n\n");
    
    // Shutdown
    printf("Shutting down runtime...\n");
    lgx_runtime_shutdown();
    printf("Test completed successfully.\n");
    
    return 0;
}
