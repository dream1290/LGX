/**
 * @file benchmark_allocation_throughput.c
 * @brief Benchmark allocation throughput across different allocators
 */

#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * lgx_frame_reset() is exported by the runtime (lgx_runtime.map, LGX_1.0) and
 * declared in include/lgx/lgx_runtime_internal.h, but the public header
 * include/lgx_runtime.h does NOT declare it -- a public-API gap this benchmark
 * works around rather than hides. Declared here, matching the internal header,
 * instead of in benchmark_framework.h where a second definition could drift.
 */
extern lgx_result_t lgx_frame_reset(void);

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

/*
 * Frame-arena benchmark using the REAL frame pattern.
 *
 * A frame is: begin -> allocate -> use -> reset. The previous version of this
 * function allocated monotonically across all 10,000 iterations without ever
 * resetting, which measures cold growing allocation -- the pattern least
 * favourable to a bump allocator, and not the pattern any game uses.
 *
 * FRAME_BURST controls how many allocations happen inside one frame. 64 is
 * representative of a particle system or a per-frame buffer batch.
 *
 * One measured interval is therefore ONE FRAME (BURST allocations + the
 * reclaiming reset), not one allocation, so these numbers are not directly
 * comparable to the malloc_/lgx_alloc_ rows above, which are per allocation.
 *
 * Caveat on the reset's own cost: lgx_frame_reset() prints an unconditional
 * "Frame reset called very frequently" warning to stderr whenever two resets
 * are less than 8 ms apart, which is every frame here but never in a 60 FPS
 * game. Measured separately, the reset costs ~0.3 us with stderr discarded
 * and ~1.3 us with the warning actually written, so a sizeable, fixed part of
 * every interval below is a diagnostic that production never pays. See
 * benchmarks/baseline_frame_arena.txt and the task-3 report.
 */
#define FRAME_BURST 64

static void bench_lgx_frame(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;

    lgx_frame_reset();                 /* start from a clean frame */

    for (int i = 0; i < FRAME_BURST; i++) {
        void* ptr = lgx_alloc_frame(actx->size);
        if (ptr) {
            memset(ptr, 0, actx->size); /* touch it, as a real frame would */
        }
    }
    /* The reset happens at the top of the next call, so the cost of
     * reclaiming the whole frame is included in the measured interval. */
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

        /* Machine-readable line consumed by scripts/verify_benchmark_methodology.sh
         * and by Plan 01's performance gate.
         *
         * benchmark_run() returns by value; benchmark_result_t stores latency
         * fields as double nanoseconds (median_ns, p99_ns). */
        {
            const benchmark_result_t* fr = &results[result_count - 1];
            printf("FRAME_RESET_PATTERN size=%s median=%.0fns p99=%.0fns\n",
                   size_names[i],
                   fr->median_ns,
                   fr->p99_ns);
        }

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
