/**
 * @file benchmark_framework.c
 * @brief Benchmark framework implementation
 */

#include "benchmark_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_SAMPLES 1000000

static uint64_t* g_samples = NULL;
static size_t g_sample_count = 0;
static size_t g_sample_capacity = 0;

/**
 * Get current time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * Compare function for qsort
 */
static int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

/**
 * Calculate percentile
 */
static double calculate_percentile(uint64_t* sorted_samples, size_t count, double percentile) {
    if (count == 0) return 0.0;
    
    double index = (percentile / 100.0) * (count - 1);
    size_t lower = (size_t)floor(index);
    size_t upper = (size_t)ceil(index);
    
    if (lower == upper) {
        return (double)sorted_samples[lower];
    }
    
    double weight = index - lower;
    return (1.0 - weight) * sorted_samples[lower] + weight * sorted_samples[upper];
}

void benchmark_init(void) {
    g_sample_capacity = MAX_SAMPLES;
    g_samples = (uint64_t*)malloc(g_sample_capacity * sizeof(uint64_t));
    g_sample_count = 0;
}

void benchmark_cleanup(void) {
    free(g_samples);
    g_samples = NULL;
    g_sample_count = 0;
    g_sample_capacity = 0;
}

uint64_t benchmark_start(void) {
    return get_time_ns();
}

uint64_t benchmark_stop(uint64_t start) {
    uint64_t end = get_time_ns();
    return end - start;
}

benchmark_result_t benchmark_run(
    const char* name,
    void (*func)(void* ctx),
    void* ctx,
    uint64_t iterations,
    uint64_t warmup_iterations
) {
    benchmark_result_t result = {0};
    result.name = name;
    result.iterations = iterations;
    
    if (!g_samples) {
        benchmark_init();
    }
    
    // Warmup
    for (uint64_t i = 0; i < warmup_iterations; i++) {
        func(ctx);
    }
    
    // Actual benchmark
    g_sample_count = 0;
    for (uint64_t i = 0; i < iterations && g_sample_count < g_sample_capacity; i++) {
        uint64_t start = benchmark_start();
        func(ctx);
        uint64_t elapsed = benchmark_stop(start);
        g_samples[g_sample_count++] = elapsed;
    }
    
    if (g_sample_count == 0) {
        return result;
    }
    
    // Sort samples for percentile calculation
    qsort(g_samples, g_sample_count, sizeof(uint64_t), compare_uint64);
    
    // Calculate statistics
    result.min_ns = g_samples[0];
    result.max_ns = g_samples[g_sample_count - 1];
    
    uint64_t sum = 0;
    for (size_t i = 0; i < g_sample_count; i++) {
        sum += g_samples[i];
    }
    result.total_ns = sum;
    result.mean_ns = (double)sum / g_sample_count;
    
    result.median_ns = calculate_percentile(g_samples, g_sample_count, 50.0);
    result.p95_ns = calculate_percentile(g_samples, g_sample_count, 95.0);
    result.p99_ns = calculate_percentile(g_samples, g_sample_count, 99.0);
    
    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = 0; i < g_sample_count; i++) {
        double diff = g_samples[i] - result.mean_ns;
        variance += diff * diff;
    }
    result.stddev_ns = sqrt(variance / g_sample_count);
    
    return result;
}

void benchmark_print_result(const benchmark_result_t* result) {
    printf("\n");
    printf("Benchmark: %s\n", result->name);
    printf("=================================================\n");
    printf("Iterations:  %lu\n", result->iterations);
    printf("Total time:  %.2f ms\n", result->total_ns / 1000000.0);
    printf("\n");
    printf("Latency Statistics (nanoseconds):\n");
    printf("  Min:       %lu ns (%.2f μs)\n", result->min_ns, result->min_ns / 1000.0);
    printf("  Max:       %lu ns (%.2f μs)\n", result->max_ns, result->max_ns / 1000.0);
    printf("  Mean:      %.2f ns (%.2f μs)\n", result->mean_ns, result->mean_ns / 1000.0);
    printf("  Median:    %.2f ns (%.2f μs)\n", result->median_ns, result->median_ns / 1000.0);
    printf("  P95:       %.2f ns (%.2f μs)\n", result->p95_ns, result->p95_ns / 1000.0);
    printf("  P99:       %.2f ns (%.2f μs)\n", result->p99_ns, result->p99_ns / 1000.0);
    printf("  Std Dev:   %.2f ns\n", result->stddev_ns);
    printf("\n");
    printf("Throughput:  %.2f ops/sec\n", 1000000000.0 / result->mean_ns);
    printf("=================================================\n");
}

bool benchmark_save_results(const char* filename, const benchmark_result_t* results, size_t count) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        return false;
    }
    
    fprintf(f, "benchmark,iterations,min_ns,max_ns,mean_ns,median_ns,p95_ns,p99_ns,stddev_ns\n");
    
    for (size_t i = 0; i < count; i++) {
        const benchmark_result_t* r = &results[i];
        fprintf(f, "%s,%lu,%lu,%lu,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                r->name, r->iterations, r->min_ns, r->max_ns,
                r->mean_ns, r->median_ns, r->p95_ns, r->p99_ns, r->stddev_ns);
    }
    
    fclose(f);
    return true;
}

void benchmark_compare(const benchmark_result_t* baseline, const benchmark_result_t* current) {
    printf("\n");
    printf("Comparison: %s\n", baseline->name);
    printf("=================================================\n");
    
    double mean_change = ((current->mean_ns - baseline->mean_ns) / baseline->mean_ns) * 100.0;
    double p99_change = ((current->p99_ns - baseline->p99_ns) / baseline->p99_ns) * 100.0;
    
    printf("Mean:      %.2f ns -> %.2f ns (%+.1f%%)\n",
           baseline->mean_ns, current->mean_ns, mean_change);
    printf("P99:       %.2f ns -> %.2f ns (%+.1f%%)\n",
           baseline->p99_ns, current->p99_ns, p99_change);
    
    if (mean_change > 5.0) {
        printf("\nWARNING: Performance regression detected (>5%% slower)\n");
    } else if (mean_change < -5.0) {
        printf("\nIMPROVEMENT: Performance improved (>5%% faster)\n");
    } else {
        printf("\nNo significant performance change\n");
    }
    
    printf("=================================================\n");
}
