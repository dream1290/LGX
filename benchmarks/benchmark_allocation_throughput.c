/**
 * @file benchmark_allocation_throughput.c
 * @brief Benchmark allocation throughput across different allocators
 */

#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * lgx_frame_reset() is exported by the runtime (lgx_runtime.map, LGX_1.0) and
 * declared in include/lgx/lgx_runtime_internal.h, but the public header
 * include/lgx_runtime.h does NOT declare it -- a public-API gap this benchmark
 * works around rather than hides. Declared here, matching the internal header,
 * instead of in benchmark_framework.h where a second definition could drift.
 */
extern lgx_result_t lgx_frame_reset(void);

/*
 * Deterministic stderr for the timed frame region.
 *
 * lgx_frame_reset() unconditionally fprintf's two lines to stderr whenever two
 * resets are closer together than 8 ms. In this tight loop that is every single
 * reset (~55,000 of them per size), while in a 60 FPS game it never happens
 * (16.7 ms per frame). Left alone, the measured cost depends on where the
 * CALLER's stderr points: measured on this machine, 64B moved 2027 ns -> 2675 ns
 * (+32%) between /dev/null and a pipe, 256B +28%, 1KB +21%, 4KB +10%. A
 * benchmark whose number depends on its invocation is not a benchmark.
 *
 * So the harness mutes stderr around the frame benchmark and restores it after,
 * which makes the result a property of this machine rather than of how the
 * binary was called. THIS IS A WORKAROUND, NOT A CURE: the runtime should
 * rate-limit this warning the way its sibling warning already is, and that is a
 * runtime change deliberately out of scope here. See the task-3 report.
 */
static int g_stderr_saved_fd = -1;
static FILE* g_stderr_null = NULL;

static void stderr_mute_begin(void) {
    if (g_stderr_saved_fd >= 0) return; /* already muted */
    fflush(stderr);
    int saved = dup(STDERR_FILENO);
    FILE* null = fopen("/dev/null", "w");
    if (saved < 0 || !null) {
        /* Cannot control stderr: keep it, the numbers just stay invocation
         * dependent. Do not fail the benchmark over a diagnostic stream. */
        if (saved >= 0) close(saved);
        if (null) fclose(null);
        return;
    }
    if (dup2(fileno(null), STDERR_FILENO) < 0) {
        close(saved);
        fclose(null);
        return;
    }
    g_stderr_saved_fd = saved;
    g_stderr_null = null;
}

static void stderr_mute_end(void) {
    if (g_stderr_saved_fd < 0) return;
    fflush(stderr);
    dup2(g_stderr_saved_fd, STDERR_FILENO);
    close(g_stderr_saved_fd);
    g_stderr_saved_fd = -1;
    fclose(g_stderr_null);
    g_stderr_null = NULL;
}

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
 * The reset's return value is checked on every frame: if the arena were
 * uninitialized lgx_frame_reset() would return an error and silently no-op, and
 * the benchmark would quietly measure the old no-reset pattern while still
 * passing scripts/verify_benchmark_methodology.sh. main() reports the failure
 * count on a FRAME_RESET_STATUS line and exits non-zero.
 */
#define FRAME_BURST 64

static uint64_t g_frame_resets = 0;
static uint64_t g_frame_reset_failures = 0;

static void bench_lgx_frame(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;

    if (lgx_frame_reset() != LGX_SUCCESS) {  /* start from a clean frame */
        g_frame_reset_failures++;
    }
    g_frame_resets++;

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
        stderr_mute_begin();            /* keep diagnostics out of the timing */
        results[result_count] = benchmark_run(names[result_count], bench_lgx_frame, &ctx, 10000, 1000);
        stderr_mute_end();
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

    /* If any reset failed, the frame numbers above describe a no-reset
     * workload and must not be trusted. Say so loudly and fail the run rather
     * than let a bogus baseline propagate. */
    printf("FRAME_RESET_STATUS resets=%llu failures=%llu stderr=muted_during_timing\n",
           (unsigned long long)g_frame_resets,
           (unsigned long long)g_frame_reset_failures);
    if (g_frame_reset_failures > 0) {
        fprintf(stderr, "ERROR: %llu of %llu frame-arena resets failed; "
                        "the frame-arena numbers above are invalid.\n",
                (unsigned long long)g_frame_reset_failures,
                (unsigned long long)g_frame_resets);
        return 1;
    }

    return 0;
}
