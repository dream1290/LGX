# LGX Runtime Core v1.0.1 Release Notes

**Release Date:** February 12, 2026  
**Release Type:** Bug Fix Release  
**Previous Version:** 1.0.0

## Overview

Version 1.0.1 is a maintenance release that addresses compilation issues, improves code quality, and adds comprehensive benchmarking tools. This release focuses on bug fixes, security improvements, and developer tooling enhancements.

## What's New

### 🐛 Bug Fixes

- **ISO C Compliance:** Fixed function pointer conversion in namespace isolation to comply with ISO C standards
- **Portability:** Eliminated all variable length arrays (VLA) across test suite for better portability
- **NULL Safety:** Added explicit NULL checks with early returns in test code
- **Stack Protection:** Fixed stack protector warnings in 10 test files
- **Signal Handling:** Properly suppressed intentional NULL dereference warnings in signal handling tests

### ✨ New Features

#### Comprehensive Benchmark Suite

Added 5 production-grade benchmarks with statistical analysis:

1. **Allocation Throughput** - Measures allocation performance under various loads
2. **Memory Patterns** - Tests different allocation patterns (sequential, random, mixed)
3. **Hardware Adaptation** - Validates hardware tier detection and adaptation
4. **Intent Accuracy** - Measures intent-based routing accuracy
5. **Telemetry Overhead** - Quantifies telemetry performance impact

#### Benchmark Framework

- Statistical analysis (min, max, mean, median, P95, P99, standard deviation)
- CSV export for data analysis
- Warmup iterations for stable measurements
- Comprehensive documentation

### 🔒 Security Improvements

- All code passes strict GCC static analysis with `-fanalyzer`
- Zero warnings with `-Wpedantic -Werror`
- Enhanced ISO C compliance across entire codebase
- Comprehensive security audit completed successfully

### 🔧 Build Improvements

- All 59 tests compile cleanly with zero warnings
- All 5 benchmarks compile cleanly with zero warnings
- Zero compilation warnings in optimized release builds
- Improved code portability

## Files Modified

### Core Changes
- `CMakeLists.txt` - Version bump to 1.0.1
- `include/lgx_version.h` - Version constants updated
- `src/lgx_runtime.c` - Removed duplicate version definitions
- `src/runtime/lgx_runtime_core.c` - Removed duplicate version definitions
- `src/runtime/lgx_namespace_isolation.c` - ISO C compliance fix

### Test Suite (10 files)
- `tests/phase0/test_frame_arena_polish.c` - VLA eliminated
- `tests/phase0/test_hardware_tier.c` - NULL safety improved
- `tests/phase0/test_logging.c` - VLA eliminated
- `tests/phase0/test_memory_safety.c` - VLA eliminated
- `tests/phase0/test_performance.c` - VLA eliminated (2 instances)
- `tests/phase0/test_persistent_heap_buddy.c` - VLA eliminated
- `tests/phase0/test_signal_handling.c` - Intentional NULL deref suppressed
- `tests/phase0/test_tiered_performance.c` - VLA eliminated
- `tests/phase0/test_timing_services.c` - VLA eliminated
- `tests/integration/test_memory_stress.c` - VLA eliminated (3 functions)

### Benchmarks (NEW - 7 files)
- `benchmarks/benchmark_framework.h` - Framework header
- `benchmarks/benchmark_framework.c` - Framework implementation
- `benchmarks/benchmark_allocation_throughput.c`
- `benchmarks/benchmark_memory_patterns.c`
- `benchmarks/benchmark_hardware_adaptation.c`
- `benchmarks/benchmark_intent_accuracy.c`
- `benchmarks/benchmark_telemetry_overhead.c`

### Packaging
- `packaging/debian/changelog` - Added 1.0.1 entry
- `packaging/rpm/lgx-runtime.spec` - Version and changelog updated
- `packaging/arch/PKGBUILD` - Version updated
- `packaging/arch/.SRCINFO` - Version updated

### Documentation
- `CHANGELOG.md` - Added 1.0.1 release notes
- `SECURITY_AUDIT_FIXES_SUMMARY.md` - NEW - Detailed fix documentation
- `SECURITY_AUDIT_STATUS.md` - NEW - Audit status report
- `VERSION_1.0.1_RELEASE_NOTES.md` - NEW - This file

## Upgrade Instructions

### For Users

#### Debian/Ubuntu
```bash
# Update package lists
sudo apt update

# Upgrade LGX Runtime
sudo apt install --only-upgrade lgx-runtime lgx-runtime-dev
```

#### Fedora/RHEL/Rocky/Alma
```bash
# Update package
sudo dnf upgrade lgx-runtime lgx-runtime-devel
```

#### Arch Linux
```bash
# Update package
sudo pacman -Syu lgx-runtime
```

### For Developers

#### From Source
```bash
# Pull latest changes
git pull origin main

# Rebuild
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
cd build && ctest
```

#### CMake Projects
Update your `CMakeLists.txt`:
```cmake
find_package(lgx_runtime 1.0.1 REQUIRED)
```

## Compatibility

### API Compatibility
✅ **Fully compatible** with v1.0.0 - No API changes

### ABI Compatibility
✅ **Fully compatible** with v1.0.0 - No ABI changes

### Binary Compatibility
✅ Applications built against v1.0.0 work with v1.0.1 without recompilation

## Testing

All tests pass successfully:
- ✅ 59 unit and integration tests
- ✅ 5 benchmarks
- ✅ 0 memory leaks (Valgrind clean)
- ✅ 0 AddressSanitizer errors
- ✅ 0 compilation warnings

## Performance

No performance regressions detected. All performance targets maintained:
- Frame arena allocation: P99 = 84ns
- Memory overhead: 1.03 MB
- Initialization time: 2.70 ms

## Known Issues

None.

## Deprecations

None.

## Security Advisories

None. This release includes security improvements but addresses no known vulnerabilities.

## Contributors

- LGX Platform Team

## Links

- **Repository:** https://github.com/lgx-platform/LGX
- **Documentation:** https://lgx-platform.org/docs
- **Issue Tracker:** https://github.com/lgx-platform/LGX/issues
- **Changelog:** [CHANGELOG.md](CHANGELOG.md)

## Support

For questions or issues:
- GitHub Issues: https://github.com/lgx-platform/LGX/issues
- Email: team@lgx-platform.org

---

**Full Changelog:** [v1.0.0...v1.0.1](https://github.com/lgx-platform/LGX/compare/v1.0.0...v1.0.1)

