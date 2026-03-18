# LGX Runtime Test Suite

Comprehensive test suite for the LGX Runtime Platform with 76+ tests covering all modules and scenarios.

## Test Organization

### Unit Tests (tests/unit/)
Core functionality tests for individual components.

**Files**: 8 test files
- `test_init_shutdown.c` - Runtime initialization and shutdown
- `test_memory_allocation.c` - Memory allocation APIs
- `test_error_handler.c` - Error handling and recovery
- `test_health_monitor.c` - Health monitoring system
- `test_platform_services.c` - Platform service APIs
- `test_resource_limits.c` - Resource limit enforcement
- `test_memory_protection.c` - Memory safety features
- `test_version_compatibility.c` - Version compatibility checks

**Run**: `make run_unit_tests` or `ctest -R unit_`

### Integration Tests (tests/integration/)
End-to-end tests for component interaction.

**Files**: 6 test files
- `test_end_to_end_initialization.c` - Complete initialization flow
- `test_component_integration.c` - Multi-component interaction
- `test_suspend_resume_cycle.c` - Lifecycle management
- `test_memory_stress.c` - Memory stress testing
- `test_telemetry_collection.c` - Telemetry system integration
- `test_aaa_workload_simulation.c` - AAA game workload simulation

**Run**: `make run_integration_tests` or `ctest -R integration_`

### Phase 0 Tests (tests/phase0/)
Historical validation tests from Phase 0 development.

**Files**: 47 test files covering:
- Critical Success Factors (CSF1-5)
- Frame arena implementation
- GPU pool and capability detection
- Intent-based allocation
- Hardware adaptation
- Telemetry and observability
- Performance validation

**Run**: `make run_phase0_tests` or `ctest -R phase0`

### Performance Tests (tests/performance/)
Performance benchmarking and validation.

**Files**: 5 test files
- `test_allocation_latency.c` - Allocation latency measurements
- `test_frame_time_contribution.c` - Frame time impact
- `test_initialization_time.c` - Startup time validation
- `test_memory_footprint.c` - Memory usage analysis
- `test_memory_overhead.c` - Runtime overhead measurement

**Run**: `make run_performance_tests` or `ctest -R perf_`

### ABI Compatibility Tests (tests/abi/)
Binary compatibility and versioning tests.

**Files**: 3 test files
- `test_symbol_versioning.c` - ELF symbol versioning
- `test_struct_evolution.c` - Struct forward compatibility
- `test_game_v1_0.c` - v1.0 binary compatibility

**Run**: `ctest -R abi_`

### Failure Injection Tests (tests/failure_injection/)
Chaos testing and error path validation.

**Files**: 6 test files
- `test_oom_injection.c` - Out-of-memory scenarios
- `test_gpu_timeout.c` - GPU timeout handling
- `test_filesystem_full.c` - Disk full scenarios
- `test_telemetry_crash.c` - Telemetry process crashes
- `test_library_version_mismatch.c` - Version mismatch handling
- `test_toctou_races.c` - Time-of-check-time-of-use races

**Run**: `ctest -R failure_`

### Fuzzing Tests (tests/fuzzing/)
Fuzz testing for security and robustness.

**Files**: 3 fuzzers
- `fuzz_api_inputs.c` - API input fuzzing
- `fuzz_allocation_patterns.cpp` - Allocation pattern fuzzing
- `fuzz_lifecycle.c` - Lifecycle state fuzzing

**Build**: `./build_afl.sh` or `./build_libfuzzer.sh`

### Module Tests

#### Threading Tests (tests/threading/)
Threading module validation (v1.1).

**Files**: 5 test files
- `test_threading_basic.c` - Basic threading operations
- `test_threading_stress.c` - Stress testing
- `test_threading_fiber.c` - Fiber system
- `test_threading_advanced.c` - Advanced features
- `benchmark_threading.c` - Performance benchmarks

**Run**: `make run_threading_tests` or `ctest -R threading`

#### Graphics Tests (tests/graphics/)
Graphics module validation (v1.2).

**Files**: 1 test file
- `test_graphics_basic.c` - Basic Vulkan wrapper functionality

**Run**: `ctest -R graphics`

#### Input Tests (tests/input/)
Input module validation (v1.3).

**Files**: 1 test file
- `test_input_basic.c` - Gamepad, keyboard, mouse handling

**Run**: `ctest -R input`

#### Audio Tests (tests/audio/)
Audio module validation (v1.4).

**Files**: 1 test file
- `test_audio_basic.c` - 3D audio and ALSA output

**Run**: `ctest -R audio`

#### Profiling Tests (tests/profile/)
Profiling module validation (v1.5).

**Files**: 1 test file
- `test_profile_basic.c` - Frame profiler and counters

**Run**: `ctest -R profile`

#### Networking Tests (tests/net/)
Networking module validation (v2.0).

**Files**: 1 test file
- `test_net_basic.c` - UDP sockets and serialization

**Run**: `ctest -R net`

#### Asset Tests (tests/asset/)
Asset pipeline validation (v2.1).

**Files**: 1 test file
- `test_asset_basic.c` - File loading and hot reload

**Run**: `ctest -R asset`

#### Tools Tests (tests/tools/)
Tooling module validation (v2.2).

**Files**: 1 test file
- `test_tools_basic.c` - Performance analyzer and memory tracker

**Run**: `ctest -R tools`

### Manual Tests (tests/manual/)
Manual testing utilities and debugging tools.

**Files**: 9 test files
- Frame arena debugging and profiling tools
- Adaptive sizing validation
- Overflow handling tests
- Stress testing utilities

**Note**: These are not part of automated test suite. Run manually for debugging.

### Debug Tests (tests/debug/)
Debugging and diagnostic utilities.

**Files**: 1 test file
- `test_frame_arena_diagnosis.c` - Frame arena diagnostics

**Note**: Manual debugging tool, not automated.

## Running Tests

### All Tests
```bash
cd build
ctest --output-on-failure
```

### By Category
```bash
ctest -R unit_          # Unit tests
ctest -R integration_   # Integration tests
ctest -R phase0         # Phase 0 tests
ctest -R perf_          # Performance tests
ctest -R abi_           # ABI tests
ctest -R threading      # Threading tests
ctest -R graphics       # Graphics tests
```

### With Sanitizers
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON ..
make -j$(nproc)
LSAN_OPTIONS=suppressions=../lsan.supp ctest --output-on-failure
```

### Verbose Output
```bash
ctest --verbose
ctest --output-on-failure  # Show output only on failure
```

## Test Statistics

**Total Tests**: 76 tests
- Unit tests: 8
- Integration tests: 6
- Phase 0 tests: 47
- Performance tests: 5
- ABI tests: 3
- Failure injection: 6
- Module tests: 9 (threading, graphics, input, audio, profile, net, asset, tools)
- Manual/debug: 10 (not counted in automated suite)

**Test Coverage**: 100% passing (76/76)

**Test Types**:
- Functional tests: 60
- Performance tests: 5
- Security tests: 6
- Compatibility tests: 3
- Stress tests: 2

## Writing New Tests

### Test Template
```c
#include <lgx_runtime.h>
#include <assert.h>
#include <stdio.h>

int main(void) {
    printf("Test: [Test Name]\n");
    
    // Setup
    lgx_runtime_config_t* config = lgx_config_create();
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    lgx_config_destroy(config);
    
    // Test logic
    // ... your test code ...
    
    // Cleanup
    lgx_runtime_shutdown();
    
    printf("PASS\n");
    return 0;
}
```

### Adding to CMake
```cmake
# In appropriate CMakeLists.txt
add_executable(test_my_feature test_my_feature.c)
target_link_libraries(test_my_feature lgx_runtime)
add_test(NAME test_my_feature COMMAND test_my_feature)
set_tests_properties(test_my_feature PROPERTIES
    LABELS "unit"
    TIMEOUT 30
)
```

### Test Guidelines
1. Each test should be independent (no shared state)
2. Clean up all resources (no memory leaks)
3. Use descriptive test names
4. Print clear pass/fail messages
5. Return 0 on success, non-zero on failure
6. Keep tests focused (one concept per test)
7. Add timeout properties (prevent hangs)

## Continuous Integration

Tests run automatically on:
- Every commit (GitHub Actions)
- Pull requests
- Release builds

**CI Configuration**: `.github/workflows/ci.yml`

## Test Results

Test results are stored in:
- `build/Testing/Temporary/LastTest.log` - Latest test run
- `build/benchmark_*.csv` - Benchmark results
- `build/abi_test_results/` - ABI compatibility results

**Note**: Test binaries and results are in `build/` directory, not in `tests/` source directory.

## Troubleshooting

### Test Failures

**Memory leaks detected**:
```bash
# Check leak sanitizer suppressions
cat lsan.supp
# Run with leak detection
LSAN_OPTIONS=suppressions=lsan.supp ./test_name
```

**Timeout failures**:
```bash
# Increase timeout in CMakeLists.txt
set_tests_properties(test_name PROPERTIES TIMEOUT 60)
```

**Vulkan tests fail**:
```bash
# Check Vulkan installation
vulkaninfo
# Tests skip gracefully if Vulkan unavailable
```

**ALSA tests fail**:
```bash
# Check ALSA installation
aplay -l
# Tests skip gracefully if ALSA unavailable
```

### Debug Failed Tests
```bash
# Run single test with verbose output
./build/test_name

# Run with debugger
gdb ./build/test_name

# Run with valgrind
valgrind --leak-check=full ./build/test_name
```

## Related Documentation

- [Testing Strategy](../docs/06-testing/README.md)
- [Performance Testing](../docs/06-testing/performance-testing.md)
- [Chaos Testing](../docs/06-testing/chaos-testing.md)
- [Fuzzing Guide](../docs/06-testing/fuzzing.md)
- [Complete Test Suite](../docs/06-testing/complete-suite.md)

---

**Last Updated**: 2026-03-18
**Test Suite Version**: 1.0.1
**Status**: All tests passing (76/76)
