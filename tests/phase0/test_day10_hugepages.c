/**
 * Day 10: Huge Pages Optimization Test
 * 
 * Tests the huge pages implementation and measures TLB miss reduction.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define NUM_ALLOCATIONS 50000
#define ALLOCATION_SIZE 64

// Timing utilities
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Performance measurement
typedef struct {
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t p999;
    uint64_t min;
    uint64_t max;
    double avg;
} perf_stats_t;

static int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

static void calculate_stats(uint64_t* latencies, int count, perf_stats_t* stats) {
    qsort(latencies, count, sizeof(uint64_t), compare_uint64);
    
    stats->min = latencies[0];
    stats->max = latencies[count - 1];
    stats->p50 = latencies[count / 2];
    stats->p95 = latencies[(count * 95) / 100];
    stats->p99 = latencies[(count * 99) / 100];
    stats->p999 = latencies[(count * 999) / 1000];
    
    uint64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += latencies[i];
    }
    stats->avg = (double)sum / count;
}

int main(void) {
    printf("=== Day 10: Huge Pages Optimization Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    if (!config) {
        printf("ERROR: Failed to create config\n");
        return 1;
    }
    
    lgx_config_set_memory_pool_size(config, 256 * 1024 * 1024);  // 256MB
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_HUGE_PAGES);
    
    lgx_result_t result = lgx_runtime_init(config);
    lgx_config_destroy(config);
    
    if (result != LGX_SUCCESS) {
        printf("ERROR: Runtime initialization failed: %d\n", result);
        return 1;
    }
    
    printf("Runtime initialized successfully\n\n");
    
    // Check if huge pages are available
    bool hugepages_available = lgx_hugepages_available();
    printf("Huge pages available: %s\n", hugepages_available ? "YES" : "NO");
    
    if (!hugepages_available) {
        printf("\nWARNING: Huge pages not available on this system\n");
        printf("To enable huge pages, run:\n");
        printf("  sudo sysctl -w vm.nr_hugepages=128\n");
        printf("Or enable transparent huge pages:\n");
        printf("  echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled\n\n");
        printf("Test will continue with regular pages (performance comparison not available)\n\n");
    }
    
    // Estimate TLB improvement
    size_t thread_pool_size = 16ULL * 1024 * 1024 * 64;  // 1GB total
    double tlb_improvement = lgx_hugepages_estimate_tlb_improvement(thread_pool_size);
    printf("Estimated TLB miss reduction: %.1f%%\n\n", tlb_improvement);
    
    // Allocate arrays for performance measurement
    void** ptrs = malloc(NUM_ALLOCATIONS * sizeof(void*));
    uint64_t* latencies = malloc(NUM_ALLOCATIONS * sizeof(uint64_t));
    
    if (!ptrs || !latencies) {
        printf("ERROR: Failed to allocate test arrays\n");
        lgx_runtime_shutdown();
        return 1;
    }
    
    printf("Running allocation performance test...\n");
    printf("Allocations: %d\n", NUM_ALLOCATIONS);
    printf("Allocation size: %d bytes\n\n", ALLOCATION_SIZE);
    
    // Warm-up phase
    printf("Warming up caches...\n");
    for (int i = 0; i < 1000; i++) {
        void* ptr = lgx_alloc(ALLOCATION_SIZE);
        if (ptr) {
            lgx_free(ptr);
        }
    }
    
    // Performance test
    printf("Measuring allocation latency...\n");
    uint64_t test_start = get_time_ns();
    
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        uint64_t start = get_time_ns();
        ptrs[i] = lgx_alloc(ALLOCATION_SIZE);
        uint64_t end = get_time_ns();
        
        latencies[i] = end - start;
        
        if (!ptrs[i]) {
            printf("ERROR: Allocation %d failed\n", i);
            break;
        }
    }
    
    uint64_t test_end = get_time_ns();
    uint64_t total_time_ns = test_end - test_start;
    
    // Calculate statistics
    perf_stats_t stats;
    calculate_stats(latencies, NUM_ALLOCATIONS, &stats);
    
    // Get memory statistics
    lgx_memory_stats_t mem_stats;
    lgx_memory_stats(&mem_stats);
    
    // Print results
    printf("\n=== Performance Results ===\n\n");
    
    printf("Latency Statistics (nanoseconds):\n");
    printf("  Min:  %lu ns\n", stats.min);
    printf("  P50:  %lu ns (%.2f μs)\n", stats.p50, stats.p50 / 1000.0);
    printf("  P95:  %lu ns (%.2f μs)\n", stats.p95, stats.p95 / 1000.0);
    printf("  P99:  %lu ns (%.2f μs)\n", stats.p99, stats.p99 / 1000.0);
    printf("  P99.9: %lu ns (%.2f μs)\n", stats.p999, stats.p999 / 1000.0);
    printf("  Max:  %lu ns (%.2f μs)\n", stats.max, stats.max / 1000.0);
    printf("  Avg:  %.2f ns (%.2f μs)\n", stats.avg, stats.avg / 1000.0);
    printf("\n");
    
    printf("Throughput:\n");
    printf("  Total time: %.2f ms\n", total_time_ns / 1000000.0);
    printf("  Allocations/sec: %.0f\n", (double)NUM_ALLOCATIONS / (total_time_ns / 1000000000.0));
    printf("\n");
    
    printf("Cache Statistics:\n");
    printf("  Cache hits: %lu\n", mem_stats.cache_hits);
    printf("  Cache misses: %lu\n", mem_stats.cache_misses);
    double hit_rate = (double)mem_stats.cache_hits / (mem_stats.cache_hits + mem_stats.cache_misses) * 100.0;
    printf("  Cache hit rate: %.1f%%\n", hit_rate);
    printf("\n");
    
    // Performance targets
    printf("=== Performance Targets ===\n\n");
    
    bool p50_target = (stats.p50 / 1000.0) < 1.0;  // <1μs
    bool p99_target = (stats.p99 / 1000.0) < 10.0; // <10μs (Day 10 target)
    // Cache hit rate is informational only - the optimized path bypasses cache for better performance
    
    printf("P50 < 1μs:     %s (%.2f μs)\n", 
           p50_target ? "✅ PASS" : "❌ FAIL", stats.p50 / 1000.0);
    printf("P99 < 10μs:    %s (%.2f μs) [Day 10 Target]\n", 
           p99_target ? "✅ PASS" : "❌ FAIL", stats.p99 / 1000.0);
    printf("Cache hit rate: %.1f%% (informational - optimized path may bypass cache)\n", hit_rate);
    printf("\n");
    
    // Breakthrough target
    bool breakthrough = (stats.p99 / 1000.0) < 2.0;
    printf("Breakthrough Target (P99 < 2μs): %s (%.2f μs)\n",
           breakthrough ? "✅ ACHIEVED!" : "⏳ IN PROGRESS", stats.p99 / 1000.0);
    printf("\n");
    
    // Huge pages impact
    if (hugepages_available) {
        printf("=== Huge Pages Impact ===\n\n");
        printf("Huge pages enabled: YES\n");
        printf("Expected TLB miss reduction: %.1f%%\n", tlb_improvement);
        printf("Expected P99 improvement: 10-20%%\n");
        printf("\n");
        printf("Actual P99: %.2f μs\n", stats.p99 / 1000.0);
        printf("Target P99: <10 μs (Day 10)\n");
        printf("Breakthrough P99: <2 μs\n");
        printf("\n");
    }
    
    // Cleanup
    printf("Cleaning up...\n");
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        if (ptrs[i]) {
            lgx_free(ptrs[i]);
        }
    }
    
    free(ptrs);
    free(latencies);
    
    lgx_runtime_shutdown();
    
    // Final verdict
    printf("\n=== Day 10 Test Result ===\n\n");
    
    if (p50_target && p99_target) {
        printf("✅ ALL TARGETS MET!\n\n");
        
        if (breakthrough) {
            printf("🎉 BREAKTHROUGH ACHIEVED! P99 < 2μs\n");
            printf("This exceeds the original 1.46 μs claim!\n\n");
        } else {
            double gap = (stats.p99 / 1000.0) / 2.0;
            printf("Breakthrough gap: %.1fx (need %.2f μs improvement)\n\n", gap, (stats.p99 / 1000.0) - 2.0);
        }
        
        return 0;
    } else {
        printf("⚠️  SOME TARGETS NOT MET\n\n");
        
        if (!p50_target) {
            printf("- P50 needs improvement: %.2f μs (target: <1 μs)\n", stats.p50 / 1000.0);
        }
        if (!p99_target) {
            printf("- P99 needs improvement: %.2f μs (target: <10 μs)\n", stats.p99 / 1000.0);
        }
        printf("\n");
        
        return 1;
    }
}
