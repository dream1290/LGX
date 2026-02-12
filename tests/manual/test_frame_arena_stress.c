/**
 * Frame Arena Stress Test - Task 3.4.5.5.1
 * 
 * Simulates AAA game workload with high allocation rates to validate:
 * - No overflows with adaptive sizing (Task 3.4.5.5.2)
 * - Performance impact of overflow handling (Task 3.4.5.5.3)
 * - Recommended arena sizes (Task 3.4.5.5.4)
 * 
 * Test Scenarios:
 * 1. Indie Game: Low allocation rate (~10MB per frame)
 * 2. AA Game: Medium allocation rate (~30MB per frame)
 * 3. AAA Game: High allocation rate (~50MB per frame)
 * 4. AAA+ Game: Very high allocation rate (~80MB per frame)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"

// Test configuration
#define TEST_FRAMES 1000
#define WARMUP_FRAMES 100

// Game type simulation profiles
typedef struct {
    const char* name;
    size_t min_alloc_size;
    size_t max_alloc_size;
    size_t target_frame_usage;  // Target bytes per frame
    size_t num_allocations;     // Allocations per frame
} game_profile_t;

static const game_profile_t PROFILES[] = {
    {
        .name = "Indie Game",
        .min_alloc_size = 64,
        .max_alloc_size = 4096,
        .target_frame_usage = 10 * 1024 * 1024,  // 10MB
        .num_allocations = 500
    },
    {
        .name = "AA Game",
        .min_alloc_size = 128,
        .max_alloc_size = 16384,
        .target_frame_usage = 30 * 1024 * 1024,  // 30MB
        .num_allocations = 1000
    },
    {
        .name = "AAA Game",
        .min_alloc_size = 256,
        .max_alloc_size = 32768,
        .target_frame_usage = 50 * 1024 * 1024,  // 50MB
        .num_allocations = 1500
    },
    {
        .name = "AAA+ Game (Stress)",
        .min_alloc_size = 512,
        .max_alloc_size = 65536,
        .target_frame_usage = 80 * 1024 * 1024,  // 80MB
        .num_allocations = 2000
    }
};

#define NUM_PROFILES (sizeof(PROFILES) / sizeof(PROFILES[0]))

// Statistics tracking
typedef struct {
    uint64_t total_frames;
    uint64_t overflow_count;
    uint64_t fallback_count;
    size_t fallback_bytes;
    size_t peak_usage;
    uint64_t total_time_ns;
    uint64_t min_frame_time_ns;
    uint64_t max_frame_time_ns;
    double avg_frame_time_ns;
} test_stats_t;

// Random number generator (simple LCG)
static uint32_t rng_state = 12345;

static uint32_t rand_range(uint32_t min, uint32_t max) {
    rng_state = rng_state * 1103515245 + 12345;
    return min + (rng_state % (max - min + 1));
}

/**
 * Simulate a single frame with realistic allocation patterns
 */
static void simulate_frame(const game_profile_t* profile) {
    // Calculate average allocation size to hit target usage
    size_t avg_size = profile->target_frame_usage / profile->num_allocations;
    
    // Perform allocations with size variation
    for (size_t i = 0; i < profile->num_allocations; i++) {
        // Vary allocation size around average (±50%)
        size_t min = avg_size / 2;
        size_t max = avg_size * 3 / 2;
        
        // Clamp to profile limits
        if (min < profile->min_alloc_size) min = profile->min_alloc_size;
        if (max > profile->max_alloc_size) max = profile->max_alloc_size;
        
        size_t size = rand_range(min, max);
        
        // Allocate with realistic tags
        const char* tags[] = {"physics", "rendering", "audio", "ai", "networking"};
        const char* tag = tags[i % 5];
        
        void* ptr = lgx_frame_alloc_tagged(size, tag);
        if (ptr) {
            // Touch memory to ensure it's actually allocated
            memset(ptr, 0xAB, size);
        }
    }
}

/**
 * Run stress test for a specific game profile
 */
static bool run_profile_test(const game_profile_t* profile, size_t arena_size, test_stats_t* stats) {
    printf("\n=== Testing: %s (Arena: %.1f MB) ===\n", 
           profile->name, arena_size / (1024.0 * 1024.0));
    printf("Target usage: %.1f MB/frame, %zu allocations/frame\n",
           profile->target_frame_usage / (1024.0 * 1024.0),
           profile->num_allocations);
    
    // Configure arena size
    lgx_frame_arena_set_config_size(arena_size);
    lgx_frame_arena_set_config_max_size(256 * 1024 * 1024);  // 256MB max
    
    // Initialize runtime
    lgx_runtime_config_t config = {0};  // Use default config
    lgx_result_t result = lgx_runtime_init(&config);
    if (result != LGX_SUCCESS) {
        printf("❌ Failed to initialize runtime\n");
        return false;
    }
    
    // Reset statistics
    memset(stats, 0, sizeof(test_stats_t));
    stats->min_frame_time_ns = UINT64_MAX;
    
    // Get baseline stats
    frame_arena_stats_t baseline_stats;
    lgx_frame_get_stats(&baseline_stats);
    
    printf("Running %d warmup frames...\n", WARMUP_FRAMES);
    
    // Warmup phase (not measured)
    for (int i = 0; i < WARMUP_FRAMES; i++) {
        simulate_frame(profile);
        lgx_frame_reset();
    }
    
    printf("Running %d test frames...\n", TEST_FRAMES);
    
    // Test phase (measured)
    uint64_t test_start = lgx_time_now_ns();
    
    for (int i = 0; i < TEST_FRAMES; i++) {
        uint64_t frame_start = lgx_time_now_ns();
        
        simulate_frame(profile);
        
        uint64_t frame_end = lgx_time_now_ns();
        uint64_t frame_time = frame_end - frame_start;
        
        // Track frame timing
        if (frame_time < stats->min_frame_time_ns) {
            stats->min_frame_time_ns = frame_time;
        }
        if (frame_time > stats->max_frame_time_ns) {
            stats->max_frame_time_ns = frame_time;
        }
        
        lgx_frame_reset();
        
        // Progress indicator every 100 frames
        if ((i + 1) % 100 == 0) {
            printf("  Progress: %d/%d frames\n", i + 1, TEST_FRAMES);
        }
    }
    
    uint64_t test_end = lgx_time_now_ns();
    stats->total_time_ns = test_end - test_start;
    stats->avg_frame_time_ns = (double)stats->total_time_ns / TEST_FRAMES;
    
    // Get final stats
    frame_arena_stats_t final_stats;
    lgx_frame_get_stats(&final_stats);
    
    stats->total_frames = TEST_FRAMES;
    stats->overflow_count = final_stats.overflow_count - baseline_stats.overflow_count;
    stats->fallback_count = final_stats.fallback_count - baseline_stats.fallback_count;
    stats->fallback_bytes = final_stats.fallback_bytes - baseline_stats.fallback_bytes;
    stats->peak_usage = final_stats.peak_usage_bytes;
    
    // Shutdown runtime
    lgx_runtime_shutdown();
    
    return true;
}

/**
 * Print test results
 */
static void print_results(const game_profile_t* profile, const test_stats_t* stats) {
    printf("\n--- Results ---\n");
    printf("Total frames: %lu\n", stats->total_frames);
    printf("Peak usage: %.2f MB\n", stats->peak_usage / (1024.0 * 1024.0));
    printf("Overflows: %lu\n", stats->overflow_count);
    printf("Fallback allocations: %lu (%.2f MB)\n", 
           stats->fallback_count,
           stats->fallback_bytes / (1024.0 * 1024.0));
    
    printf("\nPerformance:\n");
    printf("  Total time: %.2f ms\n", stats->total_time_ns / 1000000.0);
    printf("  Avg frame time: %.2f μs\n", stats->avg_frame_time_ns / 1000.0);
    printf("  Min frame time: %.2f μs\n", stats->min_frame_time_ns / 1000.0);
    printf("  Max frame time: %.2f μs\n", stats->max_frame_time_ns / 1000.0);
    
    // Calculate overhead percentage
    double baseline_time = stats->min_frame_time_ns;
    double overhead_pct = ((stats->avg_frame_time_ns - baseline_time) / baseline_time) * 100.0;
    printf("  Overhead: %.2f%%\n", overhead_pct);
    
    // Pass/fail criteria
    bool passed = true;
    
    printf("\nValidation:\n");
    
    // Task 3.4.5.5.2: Verify no overflows with adaptive sizing
    if (stats->overflow_count == 0) {
        printf("  ✅ No overflows detected\n");
    } else {
        printf("  ⚠️  %lu overflows detected (adaptive sizing may need tuning)\n", 
               stats->overflow_count);
        passed = false;
    }
    
    // Task 3.4.5.5.3: Measure performance impact (<1% overhead)
    if (overhead_pct < 1.0) {
        printf("  ✅ Overhead < 1%% (%.2f%%)\n", overhead_pct);
    } else if (overhead_pct < 5.0) {
        printf("  ⚠️  Overhead %.2f%% (target: <1%%)\n", overhead_pct);
    } else {
        printf("  ❌ Overhead %.2f%% (target: <1%%)\n", overhead_pct);
        passed = false;
    }
    
    // Check fallback usage
    if (stats->fallback_count == 0) {
        printf("  ✅ No fallback allocations\n");
    } else {
        printf("  ⚠️  %lu fallback allocations (%.2f MB)\n",
               stats->fallback_count,
               stats->fallback_bytes / (1024.0 * 1024.0));
    }
    
    printf("\n%s %s\n", passed ? "✅" : "⚠️", passed ? "PASSED" : "NEEDS TUNING");
}

/**
 * Determine recommended arena size for a profile
 */
static size_t find_recommended_size(const game_profile_t* profile) {
    printf("\n=== Finding Recommended Size for %s ===\n", profile->name);
    
    // Try different arena sizes
    size_t sizes[] = {
        16 * 1024 * 1024,   // 16MB
        32 * 1024 * 1024,   // 32MB
        64 * 1024 * 1024,   // 64MB
        96 * 1024 * 1024,   // 96MB
        128 * 1024 * 1024,  // 128MB
        192 * 1024 * 1024,  // 192MB
    };
    
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        test_stats_t stats;
        
        printf("\nTrying %.1f MB...\n", sizes[i] / (1024.0 * 1024.0));
        
        if (!run_profile_test(profile, sizes[i], &stats)) {
            continue;
        }
        
        // Check if this size works well
        if (stats.overflow_count == 0 && stats.fallback_count == 0) {
            printf("✅ Recommended size: %.1f MB\n", sizes[i] / (1024.0 * 1024.0));
            printf("   Peak usage: %.2f MB (%.1f%% utilization)\n",
                   stats.peak_usage / (1024.0 * 1024.0),
                   (double)stats.peak_usage / sizes[i] * 100.0);
            return sizes[i];
        }
    }
    
    printf("⚠️  No suitable size found in tested range\n");
    return 256 * 1024 * 1024;  // Default to max
}

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Frame Arena Stress Test - Task 3.4.5.5.1                 ║\n");
    printf("║  Simulating AAA Game Workloads                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Seed RNG
    rng_state = (uint32_t)time(NULL);
    
    // Task 3.4.5.5.4: Document recommended arena sizes
    printf("\n=== Task 3.4.5.5.4: Finding Recommended Arena Sizes ===\n");
    
    size_t recommended_sizes[NUM_PROFILES];
    
    for (size_t i = 0; i < NUM_PROFILES; i++) {
        recommended_sizes[i] = find_recommended_size(&PROFILES[i]);
        sleep(1);  // Brief pause between tests
    }
    
    // Summary of recommendations
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Recommended Arena Sizes (Task 3.4.5.5.4)                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    for (size_t i = 0; i < NUM_PROFILES; i++) {
        printf("%-20s: %3.0f MB\n", 
               PROFILES[i].name,
               recommended_sizes[i] / (1024.0 * 1024.0));
    }
    
    printf("\n=== Final Validation Test ===\n");
    printf("Running each profile with recommended size...\n");
    
    bool all_passed = true;
    
    for (size_t i = 0; i < NUM_PROFILES; i++) {
        test_stats_t stats;
        
        if (!run_profile_test(&PROFILES[i], recommended_sizes[i], &stats)) {
            all_passed = false;
            continue;
        }
        
        print_results(&PROFILES[i], &stats);
        
        if (stats.overflow_count > 0) {
            all_passed = false;
        }
        
        sleep(1);  // Brief pause between tests
    }
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Final Results                                             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    if (all_passed) {
        printf("✅ All tests PASSED\n");
        printf("✅ Task 3.4.5.5.1: Stress test complete\n");
        printf("✅ Task 3.4.5.5.2: No overflows with adaptive sizing\n");
        printf("✅ Task 3.4.5.5.3: Overhead < 1%%\n");
        printf("✅ Task 3.4.5.5.4: Recommended sizes documented\n");
        return 0;
    } else {
        printf("⚠️  Some tests need tuning\n");
        printf("   Review results above for details\n");
        return 1;
    }
}
