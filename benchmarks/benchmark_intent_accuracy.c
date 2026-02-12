/**
 * @file benchmark_intent_accuracy.c
 * @brief Benchmark intent-based allocation routing accuracy
 */

#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    lgx_allocation_intent_base_t intent;
    size_t size;
} intent_context_t;

static void bench_frame_intent(void* ctx) {
    intent_context_t* ictx = (intent_context_t*)ctx;
    
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = ictx->size,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    if (ptr) {
        memset(ptr, 0, ictx->size);
        // Frame allocations are not freed individually
    }
}

static void bench_persistent_intent(void* ctx) {
    intent_context_t* ictx = (intent_context_t*)ctx;
    
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = ictx->size,
        .access_pattern = LGX_ACCESS_RANDOM,
        .lifetime = LGX_LIFETIME_SESSION,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    if (ptr) {
        memset(ptr, 0, ictx->size);
        lgx_free(ptr);
    }
}

static void bench_level_intent(void* ctx) {
    intent_context_t* ictx = (intent_context_t*)ctx;
    
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = ictx->size,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_LEVEL,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    if (ptr) {
        memset(ptr, 0, ictx->size);
        lgx_free(ptr);
    }
}

int main(void) {
    printf("LGX Runtime Core - Intent-Based Allocation Benchmark\n");
    printf("=====================================================\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    if (lgx_runtime_init(config) != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        lgx_config_destroy(config);
        return 1;
    }
    
    benchmark_init();
    
    size_t sizes[] = {64, 256, 1024, 4096};
    const char* size_names[] = {"64B", "256B", "1KB", "4KB"};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    benchmark_result_t results[50];
    size_t result_count = 0;
    
    for (size_t i = 0; i < num_sizes; i++) {
        intent_context_t ctx = {
            .size = sizes[i]
        };
        
        char name[256];
        
        // Frame intent
        snprintf(name, sizeof(name), "frame_intent_%s", size_names[i]);
        results[result_count++] = benchmark_run(name, bench_frame_intent, &ctx, 10000, 1000);
        benchmark_print_result(&results[result_count - 1]);
        // Note: Frame allocations are not reset in benchmarks
        
        // Persistent intent
        snprintf(name, sizeof(name), "persistent_intent_%s", size_names[i]);
        results[result_count++] = benchmark_run(name, bench_persistent_intent, &ctx, 10000, 1000);
        benchmark_print_result(&results[result_count - 1]);
        
        // Level intent
        snprintf(name, sizeof(name), "level_intent_%s", size_names[i]);
        results[result_count++] = benchmark_run(name, bench_level_intent, &ctx, 10000, 1000);
        benchmark_print_result(&results[result_count - 1]);
    }
    
    // Save results
    benchmark_save_results("benchmark_intent_accuracy.csv", results, result_count);
    
    benchmark_cleanup();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\nResults saved to: benchmark_intent_accuracy.csv\n");
    
    return 0;
}
