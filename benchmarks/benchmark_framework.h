/**
 * @file benchmark_framework.h
 * @brief Benchmark framework for LGX Runtime Core
 * 
 * Provides utilities for running and reporting benchmarks with statistical analysis.
 */

#ifndef BENCHMARK_FRAMEWORK_H
#define BENCHMARK_FRAMEWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Benchmark configuration
 */
typedef struct {
    const char* name;
    uint64_t iterations;
    uint64_t warmup_iterations;
    bool report_percentiles;
    bool report_histogram;
} benchmark_config_t;

/**
 * Benchmark results
 */
typedef struct {
    const char* name;
    uint64_t iterations;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
    double mean_ns;
    double median_ns;
    double p95_ns;
    double p99_ns;
    double stddev_ns;
} benchmark_result_t;

/**
 * Initialize benchmark framework
 */
void benchmark_init(void);

/**
 * Cleanup benchmark framework
 */
void benchmark_cleanup(void);

/**
 * Start a benchmark timer
 */
uint64_t benchmark_start(void);

/**
 * Stop a benchmark timer and return elapsed nanoseconds
 */
uint64_t benchmark_stop(uint64_t start);

/**
 * Run a benchmark function
 */
benchmark_result_t benchmark_run(
    const char* name,
    void (*func)(void* ctx),
    void* ctx,
    uint64_t iterations,
    uint64_t warmup_iterations
);

/**
 * Print benchmark results
 */
void benchmark_print_result(const benchmark_result_t* result);

/**
 * Save benchmark results to file
 */
bool benchmark_save_results(const char* filename, const benchmark_result_t* results, size_t count);

/**
 * Compare two benchmark results
 */
void benchmark_compare(const benchmark_result_t* baseline, const benchmark_result_t* current);

#ifdef __cplusplus
}
#endif

#endif /* BENCHMARK_FRAMEWORK_H */
