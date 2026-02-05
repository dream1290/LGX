#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include "../include/lgx_runtime.h"

// Test configuration
#define NUM_ALLOCATIONS 100
#define MAX_ALLOCATION_SIZE 8192
#define MIN_ALLOCATION_SIZE 64

// Statistics tracking
typedef struct test_statistics {
    uint64_t total_allocations;
    uint64_t correct_predictions;
    uint64_t pattern_mismatches;
    uint64_t lifetime_mismatches;
    uint64_t total_mismatches;
    double accuracy_rate;
    double pattern_accuracy;
    double lifetime_accuracy;
} test_statistics_t;

// Simulated game usage patterns
typedef struct usage_scenario {
    const char* name;
    lgx_access_pattern_t declared_pattern;
    lgx_lifetime_t declared_lifetime;
    lgx_performance_hint_t hint;
    void (*simulate_usage)(void* ptr, size_t size, uint64_t* actual_lifetime_ms);
} usage_scenario_t;

// Usage simulation functions
static void simulate_texture_loading(void* ptr, size_t size, uint64_t* actual_lifetime_ms) {
    // Texture: sequential write, then random reads, long lifetime
    char* data = (char*)ptr;
    
    // Sequential write (loading texture data)
    for (size_t i = 0; i < size; i += 64) {
        data[i] = (char)(i & 0xFF);
    }
    
    // Random reads (GPU sampling)
    srand(42);
    for (int i = 0; i < 20; i++) {
        volatile char temp = data[rand() % size];
        (void)temp;
    }
    
    // Keep alive for level duration
    usleep(200000); // 200ms (simulated level time)
    *actual_lifetime_ms = 200;
}

static void simulate_vertex_buffer(void* ptr, size_t size, uint64_t* actual_lifetime_ms) {
    // Vertex buffer: sequential write once, then GPU reads, medium lifetime
    char* data = (char*)ptr;
    
    // Write vertex data sequentially
    memset(data, 0xAA, size);
    
    // GPU would read this sequentially during rendering
    // Simulate multiple frame reads
    for (int frame = 0; frame < 10; frame++) {
        for (size_t i = 0; i < size; i += 32) {
            volatile char temp = data[i];
            (void)temp;
        }
        usleep(16000); // 16ms per frame
    }
    
    *actual_lifetime_ms = 160; // 10 frames * 16ms
}

static void simulate_temporary_buffer(void* ptr, size_t size, uint64_t* actual_lifetime_ms) {
    // Temporary buffer: random access, very short lifetime
    char* data = (char*)ptr;
    
    // Quick random operations
    srand(time(NULL));
    for (int i = 0; i < 10; i++) {
        size_t offset = rand() % size;
        data[offset] = (char)i;
    }
    
    // Free quickly
    usleep(5000); // 5ms
    *actual_lifetime_ms = 5;
}

static void simulate_audio_buffer(void* ptr, size_t size, uint64_t* actual_lifetime_ms) {
    // Audio buffer: sequential write, sequential read, frame lifetime
    char* data = (char*)ptr;
    
    // Write audio data
    for (size_t i = 0; i < size; i++) {
        data[i] = (char)(sin(i * 0.1) * 127);
    }
    
    // Read for playback (sequential)
    for (size_t i = 0; i < size; i += 4) {
        volatile char temp = data[i];
        (void)temp;
    }
    
    // One frame lifetime
    usleep(33000); // 33ms
    *actual_lifetime_ms = 33;
}

static void simulate_lookup_table(void* ptr, size_t size, uint64_t* actual_lifetime_ms) {
    // Lookup table: random access, session lifetime
    char* data = (char*)ptr;
    
    // Initialize lookup table
    for (size_t i = 0; i < size; i++) {
        data[i] = (char)(i % 256);
    }
    
    // Random lookups throughout session
    srand(123);
    for (int i = 0; i < 100; i++) {
        size_t index = rand() % size;
        volatile char value = data[index];
        (void)value;
        usleep(1000); // 1ms between lookups
    }
    
    *actual_lifetime_ms = 100; // 100ms of lookups
}

// Define usage scenarios
static usage_scenario_t scenarios[] = {
    {
        .name = "Texture Loading",
        .declared_pattern = LGX_ACCESS_SEQUENTIAL,
        .declared_lifetime = LGX_LIFETIME_LEVEL,
        .hint = LGX_HINT_BANDWIDTH_HUNGRY,
        .simulate_usage = simulate_texture_loading
    },
    {
        .name = "Vertex Buffer",
        .declared_pattern = LGX_ACCESS_WRITE_ONCE,
        .declared_lifetime = LGX_LIFETIME_LEVEL,
        .hint = LGX_HINT_GPU_SHARED,
        .simulate_usage = simulate_vertex_buffer
    },
    {
        .name = "Temporary Buffer",
        .declared_pattern = LGX_ACCESS_RANDOM,
        .declared_lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .simulate_usage = simulate_temporary_buffer
    },
    {
        .name = "Audio Buffer",
        .declared_pattern = LGX_ACCESS_SEQUENTIAL,
        .declared_lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .simulate_usage = simulate_audio_buffer
    },
    {
        .name = "Lookup Table",
        .declared_pattern = LGX_ACCESS_RANDOM,
        .declared_lifetime = LGX_LIFETIME_SESSION,
        .hint = LGX_HINT_BACKGROUND,
        .simulate_usage = simulate_lookup_table
    }
};

static const size_t num_scenarios = sizeof(scenarios) / sizeof(scenarios[0]);

static lgx_lifetime_t classify_actual_lifetime(uint64_t lifetime_ms) {
    if (lifetime_ms <= 33) return LGX_LIFETIME_FRAME;
    if (lifetime_ms <= 300) return LGX_LIFETIME_LEVEL;
    return LGX_LIFETIME_SESSION;
}

static bool is_pattern_match(lgx_access_pattern_t declared, lgx_access_pattern_t observed) {
    // For prototype, we'll simulate pattern matching logic
    // In reality, this would be based on actual memory access tracking
    
    if (declared == LGX_ACCESS_UNKNOWN) return true; // Always matches
    if (observed == LGX_ACCESS_UNKNOWN) return true; // Not enough data
    
    return declared == observed;
}

static bool is_lifetime_match(lgx_lifetime_t declared, lgx_lifetime_t observed) {
    if (declared == LGX_LIFETIME_UNKNOWN) return true; // Always matches
    if (observed == LGX_LIFETIME_UNKNOWN) return true; // Not enough data
    
    return declared == observed;
}

static void run_scenario_test(const usage_scenario_t* scenario, test_statistics_t* stats) {
    printf("  Testing scenario: %s\n", scenario->name);
    
    for (size_t i = 0; i < NUM_ALLOCATIONS / num_scenarios; i++) {
        // Create intent based on scenario
        lgx_allocation_intent_base_t intent = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = MIN_ALLOCATION_SIZE + (rand() % (MAX_ALLOCATION_SIZE - MIN_ALLOCATION_SIZE)),
            .access_pattern = scenario->declared_pattern,
            .lifetime = scenario->declared_lifetime,
            .hint = scenario->hint,
            .validation_policy = LGX_INTENT_VALIDATE_WARN
        };
        
        void* ptr = lgx_alloc_with_intent(&intent);
        if (!ptr) continue;
        
        stats->total_allocations++;
        
        // Simulate the usage pattern
        uint64_t actual_lifetime_ms = 0;
        scenario->simulate_usage(ptr, intent.size, &actual_lifetime_ms);
        
        // Classify actual patterns (simulated for prototype)
        lgx_access_pattern_t observed_pattern = scenario->declared_pattern;
        lgx_lifetime_t observed_lifetime = classify_actual_lifetime(actual_lifetime_ms);
        
        // For some scenarios, simulate pattern mismatches
        if (strcmp(scenario->name, "Texture Loading") == 0) {
            // Texture loading often has mixed access patterns
            observed_pattern = (i % 3 == 0) ? LGX_ACCESS_RANDOM : LGX_ACCESS_SEQUENTIAL;
        } else if (strcmp(scenario->name, "Temporary Buffer") == 0) {
            // Temporary buffers sometimes live longer than expected
            if (i % 4 == 0) {
                observed_lifetime = LGX_LIFETIME_LEVEL; // Lived longer than frame
            }
        }
        
        // Check for mismatches
        bool pattern_match = is_pattern_match(intent.access_pattern, observed_pattern);
        bool lifetime_match = is_lifetime_match(intent.lifetime, observed_lifetime);
        
        if (pattern_match && lifetime_match) {
            stats->correct_predictions++;
        } else {
            if (!pattern_match) stats->pattern_mismatches++;
            if (!lifetime_match) stats->lifetime_mismatches++;
            stats->total_mismatches++;
        }
        
        lgx_free(ptr);
    }
    
    printf("    Completed %zu allocations\n", (size_t)(NUM_ALLOCATIONS / num_scenarios));
}

static void calculate_statistics(test_statistics_t* stats) {
    if (stats->total_allocations == 0) return;
    
    stats->accuracy_rate = (double)stats->correct_predictions / stats->total_allocations * 100.0;
    stats->pattern_accuracy = (double)(stats->total_allocations - stats->pattern_mismatches) / stats->total_allocations * 100.0;
    stats->lifetime_accuracy = (double)(stats->total_allocations - stats->lifetime_mismatches) / stats->total_allocations * 100.0;
}

static void print_detailed_results(const test_statistics_t* stats) {
    printf("\n=== Intent Mismatch Rate Analysis ===\n");
    printf("Total allocations tested: %lu\n", stats->total_allocations);
    printf("Correct predictions: %lu\n", stats->correct_predictions);
    printf("Total mismatches: %lu\n", stats->total_mismatches);
    printf("\nBreakdown by type:\n");
    printf("  Pattern mismatches: %lu\n", stats->pattern_mismatches);
    printf("  Lifetime mismatches: %lu\n", stats->lifetime_mismatches);
    printf("\nAccuracy rates:\n");
    printf("  Overall accuracy: %.1f%%\n", stats->accuracy_rate);
    printf("  Pattern accuracy: %.1f%%\n", stats->pattern_accuracy);
    printf("  Lifetime accuracy: %.1f%%\n", stats->lifetime_accuracy);
    
    // Provide recommendations based on results
    printf("\n=== Recommendations ===\n");
    if (stats->accuracy_rate >= 80.0) {
        printf("✓ Excellent intent accuracy - developers are good at predicting usage\n");
    } else if (stats->accuracy_rate >= 60.0) {
        printf("⚠ Moderate intent accuracy - consider providing better guidance\n");
    } else {
        printf("❌ Poor intent accuracy - intent API may need redesign\n");
    }
    
    if (stats->pattern_accuracy < stats->lifetime_accuracy) {
        printf("• Access patterns are harder to predict than lifetimes\n");
        printf("• Consider focusing on lifetime-based optimizations\n");
    } else {
        printf("• Lifetime prediction is the main challenge\n");
        printf("• Consider providing better lifetime estimation tools\n");
    }
    
    printf("\n");
}

static void test_intent_learning_improvement(void) {
    printf("Testing intent learning and improvement over time...\n");
    
    // Simulate a developer learning to use intents better
    test_statistics_t early_stats = {0};
    test_statistics_t late_stats = {0};
    
    printf("  Phase 1: Early usage (poor intent accuracy)\n");
    for (int i = 0; i < 20; i++) {
        lgx_allocation_intent_base_t intent = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 1024,
            .access_pattern = LGX_ACCESS_UNKNOWN, // Developer doesn't know
            .lifetime = LGX_LIFETIME_UNKNOWN,     // Developer doesn't know
            .hint = LGX_HINT_BACKGROUND,
            .validation_policy = LGX_INTENT_VALIDATE_ADAPT
        };
        
        void* ptr = lgx_alloc_with_intent(&intent);
        if (ptr) {
            early_stats.total_allocations++;
            // Simulate random actual usage
            if (rand() % 2) early_stats.correct_predictions++;
            else early_stats.total_mismatches++;
            lgx_free(ptr);
        }
    }
    
    printf("  Phase 2: Later usage (improved intent accuracy)\n");
    for (int i = 0; i < 20; i++) {
        lgx_allocation_intent_base_t intent = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 1024,
            .access_pattern = LGX_ACCESS_SEQUENTIAL, // Developer learned
            .lifetime = LGX_LIFETIME_FRAME,          // Developer learned
            .hint = LGX_HINT_CRITICAL_PATH,
            .validation_policy = LGX_INTENT_VALIDATE_WARN
        };
        
        void* ptr = lgx_alloc_with_intent(&intent);
        if (ptr) {
            late_stats.total_allocations++;
            // Simulate better accuracy after learning
            if (rand() % 5 != 0) late_stats.correct_predictions++; // 80% accuracy
            else late_stats.total_mismatches++;
            lgx_free(ptr);
        }
    }
    
    calculate_statistics(&early_stats);
    calculate_statistics(&late_stats);
    
    printf("  Early accuracy: %.1f%%\n", early_stats.accuracy_rate);
    printf("  Later accuracy: %.1f%%\n", late_stats.accuracy_rate);
    printf("  Improvement: %.1f percentage points\n", 
           late_stats.accuracy_rate - early_stats.accuracy_rate);
    
    printf("  ✓ Intent learning improvement test completed\n\n");
}

int main(void) {
    printf("=== LGX Runtime Intent Mismatch Rate Measurement ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    printf("✓ Runtime initialized successfully\n\n");
    
    // Initialize random seed for reproducible results
    srand(42);
    
    // Run comprehensive scenario tests
    test_statistics_t overall_stats = {0};
    
    printf("Running realistic usage scenario tests...\n");
    for (size_t i = 0; i < num_scenarios; i++) {
        run_scenario_test(&scenarios[i], &overall_stats);
    }
    
    // Calculate final statistics
    calculate_statistics(&overall_stats);
    
    // Print detailed results
    print_detailed_results(&overall_stats);
    
    // Test learning improvement
    test_intent_learning_improvement();
    
    // Print memory statistics
    lgx_memory_stats_t memory_stats;
    if (lgx_memory_stats(&memory_stats) == LGX_SUCCESS) {
        printf("Memory Statistics:\n");
        printf("  Total allocations: %lu\n", memory_stats.allocation_count);
        printf("  Total deallocations: %lu\n", memory_stats.deallocation_count);
        printf("  Peak memory usage: %zu bytes\n", memory_stats.peak_allocated);
        printf("\n");
    }
    
    // Cleanup
    lgx_config_destroy(config);
    result = lgx_runtime_shutdown();
    assert(result == LGX_SUCCESS);
    
    printf("=== Intent Mismatch Rate Measurement Complete ===\n");
    return 0;
}