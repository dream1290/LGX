# Changelog

All notable changes to LGX Runtime Core will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-02-12

### Bug Fixes

- **namespace-isolation**: Fixed ISO C pedantic compliance in function pointer conversion
- **test-suite**: Eliminated variable length arrays (VLA) across all test files for better portability
- **test-suite**: Added NULL safety checks with early returns in test code
- **test-suite**: Fixed stack protector warnings in 10 test files
- **signal-handling**: Suppressed intentional NULL dereference warnings in signal handling tests

### New Features

- **benchmarks**: Added comprehensive benchmark suite with 5 benchmarks
- **benchmarks**: Benchmark framework with statistical analysis (min, max, mean, median, P95, P99, stddev)
- **benchmarks**: CSV export functionality for benchmark results
- **benchmarks**: Allocation throughput benchmark
- **benchmarks**: Memory patterns benchmark
- **benchmarks**: Hardware adaptation benchmark
- **benchmarks**: Intent accuracy benchmark
- **benchmarks**: Telemetry overhead benchmark

### Security Improvements

- **static-analysis**: All code passes strict GCC static analysis with `-fanalyzer`
- **compiler-warnings**: Zero warnings with `-Wpedantic -Werror`
- **code-quality**: Enhanced ISO C compliance across codebase
- **security-audit**: Comprehensive security audit completed successfully

### Build Improvements

- **compilation**: All 59 tests compile cleanly with zero warnings
- **compilation**: All 5 benchmarks compile cleanly with zero warnings
- **release-builds**: Zero compilation warnings in optimized release builds
- **portability**: Improved code portability by eliminating VLAs

### Documentation

- Added `SECURITY_AUDIT_FIXES_SUMMARY.md` documenting all fixes
- Added `SECURITY_AUDIT_STATUS.md` with audit status and recommendations
- Updated security documentation with audit results

## [1.0.0] - 2026-02-09

### Features

- **frame-arena**: Ultra-fast bump pointer allocation (P99 < 100ns)
- **gpu-pool**: GPU memory pool with Vulkan integration
- **persistent-heap**: Fragmentation-resistant allocator for long-lived data
- **intent-api**: Intent-based allocation API for automatic routing
- **hardware-adaptation**: Hardware tier classification and graceful degradation
- **telemetry**: Privacy-first telemetry framework with adaptive sampling
- **observability**: Comprehensive error handling and structured logging
- **security**: Input validation, memory safety, and fuzzing tests
- **testing**: Complete test suite (unit, integration, performance, ABI, fuzzing)
- **packaging**: Distribution packages for Debian, RPM, and Arch Linux

### Performance

- **allocation**: Frame arena P99 = 84ns (Tier 2 target: <1μs)
- **memory**: Runtime overhead = 1.03 MB (199x under target)
- **initialization**: Init time = 2.70 ms (185x faster than target)
- **optimization**: Lock-free techniques, huge pages, SIMD acceleration

### Documentation

- API reference documentation
- Integration guide
- Architecture documentation
- Performance optimization reports
- Security threat model
- Testing documentation

### Build System

- CMake build system with symbol versioning
- GitHub Actions CI/CD pipeline
- Multi-distribution testing (Ubuntu, Fedora, Arch)
- Performance regression detection
- ABI compatibility testing

### Packaging

- Debian/Ubuntu packages (.deb)
- Fedora/RHEL packages (.rpm)
- Arch Linux packages (.pkg.tar.zst)
- Universal installation scripts
- Automated build scripts

### Tests

- Unit tests for all components
- Integration tests for end-to-end workflows
- Performance benchmarks
- ABI compatibility tests
- Fuzzing tests (AFL, libFuzzer)
- Failure injection tests
- Chaos testing framework

### Security

- Input validation on all API functions
- Memory safety features (guard pages, canaries, delayed reclamation)
- Namespace isolation for library pinning
- Security testing (fuzzing, static analysis)
- Threat model documentation

## [Unreleased]

### Planned Features

- Additional documentation (API examples, tutorials)
- Production hardening (resource limits, monitoring)
- Performance optimizations (per-thread caches, lock-free paths)
- Hardware diversity testing (multiple GPU vendors, NUMA configurations)

---

## Version History

- **1.0.0** (2026-02-09) - Initial release

## Links

- [GitHub Repository](https://github.com/lgx-platform/LGX)
- [Issue Tracker](https://github.com/lgx-platform/LGX/issues)
- [Documentation](https://github.com/lgx-platform/LGX/tree/main/docs)
