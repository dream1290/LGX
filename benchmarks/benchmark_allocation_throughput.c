/**
 * @file benchmark_allocation_throughput.c
 * @brief Benchmark allocation throughput across different allocators
 */

#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t size;
    size_t count;
    void** ptrs;
} alloc_context_t;

static void bench_malloc(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;
    void* ptr = malloc(actx->size);
    if (ptr) {
        memset(ptr, 0, actx->size);
        free(ptr);
    }
}

static void bench_lgx_alloc(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;
    void* ptr = lgx_alloc(actx->size);
    if (ptr) {
        memset(ptr, 0, actx->size);
        lgx_free(ptr);
    }
}

static void bench_lgx_frame(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;
    void* ptr = lgx_alloc_frame(actx->size);
    if (ptr) {
        memset(ptr, 0, actx->size);
        // Frame allocations are not freed individually
    }
}

static void bench_lgx_persistent(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;
    void* ptr = lgx_alloc_persistent(actx->size);
    if (ptr) {
        memset(ptr, 0, actx->size);
        lgx_free(ptr);
    }
}

int main(void) {
    printf("LGX Runtime Core - Allocation Throughput Benchmark\n");
    printf("===================================================\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    if (lgx_runtime_init(config) != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        lgx_config_destroy(config);
        return 1;
    }
    
    benchmark_init();
    
    size_t sizes[] = {16, 64, 256, 1024, 4096};
    const char* size_names[] = {"16B", "64B", "256B", "1KB", "4KB"};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    benchmark_result_t results[100];
    char names[100][256];
    size_t result_count = 0;
    
    for (size_t i = 0; i < num_sizes; i++) {
        alloc_context_t ctx = {
            .size = sizes[i],
            .count = 0,
            .ptrs = NULL
        };
        
        // Benchmark malloc
        snprintf(names[result_count], 256, "malloc_%s", size_names[i]);
        results[result_count] = benchmark_run(names[result_count], bench_malloc, &ctx, 10000, 1000);
        benchmark_print_result(&results[result_count]);
        result_count++;
        
        // Benchmark lgx_alloc
        snprintf(names[result_count], 256, "lgx_alloc_%s", size_names[i]);
        results[result_count] = benchmark_run(names[result_count], bench_lgx_alloc, &ctx, 10000, 1000);
        benchmark_print_result(&results[result_count]);
        result_count++;
        
        // Benchmark lgx_alloc_frame
        snprintf(names[result_count], 256, "lgx_frame_%s", size_names[i]);
        results[result_count] = benchmark_run(names[result_count], bench_lgx_frame, &ctx, 10000, 1000);
        benchmark_print_result(&results[result_count]);
        result_count++;
        // Note: Frame allocations are not reset in benchmarks
        
        // Benchmark lgx_alloc_persistent
        snprintf(names[result_count], 256, "lgx_persistent_%s", size_names[i]);
        results[result_count] = benchmark_run(names[result_count], bench_lgx_persistent, &ctx, 10000, 1000);
        benchmark_print_result(&results[result_count]);
        result_count++;
    }
    
    // Save results
    benchmark_save_results("benchmark_allocation_throughput.csv", results, result_count);
    
    benchmark_cleanup();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\nResults saved to: benchmark_allocation_throughput.csv\n");
    
    return 0;
}
