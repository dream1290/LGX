/**
 * @file benchmark_memory_patterns.c
 * @brief Benchmark different memory access patterns
 */

#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE (1024 * 1024)  // 1MB

typedef struct {
    void* buffer;
    size_t size;
    size_t stride;
} pattern_context_t;

static void bench_sequential_read(void* ctx) {
    pattern_context_t* pctx = (pattern_context_t*)ctx;
    volatile uint8_t* buf = (volatile uint8_t*)pctx->buffer;
    uint8_t sum = 0;
    
    for (size_t i = 0; i < pctx->size; i++) {
        sum += buf[i];
    }
}

static void bench_sequential_write(void* ctx) {
    pattern_context_t* pctx = (pattern_context_t*)ctx;
    uint8_t* buf = (uint8_t*)pctx->buffer;
    
    for (size_t i = 0; i < pctx->size; i++) {
        buf[i] = (uint8_t)i;
    }
}

static void bench_strided_read(void* ctx) {
    pattern_context_t* pctx = (pattern_context_t*)ctx;
    volatile uint8_t* buf = (volatile uint8_t*)pctx->buffer;
    uint8_t sum = 0;
    
    for (size_t i = 0; i < pctx->size; i += pctx->stride) {
        sum += buf[i];
    }
}

static void bench_random_read(void* ctx) {
    pattern_context_t* pctx = (pattern_context_t*)ctx;
    volatile uint8_t* buf = (volatile uint8_t*)pctx->buffer;
    uint8_t sum = 0;
    
    // Simple pseudo-random pattern
    size_t index = 0;
    for (size_t i = 0; i < 1000; i++) {
        index = (index * 1103515245 + 12345) % pctx->size;
        sum += buf[index];
    }
}

int main(void) {
    printf("LGX Runtime Core - Memory Access Pattern Benchmark\n");
    printf("===================================================\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    if (lgx_runtime_init(config) != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        lgx_config_destroy(config);
        return 1;
    }
    
    benchmark_init();
    
    // Allocate test buffer
    void* buffer = lgx_alloc(BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate test buffer\n");
        lgx_runtime_shutdown();
        lgx_config_destroy(config);
        return 1;
    }
    memset(buffer, 0, BUFFER_SIZE);
    
    benchmark_result_t results[10];
    size_t result_count = 0;
    
    // Sequential read
    pattern_context_t ctx = {
        .buffer = buffer,
        .size = BUFFER_SIZE,
        .stride = 1
    };
    results[result_count++] = benchmark_run("sequential_read", bench_sequential_read, &ctx, 1000, 100);
    benchmark_print_result(&results[result_count - 1]);
    
    // Sequential write
    results[result_count++] = benchmark_run("sequential_write", bench_sequential_write, &ctx, 1000, 100);
    benchmark_print_result(&results[result_count - 1]);
    
    // Strided read (cache line)
    ctx.stride = 64;
    results[result_count++] = benchmark_run("strided_read_64B", bench_strided_read, &ctx, 1000, 100);
    benchmark_print_result(&results[result_count - 1]);
    
    // Strided read (page)
    ctx.stride = 4096;
    results[result_count++] = benchmark_run("strided_read_4KB", bench_strided_read, &ctx, 1000, 100);
    benchmark_print_result(&results[result_count - 1]);
    
    // Random read
    results[result_count++] = benchmark_run("random_read", bench_random_read, &ctx, 10000, 1000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Save results
    benchmark_save_results("benchmark_memory_patterns.csv", results, result_count);
    
    lgx_free(buffer);
    benchmark_cleanup();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\nResults saved to: benchmark_memory_patterns.csv\n");
    
    return 0;
}
