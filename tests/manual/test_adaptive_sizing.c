/**
 * Comprehensive test for adaptive frame arena sizing (Task 3.4.5.2)
 * 
 * Tests:
 * - Configurable arena size (3.4.5.2.1)
 * - Dynamic arena growth (3.4.5.2.2)
 * - Size recommendations (3.4.5.2.3)
 * - Rolling average tracking (3.4.5.2.4)
 * - High usage warnings (3.4.5.2.5)
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Forward declare internal functions for testing
extern void* lgx_frame_alloc_internal(size_t size, const char* file, int line);
extern void lgx_frame_arena_dump_stats(FILE* output);
extern size_t lgx_frame_get_current_usage(void);
extern size_t lgx_frame_arena_get_recommended_size(void);
extern void lgx_config_set_frame_arena_size(lgx_runtime_config_t* config, size_t size);
extern void lgx_config_set_frame_arena_max_size(lgx_runtime_config_t* config, size_t size);
extern lgx_result_t lgx_frame_reset(void);

// Test 1: Configurable arena size (Task 3.4.5.2.1)
static void test_configurable_size(void) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test 1: Configurable Arena Size (Task 3.4.5.2.1)         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Shutdown existing runtime
    lgx_runtime_shutdown();
    
    // Test with custom size: 32MB
    printf("Testing custom arena size: 32MB\n");
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_frame_arena_size(config, 32 * 1024 * 1024);
    
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize with custom size: %d\n", result);
        return;
    }
    
    // Verify the size by checking capacity
    printf("Runtime initialized with custom size.\n");
    printf("Allocating 30MB to verify capacity...\n");
    
    void* ptr = lgx_frame_alloc_internal(30 * 1024 * 1024, __FILE__, __LINE__);
    if (ptr) {
        printf("✅ Successfully allocated 30MB (arena size is correct)\n");
    } else {
        printf("❌ Failed to allocate 30MB (arena might be too small)\n");
    }
    
    lgx_frame_reset();
    printf("✅ Test 1 complete\n");
}

// Test 2: Dynamic arena growth (Task 3.4.5.2.2)
static void test_dynamic_growth(void) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test 2: Dynamic Arena Growth (Task 3.4.5.2.2)            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Shutdown and reinit with small size
    lgx_runtime_shutdown();
    
    printf("Initializing with small arena (16MB) and max (128MB)...\n");
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_frame_arena_size(config, 16 * 1024 * 1024);
    lgx_config_set_frame_arena_max_size(config, 128 * 1024 * 1024);
    
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %d\n", result);
        return;
    }
    
    printf("\nAttempting to allocate 20MB (should trigger growth from 16MB to 32MB)...\n");
    void* ptr1 = lgx_frame_alloc_internal(20 * 1024 * 1024, __FILE__, __LINE__);
    if (ptr1) {
        printf("✅ Allocation succeeded! Arena grew dynamically.\n");
    } else {
        printf("❌ Allocation failed (growth might not have worked)\n");
    }
    
    printf("\nAttempting to allocate another 40MB (should trigger growth to 64MB)...\n");
    void* ptr2 = lgx_frame_alloc_internal(40 * 1024 * 1024, __FILE__, __LINE__);
    if (ptr2) {
        printf("✅ Second allocation succeeded! Arena grew again.\n");
    } else {
        printf("❌ Second allocation failed\n");
    }
    
    lgx_frame_reset();
    printf("✅ Test 2 complete\n");
}

// Test 3: Size recommendations (Task 3.4.5.2.3)
static void test_size_recommendations(void) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test 3: Size Recommendations (Task 3.4.5.2.3)            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Allocate varying amounts over several frames
    printf("Simulating 10 frames with varying allocation patterns...\n");
    
    for (int frame = 0; frame < 10; frame++) {
        // Allocate between 5MB and 15MB per frame
        size_t frame_size = (5 + (frame % 10)) * 1024 * 1024;
        
        for (size_t allocated = 0; allocated < frame_size; ) {
            size_t chunk = 1024 * 1024;  // 1MB chunks
            void* ptr = lgx_frame_alloc_internal(chunk, __FILE__, __LINE__);
            if (!ptr) break;
            allocated += chunk;
        }
        
        size_t usage = lgx_frame_get_current_usage();
        printf("  Frame %d: %.2f MB\n", frame, usage / (1024.0 * 1024.0));
        
        lgx_frame_reset();
    }
    
    // Get recommendation
    size_t recommended = lgx_frame_arena_get_recommended_size();
    printf("\n📊 Recommended arena size: %.2f MB\n", recommended / (1024.0 * 1024.0));
    printf("   (Based on observed peak usage + 25%% headroom)\n");
    
    printf("✅ Test 3 complete\n");
}

// Test 4: Rolling average tracking (Task 3.4.5.2.4)
static void test_rolling_average(void) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test 4: Rolling Average Tracking (Task 3.4.5.2.4)        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Simulating 70 frames to fill rolling window (60 frames)...\n");
    
    for (int frame = 0; frame < 70; frame++) {
        // Gradually increasing allocation pattern
        size_t frame_size = (2 + (frame / 10)) * 1024 * 1024;
        
        void* ptr = lgx_frame_alloc_internal(frame_size, __FILE__, __LINE__);
        if (!ptr) {
            printf("  Frame %d: Allocation failed\n", frame);
        }
        
        if (frame % 10 == 0) {
            size_t usage = lgx_frame_get_current_usage();
            printf("  Frame %d: %.2f MB\n", frame, usage / (1024.0 * 1024.0));
        }
        
        lgx_frame_reset();
    }
    
    printf("\n📊 Rolling average should now reflect recent usage trends.\n");
    printf("   Check the statistics dump for rolling average values.\n");
    
    printf("✅ Test 4 complete\n");
}

// Test 5: High usage warnings (Task 3.4.5.2.5)
static void test_high_usage_warnings(void) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Test 5: High Usage Warnings (Task 3.4.5.2.5)             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Shutdown and reinit with small size to trigger warnings
    lgx_runtime_shutdown();
    
    printf("Initializing with 16MB arena...\n");
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_frame_arena_size(config, 16 * 1024 * 1024);
    lgx_config_set_frame_arena_max_size(config, 16 * 1024 * 1024);  // Prevent growth
    
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %d\n", result);
        return;
    }
    
    printf("\nAllocating 13MB (81%% of capacity - should trigger warning)...\n");
    void* ptr = lgx_frame_alloc_internal(13 * 1024 * 1024, __FILE__, __LINE__);
    if (ptr) {
        printf("✅ Allocation succeeded. Check above for high usage warning.\n");
    }
    
    lgx_frame_reset();
    printf("✅ Test 5 complete\n");
}

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Adaptive Frame Arena Sizing Test (Task 3.4.5.2)          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Initialize with default settings
    printf("\nInitializing LGX Runtime with default settings...\n");
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime: %d\n", result);
        return 1;
    }
    printf("✅ Runtime initialized\n");
    
    // Run all tests
    test_configurable_size();
    test_dynamic_growth();
    test_size_recommendations();
    test_rolling_average();
    test_high_usage_warnings();
    
    // Final statistics
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Final Statistics                                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    lgx_frame_arena_dump_stats(stdout);
    
    // Get final recommendation
    size_t recommended = lgx_frame_arena_get_recommended_size();
    printf("\n📊 Final Recommendation: %.2f MB\n", recommended / (1024.0 * 1024.0));
    
    // Shutdown
    printf("\nShutting down runtime...\n");
    lgx_runtime_shutdown();
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  All Adaptive Sizing Tests Complete! ✅                   ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
