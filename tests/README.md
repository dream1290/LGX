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

## Test Integrity Contract

A test that cannot fail is a defect, not a passing test.

### The rule

Every test file must contain at least one mechanism that can cause a **non-zero
process exit**. A file with none of these always exits 0: it passes
unconditionally, so including it in a pass rate inflates that rate while
proving nothing.

Accepted mechanisms, as implemented by `scripts/verify_test_integrity.sh`:

| Mechanism | Example |
|---|---|
| Non-zero return from `main()` | `return 1;` |
| `exit()` with a non-zero status | `exit(1)`, `EXIT_FAILURE` |
| Abort | `abort()`, `__builtin_trap()`, `raise(SIGABRT)` |
| A failure counter that `main()` reads **and** something increments | `return (failures == 0) ? 0 : 1;` with a real `failures++` |
| A macro defined in the test file that returns non-zero, invoked from `main()` | `#define CHECK(c) do { if (!(c)) return 1; } while (0)` |

A verdict that is a bare call — `return run_suite();` — is **not** accepted: a
callee's return value cannot be read without dataflow analysis. Write it so the
value is visible:

```c
int rc = run_suite();   /* rc is driven by this assignment */
return rc;              /* accepted */
```

This errs toward reporting a capable file as incapable, never the reverse, so a
gate built on it cannot let an incapable test through.

### The gate scripts must stay tracked by git

`.gitignore` ignores `/scripts/*` and then un-ignores the scripts CI depends on.
That makes four files' tracked status depend on a handful of negation lines:

```text
scripts/verify_test_integrity.sh        fails CI if a test cannot report failure
scripts/count_tests.sh                  the census the gate is measured against
scripts/check_gate_scripts_tracked.sh   fails if the two above are untracked
scripts/verify_benchmark_methodology.sh fails CI if the benchmark stops measuring
```

A wholesale rewrite of `.gitignore` — as the next task in this plan does — will
drop those negations unless it keeps them, and then CI silently stops running
the gate. So it is enforced at run time rather than left to a comment: **both
gate scripts call `scripts/check_gate_scripts_tracked.sh` on startup and fail if
either is untracked.**

```bash
scripts/check_gate_scripts_tracked.sh          # PASS/FAIL on its own
git ls-files --error-unmatch scripts/verify_test_integrity.sh scripts/count_tests.sh
```

If you rewrite `.gitignore`, re-add the `!/scripts/...` lines and then run the
check. Do not `git add -f` as a permanent fix — an un-ignoring line is the fix,
because it survives the *next* rewrite too.

### `assert()` does not count

`CMakeLists.txt` defaults `CMAKE_BUILD_TYPE` to Release (lines 9-10) and adds
`-DNDEBUG` to `CMAKE_C_FLAGS_RELEASE` and `CMAKE_C_FLAGS_RELWITHDEBINFO`
(lines 18-19). The generated build flags confirm it:

```text
C_FLAGS = -Wall -Wextra -Werror ... -O3 -DNDEBUG -O2 -DNDEBUG -march=native
```

Under `NDEBUG`, `assert()` expands to `((void)0)`. An `assert()`-only test
therefore exits 0 in every configuration this project ships. Verified by
building `tests/phase0/test_logging.c` with a forced failing assertion against
the real Release flags:

```text
with -DNDEBUG    -> exit 0    (the test "passes")
without -DNDEBUG -> exit 134  (SIGABRT from assert)
```

Where `-DNDEBUG` comes from: CMake's own built-in default for
`CMAKE_C_FLAGS_RELEASE` is `-O3 -DNDEBUG`, and `CMakeLists.txt:18` *appends*
`-O2 -DNDEBUG -march=native` to it. A fresh configure produces:

```text
C_FLAGS = -Wall -Wextra -Werror -fPIC -Wredundant-decls -Wshadow -O3 -DNDEBUG -O2 -DNDEBUG -march=native -std=gnu11
```

Both spellings define `NDEBUG`, and the project's `-O2` comes last, so `assert()`
is compiled out either way. (Reproduce with `env -i PATH=/usr/bin:/bin
HOME=/tmp cmake -S . -B /tmp/somewhere` to confirm the flags come from the
project rather than the environment.)

This is not hypothetical: it is exactly how `test_csf2_numa_awareness.c` was
broken until commit `ca172c2`. The checker reports `assert()`-only files with an
explicit `ASSERT_ONLY` diagnosis so they are not mistaken for covered tests.

### Enforcement

```bash
scripts/verify_test_integrity.sh                            # all tests, exits 1 if any cannot fail
scripts/verify_test_integrity.sh tests/unit/test_init_shutdown.c   # one file
scripts/verify_test_integrity.sh --classify FILE            # one-line machine verdict
scripts/count_tests.sh <build-dir>                          # registered / verifying / incapable / unmapped
```

`verify_test_integrity.sh` is a CI gate: a PR that adds a test without a failure
mechanism fails CI. `count_tests.sh` uses `--classify`, so the gate and the
census can never disagree about what "capable of failing" means.

The checker answers **"could this file ever exit non-zero?"** — not "does it
assert enough?". A test that can fail but checks one trivial condition passes
the gate. `scripts/verify_test_integrity.sh --help` lists every detection rule
and every known limitation (false negatives from `return helper();`, file-wide
credit for `exit()`/`abort()`, unevaluated conditional compilation, and more).

### Current census

Baseline on 2026-10-10, from an actual Release build and an actual run:

| Metric | Count | Command |
|---|---|---|
| Test files under `tests/` (96 `.c` + 2 `.cpp`) | 98 | `verify_test_integrity.sh` (`checked=`) |
| ... of those, can report failure | 68 | (`verifying=`) |
| ... of those, **cannot** report failure | 30 | (`incapable=`, exit 1) |
| ctest-registered tests | 77 | `count_tests.sh <build-dir>` (`registered=`) |
| ... registered and verifying | 51 | (`verifying=`) |
| ... registered and incapable | 26 | (`incapable=`) |
| ... registered but unmappable to a source | 0 | (`unmapped=`) |
| Disabled in CMake (commented out of `*_SOURCES`) | 12 | see below |

The 30 incapable files break down as **25 `ASSERT_ONLY`**, **4 `ALWAYS_ZERO`**
(`main()` returns 0 and nothing else), and **1 `NO_MAIN`**
(`tests/fuzzing/fuzz_allocation_patterns.cpp`, a libFuzzer harness with no `main`).
Four of the 30 are not ctest-registered (two fuzzing harnesses, one debug tool,
`tests/unit/test_memory_protection.c`), which is why 30 − 4 = 26 above.

### Correction to the 2026-10-10 audit

The audit reported "37 of 100 test files have no failure mechanism", and named
specific groups. The aggregate is close to the measured 30 of 98, but four of
its named claims are **wrong**, and were checked by hand:

- **All five performance tests were called incapable.** They are not. Each
  assigns a live `passed` from real measurements
  (`tests/performance/test_memory_footprint.c:90-111`: `passed = 2` / `= 1` /
  `= 0`, then `return (passed > 0) ? 0 : 1;`) and exits 1 when the memory or
  latency target is missed.
- **All seven module smoke tests were called incapable.** They are not. `TEST()`
  increments `tests_run` and `PASS()` increments `tests_passed`, while `FAIL()`
  increments neither (`tests/graphics/test_graphics_basic.c:16-22`), so a failed
  check makes `tests_passed != tests_run` and `main()` returns 1.
- **Four of five threading tests were called incapable.** All five use live
  counters and return non-zero on failure.
- **`tests/phase0/test_frame_arena.c` (363 lines, zero assertions) was called
  incapable.** It is not. It has zero `assert()` calls — which is what the audit
  appears to have measured — but its `TEST()` macro increments `tests_failed`
  (line 25) and `main()` returns `(tests_failed == 0) ? 0 : 1` (line 362).
  Confirmed by building it with one sub-test forced to `return false`: the
  binary exits 1.

The real problem is narrower and worse in kind than "no non-zero exit path":
**25 files assert their way to a pass that Release builds compile out.**

### Reporting rule

Never quote the registered test count as a pass rate. Quote
`scripts/count_tests.sh <build-dir>`, which separates registered, verifying,
incapable, and unmapped. `unmapped` is reported explicitly and is never folded
into `verifying` — an unaccounted-for test is how the count drifts.

### Disabled tests

Twelve test sources are commented out of their `*_SOURCES` list in the build
files, so they are never compiled and never registered with ctest:

| File | Count | Reasons given |
|---|---|---|
| `tests/failure_injection/CMakeLists.txt` | 5 | 4 "Not implemented", 1 "Compilation errors" |
| `tests/integration/CMakeLists.txt` | 4 | 2 "Compilation errors", 1 "API mismatches", 1 non-existent `lgx_version_info_t` API |
| `tests/unit/CMakeLists.txt` | 3 | 2 "Compilation errors", 1 non-existent `lgx_version_info_t` API |

Each must be migrated to the current API or deleted. Leaving them commented out
is how the suite silently stopped covering the runtime: a source file that looks
like a test, is not built, and is not counted anywhere.

Note that `tests/unit/test_memory_protection.c` is one of these twelve and also
appears in the incapable list above — it is both not registered and unable to
report failure.

---

**Last Updated**: 2026-10-10
**Test Suite Version**: 1.0.1
**Status**: 77 registered; 26 of them cannot report failure. Do not quote a
pass rate — run `scripts/count_tests.sh <build-dir>`.
