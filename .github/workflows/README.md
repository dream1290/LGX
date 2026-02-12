# LGX Runtime CI/CD Workflows

This directory contains production-grade GitHub Actions workflows for continuous integration, testing, and release management.

## Workflows Overview

### Core Workflows

#### 1. Continuous Integration (`ci.yml`)
**Trigger**: Push to main/develop, Pull Requests
**Purpose**: Primary CI pipeline for code quality and testing

**Jobs**:
- **Code Quality**: Format checking, static analysis, security scanning
- **Build & Test**: Multi-compiler, multi-OS matrix testing
  - Ubuntu 22.04, 20.04
  - GCC 11, 12
  - Clang 14, 15
  - Debug and Release builds
- **Sanitizer Tests**: AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer
- **Multi-Distro**: Ubuntu, Fedora, Arch Linux
- **Performance Check**: Quick performance regression check on PRs
- **Coverage**: Code coverage analysis with Codecov integration

**Key Features**:
- Parallel execution with fail-fast disabled
- Artifact upload for test results
- Installation verification
- Concurrency control to cancel outdated runs

#### 2. Performance Regression Detection (`performance-regression.yml`)
**Trigger**: Pull Requests, Push to main, Nightly, Manual
**Purpose**: Detect performance regressions before merge

**Jobs**:
- **Performance Benchmark**: Compare current vs baseline
  - Allocation latency tests
  - Memory footprint tests
  - Initialization time tests
  - Throughput tests
- **Memory Profiling**: Valgrind massif profiling

**Key Features**:
- Configurable regression threshold (default: 5%)
- Automatic baseline comparison
- PR comments with regression report
- Fails CI if regression detected
- Baseline updates on main branch

#### 3. Release (`release.yml`)
**Trigger**: Version tags (v*.*.*), Manual dispatch
**Purpose**: Automated release process

**Jobs**:
- **Validate Release**: Version format, CHANGELOG verification
- **Build Source**: Source tarball with checksums
- **Build Packages**: DEB, RPM, Arch packages for multiple distros
- **Run Tests**: Full test suite on release build
- **Create Release**: GitHub release with all artifacts

**Key Features**:
- Multi-distribution package building
- Automatic changelog extraction
- SHA256/SHA512 checksums
- Installation instructions in release notes
- Pre-release support

### Specialized Workflows

#### 4. ABI Compatibility (`abi-compatibility.yml`)
**Trigger**: Pull Requests, Nightly, Manual
**Purpose**: Ensure binary compatibility across versions

**Features**:
- Critical path testing on PRs
- Full matrix testing nightly
- ABI compatibility reports
- Historical tracking

#### 5. Multi-Distribution Testing (`multi-distro.yml`)
**Trigger**: Push to main, Pull Requests, Nightly
**Purpose**: Verify compatibility across Linux distributions

**Tested Distributions**:
- Ubuntu 22.04, 20.04
- Fedora 38, 39
- Arch Linux (rolling)

#### 6. Chaos Testing (`chaos-testing.yml`)
**Trigger**: Pull Requests, Nightly, Manual
**Purpose**: Failure injection and stress testing

**Test Categories**:
- Memory pressure
- Latency spikes
- I/O errors
- Combined chaos scenarios

#### 7. Compatibility Matrix (`compatibility-matrix.yml`)
**Trigger**: Pull Requests, Push to main, Nightly
**Purpose**: Comprehensive compatibility testing

**Test Matrix**:
- Multiple OS versions
- Multiple compilers
- Multiple kernel versions
- GPU compatibility (placeholder for self-hosted runners)

#### 8. Fuzzing (`fuzzing.yml`)
**Trigger**: Nightly, Manual
**Purpose**: Automated fuzzing for security and stability

**Fuzzers**:
- libFuzzer (LLVM)
- AFL (American Fuzzy Lop)

**Targets**:
- API input validation
- Allocation patterns
- Lifecycle operations

#### 9. Coverity Scan (`coverity.yml`)
**Trigger**: Weekly, Manual
**Purpose**: Static analysis with Coverity

**Features**:
- Automated Coverity submission
- Build log artifacts on failure

## Workflow Best Practices

### Concurrency Control
All workflows use concurrency groups to cancel outdated runs:
```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

### Artifact Management
- Test results: 7 days retention
- Performance results: 30 days retention
- Release artifacts: Permanent

### Security
- Secrets used: `COVERITY_SCAN_TOKEN`, `COVERITY_SCAN_EMAIL`
- Permissions: Minimal required permissions per workflow
- No secrets in logs or artifacts

### Performance Optimization
- Parallel builds with `CMAKE_BUILD_PARALLEL_LEVEL`
- Ninja generator for faster builds
- Artifact caching where appropriate
- Fail-fast disabled for comprehensive testing

## Required Secrets

Configure these secrets in repository settings:

| Secret | Purpose | Required For |
|--------|---------|--------------|
| `COVERITY_SCAN_TOKEN` | Coverity Scan authentication | coverity.yml |
| `COVERITY_SCAN_EMAIL` | Coverity Scan email | coverity.yml |
| `CODECOV_TOKEN` | Codecov upload (optional) | ci.yml |

## Status Badges

Add these badges to README.md:

```markdown
[![CI](https://github.com/your-org/lgx-runtime-core/workflows/Continuous%20Integration/badge.svg)](https://github.com/your-org/lgx-runtime-core/actions/workflows/ci.yml)
[![Performance](https://github.com/your-org/lgx-runtime-core/workflows/Performance%20Regression%20Detection/badge.svg)](https://github.com/your-org/lgx-runtime-core/actions/workflows/performance-regression.yml)
[![Release](https://github.com/your-org/lgx-runtime-core/workflows/Release/badge.svg)](https://github.com/your-org/lgx-runtime-core/actions/workflows/release.yml)
```

## Workflow Triggers Summary

| Workflow | Push | PR | Schedule | Manual |
|----------|------|----|---------| -------|
| CI | main, develop | Yes | No | Yes |
| Performance | main | Yes | Nightly | Yes |
| Release | No | No | No | Yes (tags) |
| ABI Compatibility | No | Yes | Nightly | Yes |
| Multi-Distro | main | Yes | Nightly | No |
| Chaos Testing | No | Yes | Nightly | Yes |
| Compatibility Matrix | main | Yes | Nightly | Yes |
| Fuzzing | No | No | Nightly | Yes |
| Coverity | main, develop | No | Weekly | Yes |

## Maintenance

### Adding New Tests
1. Add test to appropriate workflow
2. Update test matrix if needed
3. Verify artifact upload/download
4. Test workflow with manual trigger

### Updating Dependencies
1. Update package lists in workflows
2. Test on all supported distributions
3. Update documentation

### Performance Baselines
- Baselines auto-update on main branch
- Manual baseline updates via workflow dispatch
- Stored in GitHub Actions cache

## Troubleshooting

### Common Issues

**Build Failures**:
- Check compiler version compatibility
- Verify all dependencies installed
- Review build logs in artifacts

**Test Failures**:
- Check test timeout settings
- Review sanitizer output
- Verify test environment setup

**Performance Regressions**:
- Review regression report in PR comments
- Compare with historical data
- Check for system load during benchmark

**Package Build Failures**:
- Verify packaging scripts
- Check distribution-specific dependencies
- Test locally with Docker

### Getting Help

1. Check workflow run logs
2. Download and review artifacts
3. Open issue with workflow run link
4. Tag maintainers for urgent issues

## Future Improvements

- [ ] Self-hosted runners for GPU testing
- [ ] Kernel-specific testing infrastructure
- [ ] Extended fuzzing campaigns (24+ hours)
- [ ] Performance trend visualization
- [ ] Automated security scanning (Snyk, Dependabot)
- [ ] Container image builds and publishing
- [ ] Documentation deployment
- [ ] Benchmark result database

## Contributing

When adding or modifying workflows:

1. Test locally with `act` if possible
2. Use manual trigger for initial testing
3. Document changes in this README
4. Update status badges if needed
5. Ensure backward compatibility
6. Follow existing naming conventions

## License

These workflows are part of the LGX Runtime Core project and are licensed under Apache 2.0.
