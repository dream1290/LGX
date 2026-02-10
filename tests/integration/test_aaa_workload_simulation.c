/*
 * AAA Game Workload Simulation Test
 * 
 * This test simulates realistic AAA game workloads to validate
 * the LGX Runtime Core can handle production-scale scenarios.
 */

#include <lgx_runtime.h>
#include <lgx_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

// Simulation parameters
#define FRAME_COUNT 1000
#define FRAME_ALLOCS_MIN 10000
#define FRAME_ALLOCS_MAX 50000
#define GPU_ALLOCS_PER_LEVEL 200
#define PERSISTENT_ALLOCS_PER_LEVEL 2000
#define FRAME_TIME_TARGET_MS 16.67  // 60 FPS

// Allocation size ranges (bytes)
#define FRAME_SIZE_MIN 16
#define FRAME_SIZE_MAX 65536
#define GPU_SIZE_MIN 262144      // 256 KB
#define GPU_SIZE_MAX 268435456   // 256 MB
#define PERSISTENT_SIZE_MIN 1024
#define PERSISTENT_SIZE_MAX 10485760  // 10 MB

// Test results
typedef struct {
    uint64_t total_frames;
    uint64_t total_allocations;
    double avg_frame_time_ms;
    double max_frame_time_ms;
    double p99_frame_time_ms;
    uint64_t allocation_failures;
    size_t peak_memory_mb;
    int passed;
} test_results_t;

// Random number generator (thread-safe)
static unsigned int g_seed = 12345;

static inline int rand_range(int min, int max) {
    return min + (rand_r(&g_seed) % (max - min + 1));
}


// Simulate a single frame of AAA game workload
static double simulate_frame(int frame_num) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Determine allocation count for this frame (varies based on scene complexity)
    int alloc_count = rand_range(FRAME_ALLOCS_MIN, FRAME_ALLOCS_MAX);
    
    // Allocate frame-scoped memory (80% of allocations)
    for (int i = 0; i < alloc_count; i++) {
        size_t size = rand_range(FRAME_SIZE_MIN, FRAME_SIZE_MAX);
        
        lgx_allocation_intent_base_t intent = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = size,
            .access_pattern = LGX_ACCESS_SEQUENTIAL,
            .lifetime = LGX_LIFETIME_FRAME,
            .hint = LGX_HINT_CRITICAL_PATH,
            .validation_policy = LGX_INTENT_TRUST
        };
        
        void* ptr = lgx_alloc_with_intent(&intent);
        if (!ptr) {
            fprintf(stderr, "Frame allocation failed: size=%zu\n", size);
            return -1.0;
        }
        
        // Simulate memory access (write pattern)
        if (size >= 64) {
            memset(ptr, 0xAA, 64);
        }
    }
    
    // Reset frame arena at frame boundary
    // (In real game, this would be called at end of frame)
    // lgx_frame_reset();  // Not exposed in public API yet
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double frame_time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                           (end.tv_nsec - start.tv_nsec) / 1000000.0;
    
    return frame_time_ms;
}


// Simulate level load (GPU + persistent allocations)
static int simulate_level_load(void) {
    printf("  Simulating level load...\n");
    
    // GPU allocations (15% of total)
    for (int i = 0; i < GPU_ALLOCS_PER_LEVEL; i++) {
        size_t size = rand_range(GPU_SIZE_MIN, GPU_SIZE_MAX);
        
        lgx_allocation_intent_base_t intent = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = size,
            .access_pattern = LGX_ACCESS_RANDOM,
            .lifetime = LGX_LIFETIME_LEVEL,
            .hint = LGX_HINT_GPU_SHARED,
            .validation_policy = LGX_INTENT_TRUST
        };
        
        void* ptr = lgx_alloc_with_intent(&intent);
        if (!ptr) {
            fprintf(stderr, "GPU allocation failed: size=%zu\n", size);
            return 0;
        }
    }
    
    // Persistent allocations (5% of total)
    for (int i = 0; i < PERSISTENT_ALLOCS_PER_LEVEL; i++) {
        size_t size = rand_range(PERSISTENT_SIZE_MIN, PERSISTENT_SIZE_MAX);
        
        lgx_allocation_intent_base_t intent = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = size,
            .access_pattern = LGX_ACCESS_RANDOM,
            .lifetime = LGX_LIFETIME_SESSION,
            .hint = LGX_HINT_BACKGROUND,
            .validation_policy = LGX_INTENT_TRUST
        };
        
        void* ptr = lgx_alloc_with_intent(&intent);
        if (!ptr) {
            fprintf(stderr, "Persistent allocation failed: size=%zu\n", size);
            return 0;
        }
    }
    
    printf("  Level load complete: %d GPU + %d persistent allocations\n",
           GPU_ALLOCS_PER_LEVEL, PERSISTENT_ALLOCS_PER_LEVEL);
    
    return 1;
}


// Main test function
int main(void) {
    printf("========================================\n");
    printf("AAA Game Workload Simulation Test\n");
    printf("========================================\n\n");
    
    // Initialize runtime
    printf("1. Initializing LGX Runtime...\n");
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime: %d\n", result);
        return 1;
    }
    printf("   ✓ Runtime initialized\n\n");
    
    // Simulate level load
    printf("2. Simulating level load...\n");
    if (!simulate_level_load()) {
        fprintf(stderr, "Level load simulation failed\n");
        lgx_runtime_shutdown();
        return 1;
    }
    printf("   ✓ Level loaded\n\n");
    
    // Simulate gameplay frames
    printf("3. Simulating %d frames of gameplay...\n", FRAME_COUNT);
    
    double frame_times[FRAME_COUNT];
    double total_time = 0.0;
    double max_time = 0.0;
    uint64_t total_allocs = 0;
    
    for (int i = 0; i < FRAME_COUNT; i++) {
        double frame_time = simulate_frame(i);
        
        if (frame_time < 0) {
            fprintf(stderr, "Frame %d simulation failed\n", i);
            lgx_runtime_shutdown();
            return 1;
        }
        
        frame_times[i] = frame_time;
        total_time += frame_time;
        if (frame_time > max_time) {
            max_time = frame_time;
        }
        
        int alloc_count = rand_range(FRAME_ALLOCS_MIN, FRAME_ALLOCS_MAX);
        total_allocs += alloc_count;
        
        // Progress indicator every 100 frames
        if ((i + 1) % 100 == 0) {
            printf("   Frame %d/%d (%.2f ms avg)\n", i + 1, FRAME_COUNT, total_time / (i + 1));
        }
    }
    
    printf("   ✓ Gameplay simulation complete\n\n");
    
    // Calculate statistics
    double avg_time = total_time / FRAME_COUNT;
    
    // Calculate P99
    // Sort frame times
    for (int i = 0; i < FRAME_COUNT - 1; i++) {
        for (int j = i + 1; j < FRAME_COUNT; j++) {
            if (frame_times[i] > frame_times[j]) {
                double temp = frame_times[i];
                frame_times[i] = frame_times[j];
                frame_times[j] = temp;
            }
        }
    }
    int p99_index = (int)(FRAME_COUNT * 0.99);
    double p99_time = frame_times[p99_index];
    
    // Print results
    printf("========================================\n");
    printf("Test Results\n");
    printf("========================================\n\n");
    
    printf("Frames Simulated:     %d\n", FRAME_COUNT);
    printf("Total Allocations:    %lu\n", total_allocs);
    printf("Avg Allocs/Frame:     %lu\n", total_allocs / FRAME_COUNT);
    printf("\n");
    
    printf("Frame Time Statistics:\n");
    printf("  Average:            %.3f ms\n", avg_time);
    printf("  Maximum:            %.3f ms\n", max_time);
    printf("  P99:                %.3f ms\n", p99_time);
    printf("  Target (60 FPS):    %.3f ms\n", FRAME_TIME_TARGET_MS);
    printf("\n");
    
    // Validate results
    int passed = 1;
    
    if (avg_time > FRAME_TIME_TARGET_MS) {
        printf("❌ FAIL: Average frame time exceeds target\n");
        passed = 0;
    } else {
        printf("✅ PASS: Average frame time within target\n");
    }
    
    if (p99_time > FRAME_TIME_TARGET_MS * 2.0) {
        printf("❌ FAIL: P99 frame time exceeds 2x target\n");
        passed = 0;
    } else {
        printf("✅ PASS: P99 frame time acceptable\n");
    }
    
    printf("\n");
    
    // Shutdown
    printf("4. Shutting down runtime...\n");
    lgx_runtime_shutdown();
    printf("   ✓ Runtime shutdown complete\n\n");
    
    if (passed) {
        printf("========================================\n");
        printf("✅ ALL TESTS PASSED\n");
        printf("========================================\n");
        return 0;
    } else {
        printf("========================================\n");
        printf("❌ SOME TESTS FAILED\n");
        printf("========================================\n");
        return 1;
    }
}
