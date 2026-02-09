# LGX Runtime Core - Chaos Testing Guide

## Overview

The LGX Runtime Core includes a comprehensive chaos testing framework that allows developers to validate system resilience under adverse conditions. This guide documents all chaos testing scenarios, expected behaviors, and integration patterns.

## Purpose

Chaos testing helps validate that the runtime:
- Handles failures gracefully without crashing
- Provides clear error messages and recovery guidance
- Maintains data integrity under stress
- Degrades gracefully when resources are constrained
- Recovers properly from transient failures

## Configuration

### Chaos Config Structure

```c
typedef struct lgx_chaos_config {
    size_t struct_size;
    
    // Memory pressure simulation
    bool inject_memory_pressure;     // Enable allocation failures
    double failure_rate;              // 0.01 = 1% of allocations fail
    
    // Latency spike simulation
    bool inject_latency_spikes;      // Enable random delays
    uint64_t max_latency_spike_ns;   // Maximum delay (e.g., 1ms = 1000000ns)
    double latency_spike_rate;       // 0.01 = 1% of operations get delays
    
    // NUMA imbalance simulation
    bool inject_numa_imbalance;      // Simulate NUMA issues
    
    // GPU hang simulation
    bool inject_gpu_hangs;           // Simulate driver hangs
    double gpu_hang_rate;            // 0.001 = 0.1% of GPU operations hang
    
    // I/O error simulation
    bool inject_io_errors;           // Simulate filesystem failures
    double io_error_rate;            // 0.01 = 1% of I/O operations fail
    
    // Reproducibility
    uint32_t random_seed;            // 0 = use time-based seed
} lgx_chaos_config_t;
```

### Enabling Chaos Testing

```c
// Example: Enable chaos testing with 1% failure rate
lgx_chaos_config_t chaos_config = {
    .struct_size = sizeof(lgx_chaos_config_t),
    .inject_memory_pressure = true,
    .failure_rate = 0.01,  // 1% of allocations fail
    .inject_latency_spikes = true,
    .max_latency_spike_ns = 1000000,  // 1ms max delay
    .latency_spike_rate = 0.01,  // 1% of operations delayed
    .inject_gpu_hangs = true,
    .gpu_hang_rate = 0.001,  // 0.1% of GPU operations hang
    .inject_io_errors = true,
    .io_error_rate = 0.01,  // 1% of I/O operations fail
    .random_seed = 12345  // For reproducible tests
};

lgx_result_t result = lgx_runtime_enable_chaos_testing(&chaos_config);
if (result != LGX_SUCCESS) {
    fprintf(stderr, "Failed to enable chaos testing: %s\n", 
            lgx_result_to_string(result));
}
```

### Disabling Chaos Testing

```c
lgx_result_t result = lgx_runtime_disable_chaos_testing();
// Prints statistics: injected failures, latency spikes, total operations
```

## Chaos Testing Scenarios

### Scenario 1: Memory Pressure (Allocation Failures)

**Purpose**: Validate that the runtime handles out-of-memory conditions gracefully.

**Configuration**:
```c
chaos_config.inject_memory_pressure = true;
chaos_config.failure_rate = 0.05;  // 5% failure rate
```

**Injection Points**:
- `lgx_alloc()` - General allocations
- `lgx_alloc_with_intent()` - Intent-based allocations
- `lgx_frame_alloc()` - Frame arena allocations
- `lgx_heap_alloc()` - Persistent heap allocations
- `lgx_gpu_alloc()` - GPU memory allocations

**Expected Behaviors**:
1. **Allocation returns NULL**: Functions return NULL pointer
2. **Error context set**: `lgx_get_last_error()` returns `LGX_ERROR_OUT_OF_MEMORY`
3. **Recovery guidance provided**: Error context includes recovery steps
4. **No crash**: Runtime continues operating
5. **Logging**: Warning logged to subsystem (MEMORY, GPU)

**Example Test**:
```c
// Enable chaos testing
lgx_chaos_config_t config = {
    .struct_size = sizeof(lgx_chaos_config_t),
    .inject_memory_pressure = true,
    .failure_rate = 0.10,  // 10% failure rate for testing
    .random_seed = 42
};
lgx_runtime_enable_chaos_testing(&config);

// Attempt allocations
int success_count = 0;
int failure_count = 0;

for (int i = 0; i < 1000; i++) {
    void* ptr = lgx_alloc(1024);
    if (ptr) {
        success_count++;
        lgx_free(ptr);
    } else {
        failure_count++;
        
        // Verify error context
        lgx_error_context_ex_t error = lgx_get_last_error_ex();
        assert(error.base.error_code == LGX_ERROR_OUT_OF_MEMORY);
        assert(error.recoverable == true);
        assert(error.suggested_action == LGX_RECOVER_DEGRADE);
    }
}

// Verify failure rate is approximately 10%
assert(failure_count >= 80 && failure_count <= 120);  // 10% ± 2%

lgx_runtime_disable_chaos_testing();
```

**Validation Checklist**:
- [ ] Allocation returns NULL on failure
- [ ] Error code is `LGX_ERROR_OUT_OF_MEMORY`
- [ ] Recovery guidance is provided
- [ ] No segmentation faults or crashes
- [ ] Failure rate matches configuration
- [ ] Statistics are tracked correctly

---

### Scenario 2: Latency Spikes

**Purpose**: Validate that the runtime handles unexpected delays without deadlocks or timeouts.

**Configuration**:
```c
chaos_config.inject_latency_spikes = true;
chaos_config.max_latency_spike_ns = 10000000;  // 10ms max
chaos_config.latency_spike_rate = 0.02;  // 2% of operations
```

**Injection Points**:
- Platform services (filesystem, timing)
- Memory allocations
- GPU operations

**Expected Behaviors**:
1. **Operation completes**: Despite delay, operation succeeds
2. **No deadlocks**: System doesn't hang
3. **Performance degradation**: Measured latency increases
4. **Logging**: Debug log shows injected latency

**Example Test**:
```c
lgx_chaos_config_t config = {
    .struct_size = sizeof(lgx_chaos_config_t),
    .inject_latency_spikes = true,
    .max_latency_spike_ns = 5000000,  // 5ms
    .latency_spike_rate = 0.50,  // 50% for testing
    .random_seed = 42
};
lgx_runtime_enable_chaos_testing(&config);

// Measure allocation latency
uint64_t start = lgx_time_now_ns();
void* ptr = lgx_alloc(1024);
uint64_t end = lgx_time_now_ns();
uint64_t latency_ns = end - start;

// Some allocations should have latency spikes
// (but not all, due to randomness)
if (latency_ns > 1000000) {  // > 1ms
    printf("Latency spike detected: %lu ns\n", latency_ns);
}

lgx_free(ptr);
lgx_runtime_disable_chaos_testing();
```

**Validation Checklist**:
- [ ] Operations complete successfully
- [ ] No deadlocks or hangs
- [ ] Latency increases are measurable
- [ ] Spike rate matches configuration
- [ ] System remains responsive

---

### Scenario 3: GPU Hangs

**Purpose**: Validate that the runtime handles GPU driver hangs gracefully.

**Configuration**:
```c
chaos_config.inject_gpu_hangs = true;
chaos_config.gpu_hang_rate = 0.01;  // 1% of GPU operations
```

**Injection Points**:
- `lgx_gpu_alloc()` - GPU memory allocations
- GPU command submission (future)

**Expected Behaviors**:
1. **Timeout detection**: Operation times out after 1 second
2. **Error returned**: Function returns NULL or error code
3. **Error context set**: `lgx_get_last_error()` returns `LGX_ERROR_GPU_UNAVAILABLE`
4. **Recovery guidance**: Suggests driver reset or fallback
5. **Logging**: Warning logged to GPU subsystem

**Example Test**:
```c
lgx_chaos_config_t config = {
    .struct_size = sizeof(lgx_chaos_config_t),
    .inject_gpu_hangs = true,
    .gpu_hang_rate = 0.20,  // 20% for testing
    .random_seed = 42
};
lgx_runtime_enable_chaos_testing(&config);

// Attempt GPU allocation
lgx_gpu_allocation_t* alloc = lgx_gpu_alloc(1024, 256, LGX_GPU_DEVICE_LOCAL);

if (!alloc) {
    // Verify error context
    lgx_error_context_ex_t error = lgx_get_last_error_ex();
    if (error.base.error_code == LGX_ERROR_GPU_UNAVAILABLE) {
        printf("GPU hang simulated: %s\n", error.recovery_steps);
        // Expected: "Check GPU drivers and Vulkan installation"
    }
} else {
    lgx_gpu_free(alloc);
}

lgx_runtime_disable_chaos_testing();
```

**Validation Checklist**:
- [ ] GPU operations timeout appropriately
- [ ] Error code is `LGX_ERROR_GPU_UNAVAILABLE`
- [ ] Recovery guidance is provided
- [ ] No infinite hangs
- [ ] System can recover from GPU hang

---

### Scenario 4: I/O Errors

**Purpose**: Validate that the runtime handles filesystem failures gracefully.

**Configuration**:
```c
chaos_config.inject_io_errors = true;
chaos_config.io_error_rate = 0.05;  // 5% of I/O operations
```

**Injection Points**:
- `lgx_platform_services_fs_open()` - File open
- `lgx_platform_services_fs_read()` - File read
- `lgx_platform_services_fs_write()` - File write

**Expected Behaviors**:
1. **Operation fails**: Function returns -1 or error code
2. **Error context set**: `lgx_get_last_error()` returns `LGX_ERROR_IO_ERROR`
3. **Recovery guidance**: Suggests checking permissions, disk space
4. **Logging**: Error logged to FILESYSTEM subsystem
5. **No data corruption**: Partial writes are handled safely

**Example Test**:
```c
lgx_chaos_config_t config = {
    .struct_size = sizeof(lgx_chaos_config_t),
    .inject_io_errors = true,
    .io_error_rate = 0.30,  // 30% for testing
    .random_seed = 42
};
lgx_runtime_enable_chaos_testing(&config);

// Attempt file operations
int fd = lgx_platform_services_fs_open(services, "/tmp/test.txt", O_RDONLY);

if (fd < 0) {
    // Verify error context
    lgx_error_context_ex_t error = lgx_get_last_error_ex();
    assert(error.base.error_code == LGX_ERROR_IO_ERROR);
    printf("I/O error simulated: %s\n", error.recovery_steps);
} else {
    lgx_platform_services_fs_close(services, fd);
}

lgx_runtime_disable_chaos_testing();
```

**Validation Checklist**:
- [ ] I/O operations fail appropriately
- [ ] Error code is `LGX_ERROR_IO_ERROR`
- [ ] Recovery guidance is provided
- [ ] No data corruption
- [ ] System can continue after I/O failure

---

## Integration with CI/CD

### Automated Chaos Testing

Chaos testing should be integrated into the CI/CD pipeline to catch regressions:

```bash
#!/bin/bash
# chaos_test_suite.sh

# Run chaos tests with different configurations
echo "Running chaos test suite..."

# Test 1: Memory pressure
./test_chaos_memory_pressure --failure-rate=0.05 --iterations=10000

# Test 2: Latency spikes
./test_chaos_latency_spikes --spike-rate=0.02 --max-latency-ms=10

# Test 3: GPU hangs
./test_chaos_gpu_hangs --hang-rate=0.01 --timeout-sec=5

# Test 4: I/O errors
./test_chaos_io_errors --error-rate=0.05 --operations=1000

# Test 5: Combined chaos (all failures enabled)
./test_chaos_combined --all-failures --duration-sec=60

echo "Chaos test suite complete"
```

### CI/CD Configuration (GitHub Actions)

```yaml
name: Chaos Testing

on:
  pull_request:
    branches: [ main ]
  schedule:
    - cron: '0 2 * * *'  # Run nightly at 2 AM

jobs:
  chaos-tests:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Build runtime
      run: |
        mkdir build && cd build
        cmake -DCMAKE_BUILD_TYPE=Debug ..
        make -j$(nproc)
    
    - name: Run chaos test suite
      run: |
        cd build
        ./scripts/chaos_test_suite.sh
    
    - name: Upload chaos test results
      if: always()
      uses: actions/upload-artifact@v3
      with:
        name: chaos-test-results
        path: build/chaos_test_results.json
```

---

## Statistics and Monitoring

### Chaos Testing Statistics

The chaos testing framework tracks statistics that can be queried:

```c
uint64_t total_ops, failures, latency_spikes;
lgx_chaos_get_stats(&total_ops, &failures, &latency_spikes);

printf("Chaos Testing Statistics:\n");
printf("  Total operations: %lu\n", total_ops);
printf("  Injected failures: %lu (%.2f%%)\n", 
       failures, (double)failures / total_ops * 100.0);
printf("  Latency spikes: %lu (%.2f%%)\n",
       latency_spikes, (double)latency_spikes / total_ops * 100.0);
```

### Logging

Chaos testing events are logged to appropriate subsystems:

```
[2026-02-08 10:15:23.456] [DEBUG] [MEMORY] Chaos: Injecting allocation failure (42/1000)
[2026-02-08 10:15:23.789] [DEBUG] [CORE] Chaos: Injecting latency spike of 5234567 ns (15/1000)
[2026-02-08 10:15:24.123] [WARN] [GPU] Chaos: Simulating GPU hang
[2026-02-08 10:15:24.456] [DEBUG] [FS] Chaos: Injecting I/O error
```

---

## Best Practices

### 1. Start with Low Failure Rates

Begin with low failure rates (1-5%) and gradually increase:

```c
// Start conservative
chaos_config.failure_rate = 0.01;  // 1%

// Increase for stress testing
chaos_config.failure_rate = 0.10;  // 10%
```

### 2. Use Reproducible Seeds

For debugging, use fixed random seeds:

```c
chaos_config.random_seed = 42;  // Reproducible test
```

### 3. Test Recovery Paths

Verify that your application handles failures correctly:

```c
void* ptr = lgx_alloc(size);
if (!ptr) {
    // REQUIRED: Handle allocation failure
    lgx_error_context_ex_t error = lgx_get_last_error_ex();
    
    if (error.suggested_action == LGX_RECOVER_DEGRADE) {
        // Reduce quality, free caches, retry
        reduce_memory_usage();
        ptr = lgx_alloc(size);
    }
    
    if (!ptr) {
        // Still failed: critical error
        fatal_error("Cannot allocate memory");
    }
}
```

### 4. Monitor Performance Impact

Chaos testing adds overhead. Measure the impact:

```c
// Baseline (no chaos)
uint64_t start = lgx_time_now_ns();
for (int i = 0; i < 10000; i++) {
    void* ptr = lgx_alloc(1024);
    lgx_free(ptr);
}
uint64_t baseline_time = lgx_time_now_ns() - start;

// With chaos testing
lgx_runtime_enable_chaos_testing(&config);
start = lgx_time_now_ns();
for (int i = 0; i < 10000; i++) {
    void* ptr = lgx_alloc(1024);
    if (ptr) lgx_free(ptr);
}
uint64_t chaos_time = lgx_time_now_ns() - start;

double overhead = (double)(chaos_time - baseline_time) / baseline_time * 100.0;
printf("Chaos testing overhead: %.2f%%\n", overhead);
```

### 5. Disable in Production

**CRITICAL**: Never enable chaos testing in production builds:

```c
#ifdef LGX_ENABLE_CHAOS_TESTING
    // Only available in debug/test builds
    lgx_runtime_enable_chaos_testing(&config);
#endif
```

---

## Troubleshooting

### Issue: Chaos testing not injecting failures

**Symptoms**: Failure rate is 0% despite configuration

**Causes**:
1. Chaos testing not enabled
2. Random seed produces no failures in test window
3. Failure rate too low for small test

**Solutions**:
```c
// Verify chaos testing is enabled
if (!lgx_runtime_is_chaos_testing_enabled()) {
    fprintf(stderr, "Chaos testing not enabled!\n");
}

// Increase failure rate for testing
chaos_config.failure_rate = 0.50;  // 50% for debugging

// Use fixed seed for reproducibility
chaos_config.random_seed = 42;
```

### Issue: Tests hang indefinitely

**Symptoms**: Test never completes

**Causes**:
1. GPU hang simulation without timeout
2. Deadlock in error handling
3. Infinite retry loop

**Solutions**:
```c
// Add timeout to tests
alarm(60);  // 60 second timeout

// Limit retry attempts
int retry_count = 0;
while (retry_count < 3) {
    void* ptr = lgx_alloc(size);
    if (ptr) break;
    retry_count++;
}
```

### Issue: Inconsistent test results

**Symptoms**: Tests pass sometimes, fail other times

**Causes**:
1. Random seed not fixed
2. Timing-dependent behavior
3. Race conditions

**Solutions**:
```c
// Use fixed seed
chaos_config.random_seed = 42;

// Run multiple iterations
for (int run = 0; run < 10; run++) {
    run_test();
}
```

---

## Summary

The chaos testing framework provides comprehensive failure injection capabilities to validate runtime resilience. Key points:

- **Memory pressure**: Validates OOM handling
- **Latency spikes**: Validates timeout handling
- **GPU hangs**: Validates driver failure handling
- **I/O errors**: Validates filesystem failure handling
- **CI/CD integration**: Automated chaos testing in pipeline
- **Statistics**: Track injection rates and failures
- **Best practices**: Start low, use seeds, test recovery

For questions or issues, see the [LGX Runtime Core documentation](../README.md).
