# LGX Runtime Performance Tests

This directory contains comprehensive performance benchmarks for the LGX Runtime Core, along with automated regression detection infrastructure.

## Overview

The performance test suite measures:

1. **Initialization Time** - Runtime startup and shutdown latency
2. **Allocation Latency** - Memory allocation performance across different sizes and allocators
3. **Frame-Time Contribution** - Impact of runtime operations on frame budget (60 FPS target)
4. **Memory Overhead** - Runtime memory footprint and allocation overhead

## Performance Targets

### Tier 1 (MVP)
- Initialization: < 1000ms
- Allocation P99: < 5μs
- Frame contribution: < 10% of frame budget
- Memory overhead: < 300MB

### Tier 2 (Competitive)
- Initialization: < 500ms
- Allocation P99: < 1μs
- Frame contribution: < 5% of frame budget
- Memory overhead: < 200MB

## Running Benchmarks

### Build the benchmarks

```bash
# Release build (required for accurate performance measurements)
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

### Run individual benchmarks

```bash
cd build-release/tests/performance

# Initialization time
./perf_test_initialization_time

# Allocation latency
./perf_test_allocation_latency

# Frame-time contribution
./perf_test_frame_time_contribution

# Memory overhead
./perf_test_memory_overhead
```

### Run all benchmarks

```bash
cd build-release/tests/performance
for bench in perf_test_*; do
    echo "Running $bench..."
    ./"$bench"
    echo ""
done
```

## Benchmark Results

Each benchmark exports results to a text file:

- `benchmark_results_init.txt` - Initialization metrics
- `benchmark_results_alloc.txt` - Allocation metrics
- `benchmark_results_frame.txt` - Frame-time metrics
- `benchmark_results_memory.txt` - Memory overhead metrics

### Result Format

Results are in key-value format for easy parsing:

```
benchmark=initialization
iterations=100
init_p50_ns=12345678
init_p95_ns=23456789
init_p99_ns=34567890
tier_passed=2
```

## Performance Regression Detection

### Capturing a Baseline

Before making changes, capture a performance baseline:

```bash
./scripts/capture_baseline.sh
```

This will:
1. Run all benchmarks 3 times
2. Select median results to reduce variance
3. Store baseline in `performance_baseline/`
4. Add metadata (git commit, timestamp, system info)

### Comparing Against Baseline

After making changes, compare current performance:

```bash
# Build and run benchmarks
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
cd build-release/tests/performance
for bench in perf_test_*; do ./"$bench"; done

# Compare against baseline
cd ../../..
mkdir -p current_results
cp build-release/tests/performance/benchmark_results_*.txt current_results/

python3 scripts/compare_benchmarks.py \
    --baseline performance_baseline \
    --current current_results \
    --threshold 5.0 \
    --variance 2.0
```

### Regression Detection Parameters

- `--threshold`: Regression threshold percentage (default: 5%)
- `--variance`: Allowed measurement variance (default: 2%)

A regression is detected if: `(current - baseline) / baseline > (threshold + variance)`

Example: With 5% threshold and 2% variance, a 7.5% increase triggers a regression.

### CI/CD Integration

Performance regression detection runs automatically on:

- **Pull Requests**: Compares against baseline from target branch
- **Main Branch**: Updates baseline after merge

The workflow:
1. Builds project in Release mode
2. Runs all performance benchmarks
3. Downloads baseline from cache
4. Compares results and detects regressions
5. Posts report as PR comment
6. Fails CI if regression detected (>5% + 2% variance)
7. Updates baseline on main branch after merge

See `.github/workflows/performance-regression.yml` for details.

## Benchmark Details

### test_initialization_time.c

Measures runtime initialization and shutdown latency.

**Methodology:**
- 10 warmup iterations
- 100 measurement iterations
- Reports P50, P95, P99, min, max

**Key Metrics:**
- `init_p50_ns` - Median initialization time
- `init_p99_ns` - 99th percentile initialization time
- `shutdown_p50_ns` - Median shutdown time

### test_allocation_latency.c

Measures memory allocation performance across sizes and allocators.

**Methodology:**
- 10,000 iterations per test
- Tests sizes: 16B to 64KB
- Tests intent-based allocators (frame, persistent, level)

**Key Metrics:**
- `size_1024_p99_ns` - P99 latency for 1KB allocations
- `frame_p99_ns` - P99 latency for frame allocations
- `persistent_p99_ns` - P99 latency for persistent allocations

### test_frame_time_contribution.c

Measures runtime impact on frame budget (60 FPS = 16.67ms).

**Methodology:**
- Simulates game frames with varying allocation counts
- 1,000 frames per test
- Mix of allocation sizes (60% small, 30% medium, 10% large)

**Key Metrics:**
- `alloc_100_p99_ns` - P99 frame time with 100 allocations
- `budget_percent` - Percentage of 16.67ms frame budget used

**Target:** < 5% of frame budget (< 833μs for 100 allocations)

### test_memory_overhead.c

Measures runtime memory footprint and allocation overhead.

**Methodology:**
- Measures RSS before/after initialization
- Allocates 10,000 × 1KB blocks
- Compares expected vs actual memory usage

**Key Metrics:**
- `init_overhead_rss_bytes` - RSS increase from initialization
- `alloc_overhead_percent` - Allocation overhead percentage

**Targets:**
- Init overhead: < 200MB (Tier 2), < 300MB (Tier 1)
- Allocation overhead: < 20%

## Interpreting Results

### Good Performance

```
✅ PASSED Tier 2: P99 = 0.89 μs < 1μs
✅ PASSED: Frame contribution = 3.2% < 5%
✅ PASSED Tier 2: Init overhead = 156.3 MB < 200MB
```

### Performance Regression

```
❌ REGRESSION  init_p99_ns      450000000 ns → 520000000 ns (+15.6%)
❌ REGRESSION  size_1024_p99_ns 890 ns → 1250 ns (+40.4%)
```

**Action:** Investigate the change that caused the regression.

### Acceptable Variance

```
✅ OK          init_p50_ns      420000000 ns → 425000000 ns (+1.2%)
✅ OK          frame_p99_ns     245000 ns → 248000 ns (+1.2%)
```

**Note:** Small variations (<2%) are normal due to system load, CPU frequency scaling, etc.

## Best Practices

### For Accurate Measurements

1. **Use Release builds** - Debug builds have 10-100x overhead
2. **Close background apps** - Reduce system noise
3. **Run multiple times** - Take median to reduce variance
4. **Disable CPU frequency scaling** - For consistent results
   ```bash
   sudo cpupower frequency-set --governor performance
   ```
5. **Disable turbo boost** - For reproducible results
   ```bash
   echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
   ```

### For Development

1. **Capture baseline before changes** - Always have a comparison point
2. **Run benchmarks after changes** - Detect regressions early
3. **Profile if regression detected** - Use `perf` to find hotspots
4. **Update baseline intentionally** - Only after verifying changes

### For CI/CD

1. **Baseline is cached** - Automatically managed by GitHub Actions
2. **PR checks block merge** - If regression > threshold
3. **Main branch updates baseline** - After successful merge
4. **Manual override** - Update baseline with `./scripts/capture_baseline.sh`

## Troubleshooting

### Benchmark fails to run

```bash
# Check if runtime is built
ls -l build-release/src/libgx_runtime.so

# Check if benchmark is built
ls -l build-release/tests/performance/perf_test_*

# Rebuild if needed
cmake --build build-release
```

### High variance in results

- Close background applications
- Run benchmarks multiple times
- Check CPU frequency scaling
- Check system load (`top`, `htop`)

### Regression detection false positives

- Increase `--variance` parameter (default: 2%)
- Check if system configuration changed
- Verify baseline was captured on same hardware

### Missing baseline

```bash
# Capture new baseline
./scripts/capture_baseline.sh

# Or download from CI artifacts
# (GitHub Actions → Performance Regression Detection → Artifacts)
```

## Contributing

When adding new benchmarks:

1. Follow naming convention: `test_<feature>_<metric>.c`
2. Export results to `benchmark_results_<name>.txt`
3. Use key-value format: `metric_name=value`
4. Include P50, P95, P99 percentiles
5. Add validation against performance targets
6. Update this README with benchmark details
7. Update `CMakeLists.txt` to build new benchmark

## References

- [Performance Targets Explained](../../docs/PERFORMANCE_TARGETS_EXPLAINED.md)
- [Phase 0 Performance Results](../../docs/PHASE_0_PERFORMANCE_RESULTS.md)
- [Design Document - Performance Characteristics](../../.kiro/specs/lgx-runtime-core/design.md)
