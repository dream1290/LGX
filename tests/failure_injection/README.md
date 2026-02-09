# LGX Runtime Failure Injection Tests

This directory contains failure injection tests that verify the LGX Runtime handles edge cases and failures gracefully.

## Overview

Failure injection tests simulate real-world failure scenarios to ensure:
- Graceful degradation under stress
- Clear error reporting
- No crashes or data corruption
- System continues to function after failures

## Test Suites

### 1. OOM Injection (`test_oom_injection.c`)

**Purpose**: Simulate out-of-memory conditions mid-frame

**Tests**:
- Allocate until pool exhaustion
- Verify graceful degradation
- Check error reporting (LGX_ERROR_OUT_OF_MEMORY)
- Verify recovery after freeing memory

**Expected Behavior**:
- Allocations fail gracefully (return NULL)
- Runtime reports OOM error clearly
- System remains functional
- Can allocate again after freeing

**Run**:
```bash
./test_oom_injection
```

### 2. GPU Timeout (`test_gpu_timeout.c`)

**Purpose**: Simulate GPU driver hangs and verify recovery

**Tests**:
- Check GPU capability detection
- Verify graceful degradation without GPU
- Simulate timeout detection
- Verify error reporting

**Expected Behavior**:
- Runtime detects GPU unavailability
- Falls back to software rendering
- Provides clear remediation steps
- System continues without GPU

**Run**:
```bash
./test_gpu_timeout
```

**Note**: Cannot actually hang GPU driver in automated tests. This test validates the detection and fallback mechanisms.

### 3. Library Version Mismatch (`test_library_version_mismatch.c`)

**Purpose**: Verify init fails with clear error on incompatible versions

**Tests**:
- Check current version
- Test incompatible major version rejection
- Test compatible version acceptance
- Verify error messages are clear

**Expected Behavior**:
- Incompatible versions are rejected
- Error code: LGX_ERROR_INCOMPATIBLE_VERSION
- Clear, actionable error messages
- Compatible versions are accepted

**Run**:
```bash
./test_library_version_mismatch
```

### 4. Telemetry Crash (`test_telemetry_crash.c`)

**Purpose**: Verify game continues if telemetry process crashes

**Tests**:
- Enable telemetry
- Verify game works with telemetry
- Simulate telemetry process crash
- Verify game continues unaffected

**Expected Behavior**:
- Game works with or without telemetry
- No crashes on telemetry failure
- Privacy policy is transparent
- Telemetry is opt-in

**Run**:
```bash
./test_telemetry_crash
```

### 5. Filesystem Full (`test_filesystem_full.c`)

**Purpose**: Verify logging disables gracefully when disk is full

**Tests**:
- Initialize with logging enabled
- Verify logging works normally
- Simulate filesystem full (log size limit)
- Verify runtime continues after log failure
- Test log rotation

**Expected Behavior**:
- Log size is limited correctly
- Log rotation prevents unbounded growth
- Runtime continues after I/O errors
- No crashes on write failures

**Run**:
```bash
./test_filesystem_full
```

### 6. TOCTOU Race Conditions (`test_toctou_races.c`)

**Purpose**: Test concurrent operations for race conditions

**Tests**:
- Concurrent allocations from multiple threads
- Double-free detection
- Use-after-free detection
- Concurrent free of same pointer
- Memory statistics consistency

**Expected Behavior**:
- Concurrent allocations work correctly
- Double-free attempts don't crash
- Use-after-free is handled safely
- Race conditions don't corrupt state
- Memory statistics remain consistent

**Run**:
```bash
./test_toctou_races
```

**Note**: Some race conditions may only be caught in debug builds with memory safety features enabled.

## Running All Tests

### Using CTest

```bash
# Build
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# Run all failure injection tests
cd build-release
ctest -L failure_injection --output-on-failure
```

### Using Custom Target

```bash
cd build-release
make run_failure_injection_tests
```

### Running Individually

```bash
cd build-release/tests/failure_injection
./test_oom_injection
./test_gpu_timeout
./test_library_version_mismatch
./test_telemetry_crash
./test_filesystem_full
./test_toctou_races
```

## Test Results

### Success Criteria

✅ **Pass**: Test completes without crashes, verifies expected behavior  
⚠️ **Warning**: Test passes but with degraded functionality (expected)  
❌ **Fail**: Test crashes or unexpected behavior

### Example Output

```
=== OOM Failure Injection Test ===

Test 1: Allocate until pool exhaustion
---------------------------------------
  Allocated: 8192 blocks
  Failed: 15 attempts
  Health check: Degraded
  Allocation failures: 15
✅ Test 1 passed

Test 2: Verify graceful degradation
------------------------------------
  Allocation failed (expected)
  Error code: 5 (Out of memory)
  Error message: Memory pool exhausted
✅ Test 2 passed

=== All OOM tests passed ===
```

## Interpreting Results

### OOM Injection
- Should allocate many blocks before failing
- Should report OOM error clearly
- Should recover after freeing memory

### GPU Timeout
- May report GPU unavailable (expected in CI)
- Should provide remediation steps
- Should continue without GPU

### Library Version Mismatch
- Should reject incompatible versions
- Should accept compatible versions
- Error messages should be actionable

### Telemetry Crash
- Game should work with or without telemetry
- No crashes on telemetry failure
- Privacy policy should be accessible

### Filesystem Full
- Log size should be limited
- Log rotation should work
- Runtime should continue after I/O errors

### TOCTOU Races
- No crashes from concurrent operations
- Double-free detection works
- Memory statistics remain consistent

## Debug vs Release Builds

### Debug Builds
- Enable memory safety features:
  - Guard pages
  - Memory canaries
  - Delayed reclamation (3-frame)
  - Double-free detection
- More verbose error messages
- Slower performance

### Release Builds
- Minimal safety overhead
- Optimized performance
- Some safety features may be disabled
- Still handles failures gracefully

## CI Integration

Failure injection tests are run:
- **Manually**: For detailed analysis
- **Nightly**: As part of comprehensive testing
- **On Demand**: When investigating specific failures

Not run on every PR because:
- Some tests are slow (race condition tests)
- Some tests require specific setup
- Focus on functional correctness in PR checks

## Adding New Tests

To add a new failure injection test:

1. Create `test_<scenario>.c` in this directory
2. Follow the existing test structure:
   ```c
   #define TEST_ASSERT(cond, msg) ...
   
   int main(void) {
       printf("=== Test Name ===\n\n");
       
       // Initialize runtime
       lgx_runtime_config_t* config = lgx_config_create();
       lgx_runtime_init(config);
       
       // Test scenarios
       printf("Test 1: ...\n");
       // ... test code ...
       printf("✅ Test 1 passed\n\n");
       
       // Cleanup
       lgx_runtime_shutdown();
       lgx_config_destroy(config);
       
       return 0;
   }
   ```
3. Add to `CMakeLists.txt`:
   ```cmake
   set(FAILURE_INJECTION_TESTS
       ...
       test_<scenario>.c
   )
   ```
4. Update this README with test description

## Best Practices

### Writing Failure Injection Tests

1. **Test one failure mode at a time**
   - Focus on specific failure scenario
   - Clear test boundaries

2. **Verify graceful degradation**
   - System should not crash
   - Error messages should be clear
   - System should recover when possible

3. **Check health after failures**
   - Use `lgx_runtime_health_check()`
   - Verify system state is consistent

4. **Clean up properly**
   - Free all allocations
   - Shutdown runtime
   - Remove temporary files

5. **Document expected behavior**
   - What failure is being simulated
   - What the expected response is
   - What recovery mechanisms are tested

### Running Tests

1. **Run in debug builds first**
   - More safety checks enabled
   - Better error messages
   - Easier to diagnose issues

2. **Run in release builds too**
   - Verify production behavior
   - Check performance impact
   - Ensure safety features work

3. **Run under valgrind**
   ```bash
   valgrind --leak-check=full ./test_oom_injection
   ```

4. **Run with sanitizers**
   ```bash
   cmake -DCMAKE_C_FLAGS="-fsanitize=address,undefined"
   ```

## Troubleshooting

### Test Hangs

- Check for infinite loops
- Verify timeouts are set
- Look for deadlocks in concurrent tests

### Test Crashes

- Run in debug build
- Use valgrind or sanitizers
- Check error logs
- Verify cleanup code runs

### Inconsistent Results

- Race conditions in concurrent tests
- System load affecting timing
- Insufficient resources
- Run multiple times to verify

## References

- [Design Document - Error Handling](../../.kiro/specs/lgx-runtime-core/design.md)
- [Memory Safety Implementation](../../docs/MEMORY_SAFETY_IMPLEMENTATION.md)
- [Security Testing Guide](../../docs/SECURITY_TESTING.md)
- [Testing Infrastructure](../../docs/TESTING_INFRASTRUCTURE_COMPLETE.md)
