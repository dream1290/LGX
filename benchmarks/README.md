# LGX Runtime Core - Benchmarks

This directory contains comprehensive benchmarks for measuring the performance characteristics of the LGX Runtime Core.

## Overview

The benchmark suite provides detailed performance analysis across multiple dimensions:

- Allocation throughput and latency
- Memory access patterns
- Hardware adaptation overhead
- Intent-based allocation accuracy
- Telemetry and monitoring overhead

## Building Benchmarks

Benchmarks are built automatically with the main project when testing is enabled:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTING=ON
cmake --build build
```

Benchmark executables are located in `build/benchmarks/`.

## Running Benchmarks

### Run All Benchmarks

```bash
cd build
ctest -L benchmark --output-on-failure
```

### Run Individual Benchmarks

```bash
cd build/benchmarks

# Allocation throughput
./benchmark_benchmark_allocation_throughput

# Memory access patterns
./benchmark_benchmark_memory_patterns

# Hardware adaptation
./benchmark_benchmark_hardware_adaptation

# Intent-based allocation
./benchmark_benchmark_intent_accuracy

# Telemetry overhead
./benchmark_benchmark_telemetry_overhead
```

## Benchmark Descriptions

### 1. Allocation Throughput (`benchmark_allocation_throughput.c`)

Measures allocation and deallocation performance across different allocators and size classes.

**Metrics:**
- Latency (min, max, mean, median, P95, P99)
- Throughput (operations per second)
- Comparison: malloc vs lgx_alloc vs lgx_frame vs lgx_persistent

**Size Classes Tested:**
- 16 bytes
- 64 bytes
- 256 bytes
- 1 KB
- 4 KB

**Output:** `benchmark_allocation_throughput.csv`

### 2. Memory Access Patterns (`benchmark_memory_patterns.c`)

Evaluates performance of different memory access patterns to understand cache behavior.

**Patterns Tested:**
- Sequential read
- Sequential write
- Strided read (64B cache line)
- Strided read (4KB page)
- Random read

**Buffer Size:** 1 MB

**Output:** `benchmark_memory_patterns.csv`

### 3. Hardware Adaptation (`benchmark_hardware_adaptation.c`)

Measures the overhead of hardware capability detection and tier classification.

**Operations Benchmarked:**
- `lgx_runtime_get_hardware_status()`
- `lgx_runtime_has_capability()`
- `lgx_runtime_get_version()`
- `lgx_runtime_check_compatibility()`

**Output:** `benchmark_hardware_adaptation.csv`

### 4. Intent-Based Allocation (`benchmark_intent_accuracy.c`)

Tests the performance of intent-based allocation routing to different allocators.

**Intents Tested:**
- Frame lifetime (LIFETIME_FRAME)
- Persistent lifetime (LIFETIME_PERSISTENT)
- Level lifetime (LIFETIME_LEVEL)

**Access Patterns:**
- Sequential
- Random

**Output:** `benchmark_intent_accuracy.csv`

### 5. Telemetry Overhead (`benchmark_telemetry_overhead.c`)

Quantifies the performance impact of telemetry and monitoring operations.

**Operations Benchmarked:**
- `lgx_memory_stats()`
- `lgx_get_counter()`
- `lgx_runtime_health_check()`
- Allocation with concurrent stats query

**Output:** `benchmark_telemetry_overhead.csv`

## Benchmark Framework

The benchmark framework (`benchmark_framework.c/h`) provides:

- High-resolution timing (nanosecond precision)
- Statistical analysis (min, max, mean, median, P95, P99, stddev)
- Warmup iterations to stabilize cache state
- CSV export for further analysis
- Comparison utilities for regression detection

### Framework API

```c
// Initialize framework
void benchmark_init(void);

// Run a benchmark
benchmark_result_t benchmark_run(
    const char* name,
    void (*func)(void* ctx),
    void* ctx,
    uint64_t iterations,
    uint64_t warmup_iterations
);

// Print results
void benchmark_print_result(const benchmark_result_t* result);

// Save to CSV
bool benchmark_save_results(const char* filename, 
                           const benchmark_result_t* results, 
                           size_t count);

// Compare results
void benchmark_compare(const benchmark_result_t* baseline, 
                      const benchmark_result_t* current);

// Cleanup
void benchmark_cleanup(void);
```

## Performance Targets

Based on production requirements:

| Metric | Target | Current (v1.0.0) |
|--------|--------|------------------|
| 1KB Allocation P99 | < 5 μs | 2.14 μs (57% margin) |
| 64B Allocation P99 | < 10 μs | 4.02 μs (60% margin) |
| Frame Arena P99 | < 1 μs | 0.40 μs |
| Persistent Heap P99 | < 20 μs | 10.92 μs |
| Initialization | < 500 ms | 2.70 ms (185x margin) |

## Interpreting Results

### Latency Metrics

- **Min/Max:** Range of observed latencies
- **Mean:** Average latency (can be skewed by outliers)
- **Median (P50):** Middle value, more robust than mean
- **P95:** 95% of operations complete within this time
- **P99:** 99% of operations complete within this time (tail latency)
- **Std Dev:** Variability in latency

### Throughput

Calculated as: `1,000,000,000 / mean_ns` operations per second

### Performance Analysis

Good performance characteristics:
- Low P99 latency (< 10 μs for critical paths)
- Small difference between mean and P99 (consistent performance)
- Low standard deviation (predictable latency)
- High throughput (> 100K ops/sec for small allocations)

## Regression Detection

Compare benchmark results across versions:

```bash
# Run baseline
git checkout v1.0.0
cmake -B build-baseline -DCMAKE_BUILD_TYPE=Release
cmake --build build-baseline
cd build-baseline/benchmarks
./benchmark_benchmark_allocation_throughput
cp benchmark_allocation_throughput.csv ../../baseline.csv

# Run current
git checkout main
cmake -B build-current -DCMAKE_BUILD_TYPE=Release
cmake --build build-current
cd build-current/benchmarks
./benchmark_benchmark_allocation_throughput

# Compare (manual analysis or use CI pipeline)
diff ../../baseline.csv benchmark_allocation_throughput.csv
```

The CI pipeline automatically detects regressions > 5% and fails the build.

## Best Practices

### Running Benchmarks

1. **Use Release builds:** Debug builds have significant overhead
2. **Minimize system load:** Close other applications
3. **Run multiple times:** Average results across runs
4. **Warm up the system:** Run warmup iterations
5. **Use consistent hardware:** Same CPU, RAM, kernel version

### Build Configuration

```bash
# Optimal benchmark build
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-O3 -march=native -mtune=native" \
      -DENABLE_TESTING=ON

cmake --build build -j$(nproc)
```

### System Configuration

For consistent results:

```bash
# Disable CPU frequency scaling
sudo cpupower frequency-set -g performance

# Disable turbo boost (optional, for consistency)
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo

# Set process priority
sudo nice -n -20 ./benchmark_benchmark_allocation_throughput
```

## Continuous Integration

Benchmarks are integrated into the CI pipeline:

- **Pull Requests:** Quick performance check (subset of benchmarks)
- **Nightly:** Full benchmark suite with regression detection
- **Release:** Complete performance validation

See `.github/workflows/performance-regression.yml` for details.

## Contributing

When adding new benchmarks:

1. Follow the existing naming convention: `benchmark_<category>.c`
2. Use the benchmark framework for consistency
3. Include warmup iterations (typically 10% of total iterations)
4. Test with multiple size classes or configurations
5. Export results to CSV for analysis
6. Update this README with benchmark description
7. Add to `CMakeLists.txt` in the `BENCHMARK_SOURCES` list

### Example Benchmark Template

```c
#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdio.h>

static void bench_my_operation(void* ctx) {
    // Your operation here
}

int main(void) {
    printf("My Benchmark\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    benchmark_init();
    
    benchmark_result_t result = benchmark_run(
        "my_operation",
        bench_my_operation,
        NULL,
        10000,  // iterations
        1000    // warmup
    );
    
    benchmark_print_result(&result);
    benchmark_save_results("my_benchmark.csv", &result, 1);
    
    benchmark_cleanup();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return 0;
}
```

## Troubleshooting

### High Variance

If you see high standard deviation or large P99 spikes:

- Check for background processes
- Verify CPU frequency scaling is disabled
- Increase warmup iterations
- Run on dedicated hardware

### Unexpected Results

- Verify Release build configuration
- Check compiler optimization flags
- Ensure runtime is properly initialized
- Review system resource limits

### Build Failures

- Ensure all dependencies are installed
- Check CMake configuration
- Verify test helpers are available
- Review compiler warnings

## References

- [Performance Testing Guide](../docs/06-testing/performance-testing.md)
- [Optimization Report](../docs/05-performance/optimization-report.md)
- [Benchmark Results](../docs/05-performance/benchmarks.md)
- [CI/CD Workflows](../.github/workflows/README.md)

## License

These benchmarks are part of the LGX Runtime Core project and are licensed under Apache 2.0.

Copyright 2026 LGX Runtime Core Contributors
