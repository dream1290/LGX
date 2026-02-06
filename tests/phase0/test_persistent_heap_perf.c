/**
 * Performance test for persistent heap allocator (Task 3.3.3.4)
 * 
 * Validates P99 < 20 μs for persistent heap allocations
 */

#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define NUM_SAMPLES 10000
#define WARMUP_SAMPLES 1000

// Get time in nanoseconds
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Compare function for qsort
static int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

// Calculate percentile
static uint64_t percentile(uint64_t* samples, int count, double p) {
    int index = (int)((double)count * p);
    if (index >= count) index = count - 1;
    return samples[index];
}

int main(void) {
    printf("=== Persistent Heap Performance Test ===\n");
    printf("Validating P99 < 20 μs (Task 3.3.3.4)\n\n");
    
    lgx_persistent_heap_init();
    
    uint64_t* latencies = (uint64_t*)malloc(sizeof(uint64_t) * NUM_SAMPLES);
    if (!latencies) {
        fprintf(stderr, "Failed to allocate latency array\n");
        return 1;
    }
    
    // Test small allocations (segregated fit)
    printf("[TEST] Small allocations (64B - 4KB)\n");
    
    // Warmup
    for (int i = 0; i < WARMUP_SAMPLES; i++) {
        void* ptr = lgx_heap_alloc(256);
        lgx_heap_free(ptr);
    }
    
    // Measure
    for (int i = 0; i < NUM_SAMPLES; i++) {
        size_t size = 64 + (rand() % 4000);  // 64B - 4KB
        
        uint64_t start = get_time_ns();
        void* ptr = lgx_heap_alloc(size);
        uint64_t end = get_time_ns();
        
        latencies[i] = end - start;
        lgx_heap_free(ptr);
    }
    
    // Sort and calculate percentiles
    qsort(latencies, NUM_SAMPLES, sizeof(uint64_t), compare_uint64);
    
    uint64_t p50 = percentile(latencies, NUM_SAMPLES, 0.50);
    uint64_t p95 = percentile(latencies, NUM_SAMPLES, 0.95);
    uint64_t p99 = percentile(latencies, NUM_SAMPLES, 0.99);
    uint64_t p999 = percentile(latencies, NUM_SAMPLES, 0.999);
    
    printf("  P50:  %.3f μs\n", p50 / 1000.0);
    printf("  P95:  %.3f μs\n", p95 / 1000.0);
    printf("  P99:  %.3f μs\n", p99 / 1000.0);
    printf("  P999: %.3f μs\n", p999 / 1000.0);
    
    if (p99 < 20000) {  // 20 μs = 20000 ns
        printf("  ✅ P99 < 20 μs (target met)\n");
    } else {
        printf("  ❌ P99 >= 20 μs (target missed)\n");
    }
    
    // Test large allocations (buddy allocator)
    printf("\n[TEST] Large allocations (8KB - 1MB)\n");
    
    // Warmup
    for (int i = 0; i < WARMUP_SAMPLES; i++) {
        void* ptr = lgx_heap_alloc(16 * 1024);
        lgx_heap_free(ptr);
    }
    
    // Measure
    for (int i = 0; i < NUM_SAMPLES; i++) {
        size_t size = (8 + (rand() % 1000)) * 1024;  // 8KB - 1MB
        
        uint64_t start = get_time_ns();
        void* ptr = lgx_heap_alloc(size);
        uint64_t end = get_time_ns();
        
        latencies[i] = end - start;
        lgx_heap_free(ptr);
    }
    
    // Sort and calculate percentiles
    qsort(latencies, NUM_SAMPLES, sizeof(uint64_t), compare_uint64);
    
    p50 = percentile(latencies, NUM_SAMPLES, 0.50);
    p95 = percentile(latencies, NUM_SAMPLES, 0.95);
    p99 = percentile(latencies, NUM_SAMPLES, 0.99);
    p999 = percentile(latencies, NUM_SAMPLES, 0.999);
    
    printf("  P50:  %.3f μs\n", p50 / 1000.0);
    printf("  P95:  %.3f μs\n", p95 / 1000.0);
    printf("  P99:  %.3f μs\n", p99 / 1000.0);
    printf("  P999: %.3f μs\n", p999 / 1000.0);
    
    if (p99 < 20000) {  // 20 μs = 20000 ns
        printf("  ✅ P99 < 20 μs (target met)\n");
    } else {
        printf("  ❌ P99 >= 20 μs (target missed)\n");
    }
    
    // Test mixed allocations
    printf("\n[TEST] Mixed allocations (16B - 1MB)\n");
    
    // Warmup
    for (int i = 0; i < WARMUP_SAMPLES; i++) {
        size_t size = 16 + (rand() % (1024 * 1024));
        void* ptr = lgx_heap_alloc(size);
        lgx_heap_free(ptr);
    }
    
    // Measure
    for (int i = 0; i < NUM_SAMPLES; i++) {
        size_t size = 16 + (rand() % (1024 * 1024));  // 16B - 1MB
        
        uint64_t start = get_time_ns();
        void* ptr = lgx_heap_alloc(size);
        uint64_t end = get_time_ns();
        
        latencies[i] = end - start;
        lgx_heap_free(ptr);
    }
    
    // Sort and calculate percentiles
    qsort(latencies, NUM_SAMPLES, sizeof(uint64_t), compare_uint64);
    
    p50 = percentile(latencies, NUM_SAMPLES, 0.50);
    p95 = percentile(latencies, NUM_SAMPLES, 0.95);
    p99 = percentile(latencies, NUM_SAMPLES, 0.99);
    p999 = percentile(latencies, NUM_SAMPLES, 0.999);
    
    printf("  P50:  %.3f μs\n", p50 / 1000.0);
    printf("  P95:  %.3f μs\n", p95 / 1000.0);
    printf("  P99:  %.3f μs\n", p99 / 1000.0);
    printf("  P999: %.3f μs\n", p999 / 1000.0);
    
    bool target_met = (p99 < 20000);
    if (target_met) {
        printf("  ✅ P99 < 20 μs (target met)\n");
    } else {
        printf("  ❌ P99 >= 20 μs (target missed)\n");
    }
    
    free(latencies);
    lgx_persistent_heap_shutdown();
    
    printf("\n=== Summary ===\n");
    if (target_met) {
        printf("✅ Performance target met: P99 < 20 μs\n");
        return 0;
    } else {
        printf("❌ Performance target missed: P99 >= 20 μs\n");
        return 1;
    }
}
