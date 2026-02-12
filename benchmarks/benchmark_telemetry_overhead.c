/**
 * @file benchmark_telemetry_overhead.c
 * @brief Benchmark telemetry and monitoring overhead
 */

#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>

static void bench_memory_stats(void* ctx) {
    (void)ctx;
    lgx_memory_stats_t stats;
    lgx_result_t result = lgx_memory_stats(&stats);
    (void)result;
}

static void bench_get_counter(void* ctx) {
    (void)ctx;
    uint64_t alloc_count = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
    uint64_t dealloc_count = lgx_get_counter(LGX_COUNTER_DEALLOCATIONS);
    (void)alloc_count;
    (void)dealloc_count;
}

static void bench_health_check(void* ctx) {
    (void)ctx;
    lgx_health_status_t health;
    lgx_result_t result = lgx_runtime_health_check(&health);
    (void)result;
}

static void bench_alloc_with_stats(void* ctx) {
    (void)ctx;
    void* ptr = lgx_alloc(1024);
    if (ptr) {
        lgx_memory_stats_t stats;
        lgx_memory_stats(&stats);
        lgx_free(ptr);
    }
}

int main(void) {
    printf("LGX Runtime Core - Telemetry Overhead Benchmark\n");
    printf("================================================\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    if (lgx_runtime_init(config) != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        lgx_config_destroy(config);
        return 1;
    }
    
    benchmark_init();
    
    benchmark_result_t results[10];
    size_t result_count = 0;
    
    // Benchmark memory stats query
    results[result_count++] = benchmark_run("memory_stats", 
                                           bench_memory_stats, NULL, 100000, 10000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Benchmark counter query
    results[result_count++] = benchmark_run("get_counter", 
                                           bench_get_counter, NULL, 100000, 10000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Benchmark health check
    results[result_count++] = benchmark_run("health_check", 
                                           bench_health_check, NULL, 10000, 1000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Benchmark allocation with stats
    results[result_count++] = benchmark_run("alloc_with_stats", 
                                           bench_alloc_with_stats, NULL, 10000, 1000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Print current statistics
    printf("\nCurrent Runtime Statistics:\n");
    printf("===========================\n");
    
    lgx_memory_stats_t stats;
    if (lgx_memory_stats(&stats) == LGX_SUCCESS) {
        printf("Total Allocated: %lu bytes\n", stats.total_allocated);
        printf("Total Deallocated: %lu bytes\n", stats.total_deallocated);
        printf("Current Allocated: %lu bytes\n", stats.current_allocated);
        printf("Peak Allocated: %lu bytes\n", stats.peak_allocated);
    }
    
    printf("\nPerformance Counters:\n");
    printf("Allocations: %lu\n", lgx_get_counter(LGX_COUNTER_ALLOCATIONS));
    printf("Deallocations: %lu\n", lgx_get_counter(LGX_COUNTER_DEALLOCATIONS));
    printf("Cache Hits: %lu\n", lgx_get_counter(LGX_COUNTER_CACHE_HITS));
    printf("Cache Misses: %lu\n", lgx_get_counter(LGX_COUNTER_CACHE_MISSES));
    
    lgx_health_status_t health;
    if (lgx_runtime_health_check(&health) == LGX_SUCCESS) {
        const char* health_str = "UNKNOWN";
        switch (health.overall_health) {
            case LGX_HEALTH_GOOD: health_str = "GOOD"; break;
            case LGX_HEALTH_WARNING: health_str = "WARNING"; break;
            case LGX_HEALTH_CRITICAL: health_str = "CRITICAL"; break;
            case LGX_HEALTH_FAILED: health_str = "FAILED"; break;
        }
        printf("\nHealth Status: %s\n", health_str);
    }
    
    // Save results
    benchmark_save_results("benchmark_telemetry_overhead.csv", results, result_count);
    
    benchmark_cleanup();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\nResults saved to: benchmark_telemetry_overhead.csv\n");
    
    return 0;
}
