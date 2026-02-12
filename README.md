# LGX Runtime Core

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)]()
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)]()
[![Tests](https://img.shields.io/badge/tests-59%2F59%20passing-brightgreen.svg)]()

A high-performance, production-ready Linux gaming runtime with specialized memory allocators, comprehensive lifecycle management, and hardware adaptation.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Performance](#performance)
- [Installation](#installation)
  - [Prerequisites](#prerequisites)
  - [Building from Source](#building-from-source)
  - [Binary Packages](#binary-packages)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Architecture](#architecture)
- [Testing](#testing)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)
- [Support](#support)

## Overview

LGX Runtime Core is a production-ready foundational runtime library designed for high-performance gaming applications on Linux. It provides deterministic behavior, versioned runtime environment, and optimized memory management through a stable C ABI. The runtime manages initialization, lifecycle operations, memory allocation, and platform services with a focus on performance, reliability, and hardware adaptation.

**Current Status**: Production-ready (v1.0.0)
- 100% test pass rate (59/59 tests)
- Zero memory leaks
- All performance targets exceeded
- Security hardening complete

### Key Objectives

- **Performance**: Sub-5μs allocation latency for critical operations
- **Determinism**: Consistent behavior across different Linux distributions and hardware configurations
- **Reliability**: Graceful degradation and comprehensive error handling
- **Compatibility**: Stable ABI with forward compatibility guarantees
- **Security**: Input validation, memory safety, and namespace isolation

## Features

### Memory Management

- **Hybrid Allocation Strategy**: Optimized allocation paths for different usage patterns
  - Hot path cache: Ultra-fast pre-allocated blocks (P99 < 1μs)
  - Lock-free pool: Zero-contention allocation (P99 < 5μs)
  - Direct malloc fallback: For large or infrequent allocations
- **Frame Arena Allocator**: Triple-buffered bump pointer allocation with automatic reset
- **GPU Memory Pool**: Pre-allocated Vulkan memory with buddy allocator
- **Persistent Heap**: Segregated fit allocator with minimal fragmentation
- **Intent-Based API**: Automatic routing to optimal allocator based on usage patterns

### Lifecycle Management

- **Suspend/Resume**: State preservation with minimal overhead
- **Signal Handling**: Graceful shutdown on SIGTERM, crash reporting on SIGSEGV/SIGABRT
- **Crash Dumps**: Automatic generation of stack traces and diagnostic information
- **Resource Limits**: Configurable memory, file handle, and allocation rate limits

### Hardware Adaptation

- **Tier Classification**: Automatic detection of OPTIMAL/COMPATIBLE/DEGRADED hardware tiers
- **Graceful Degradation**: Software fallbacks for missing hardware features
- **Performance Estimation**: Real-time impact assessment for degraded configurations
- **NUMA Awareness**: Topology detection and memory placement optimization
- **Huge Pages Support**: Transparent huge page utilization for reduced TLB misses

### Observability

- **Performance Counters**: Comprehensive metrics for allocations, cache hits, and pool usage
- **Health Monitoring**: Continuous system health assessment with anomaly detection
- **Structured Logging**: Subsystem-tagged logging with runtime filtering
- **Telemetry Framework**: Privacy-preserving performance data collection (opt-in)

### Security

- **Input Validation**: Comprehensive parameter validation with configurable policies
- **Memory Safety**: Guard pages, canaries, and secure memory wiping
- **Namespace Isolation**: Process isolation and library pinning
- **Resource Protection**: Rate limiting and deadlock detection

## Performance

### Benchmark Results

Performance measurements on reference hardware (Intel Xeon, 32GB RAM, NVIDIA RTX 3080):

#### Allocation Latency (Production Build)

| Metric | P50 | P95 | P99 | Target | Status |
|--------|-----|-----|-----|--------|--------|
| 1KB Allocation | 0.45μs | 1.00μs | 2.14μs | <5μs | PASS (57% margin) |
| 64B Allocation | 0.47μs | 0.96μs | 4.02μs | <10μs | PASS (60% margin) |
| Frame Arena | 0.24μs | 0.30μs | 0.40μs | <1μs | PASS |
| Persistent Heap | 5.11μs | 9.18μs | 10.92μs | <20μs | PASS |

#### System Performance

| Operation | Measured | Target | Status |
|-----------|----------|--------|--------|
| Initialize | 2.70ms | <500ms | PASS (185x margin) |
| Suspend | <1ms | <100ms | PASS |
| Resume | <1ms | <100ms | PASS |
| Shutdown | <20ms | <100ms | PASS |

#### Memory Footprint

| Component | Size | Notes |
|-----------|------|-------|
| Runtime Core | 1.03MB | All subsystems initialized |
| Frame Arenas | 192MB | 3 x 64MB buffers (configurable) |
| GPU Pools | 2.3GB | Device + host memory (optional) |
| Thread Pools | 1GB | 64 threads x 16MB (pre-allocated) |

#### Test Coverage

- **Total Tests**: 59/59 passing (100%)
- **Memory Leaks**: 0 bytes
- **Security Tests**: 41/41 passing
- **Performance Tests**: 5/5 passing
- **Integration Tests**: 1/1 passing

## Installation

### Prerequisites

**Required**:
- Linux kernel 5.10 or later (x86_64)
- GCC 11+ or Clang 10+
- CMake 3.16 or later
- pthread library

**Optional**:
- Vulkan 1.3+ (for GPU memory pool)
- jemalloc (for fallback allocator)
- Huge pages support (for TLB optimization)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/your-org/lgx-runtime-core.git
cd lgx-runtime-core

# Create build directory
mkdir build && cd build

# Configure with CMake (Release build)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Install (optional)
sudo make install
```

### Build Options

```bash
# Debug build with symbols and sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_ASAN=ON \
      -DENABLE_UBSAN=ON ..

# Minimal build (no GPU support)
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_GPU_POOL=OFF ..

# With specific compiler
CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Release ..

# Enable additional diagnostics
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_DIAGNOSTICS=ON \
      -DENABLE_TELEMETRY=ON ..
```

### Binary Packages

Pre-built packages are available for major Linux distributions:

**Debian/Ubuntu**:
```bash
wget https://github.com/your-org/lgx-runtime-core/releases/download/v1.0.0/lgx-runtime_1.0.0_amd64.deb
sudo dpkg -i lgx-runtime_1.0.0_amd64.deb
```

**Fedora/RHEL**:
```bash
wget https://github.com/your-org/lgx-runtime-core/releases/download/v1.0.0/lgx-runtime-1.0.0-1.x86_64.rpm
sudo rpm -i lgx-runtime-1.0.0-1.x86_64.rpm
```

**Arch Linux**:
```bash
wget https://github.com/your-org/lgx-runtime-core/releases/download/v1.0.0/lgx-runtime-1.0.0-1-x86_64.pkg.tar.zst
sudo pacman -U lgx-runtime-1.0.0-1-x86_64.pkg.tar.zst
```

## Quick Start

### Basic Example

```c
#include <lgx_runtime.h>
#include <stdio.h>

int main(void) {
    // Create and configure runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_memory_pool_size(config, 256 * 1024 * 1024); // 256MB
    
    // Initialize runtime
    lgx_result_t result = lgx_runtime_init(config);
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Runtime initialization failed: %s\n", 
                lgx_result_to_string(result));
        lgx_config_destroy(config);
        return 1;
    }
    
    // Check hardware capabilities
    lgx_hardware_status_t hw_status = lgx_runtime_get_hardware_status();
    printf("Hardware Tier: %d\n", hw_status.achieved_tier);
    
    // Allocate memory
    void* memory = lgx_alloc(4096);
    if (memory) {
        // Use allocated memory
        // ...
        
        // Free memory
        lgx_free(memory);
    }
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return 0;
}
```

### Compilation and Linking

```bash
# Compile application
gcc -o myapp myapp.c \
    -I/usr/local/include \
    -L/usr/local/lib \
    -llgx_runtime \
    -lpthread

# Run application
LD_LIBRARY_PATH=/usr/local/lib ./myapp
```

### Intent-Based Allocation

```c
// Frame-scoped allocation (automatically freed on frame reset)
void* frame_data = lgx_alloc_frame(1024);

// Persistent allocation (long-lived data)
void* persistent_data = lgx_alloc_persistent(4096);

// Level-scoped allocation (freed on level unload)
void* level_data = lgx_alloc_level(8192);

// Generic allocation with intent
lgx_allocation_intent_base_t intent = {
    .struct_size = sizeof(lgx_allocation_intent_base_t),
    .size = 4096,
    .access_pattern = LGX_ACCESS_SEQUENTIAL,
    .lifetime = LGX_LIFETIME_FRAME,
    .hint = LGX_HINT_CRITICAL_PATH,
    .validation_policy = LGX_INTENT_VALIDATE_WARN
};
void* memory = lgx_alloc_with_intent(&intent);
```

## API Reference

### Initialization

```c
lgx_runtime_config_t* lgx_config_create(void);
void lgx_config_set_memory_pool_size(lgx_runtime_config_t* config, size_t size);
void lgx_config_set_log_path(lgx_runtime_config_t* config, const char* path);
void lgx_config_set_flags(lgx_runtime_config_t* config, uint32_t flags);
void lgx_config_destroy(lgx_runtime_config_t* config);

lgx_result_t lgx_runtime_init(const lgx_runtime_config_t* config);
lgx_result_t lgx_runtime_shutdown(void);
```

### Memory Management

```c
// Generic allocation
void* lgx_alloc(size_t size);
void* lgx_alloc_aligned(size_t size, size_t alignment);
void lgx_free(void* ptr);

// Intent-based allocation
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent);
void* lgx_alloc_frame(size_t size);
void* lgx_alloc_persistent(size_t size);
void* lgx_alloc_level(size_t size);

// Frame management
lgx_result_t lgx_frame_reset(void);
```

### Lifecycle Management

```c
lgx_result_t lgx_runtime_suspend(void);
lgx_result_t lgx_runtime_resume(void);
```

### Hardware Capabilities

```c
lgx_version_t lgx_runtime_get_version(void);
lgx_result_t lgx_runtime_check_compatibility(const lgx_version_t* required);
bool lgx_runtime_has_capability(lgx_capability_t cap);
lgx_hardware_status_t lgx_runtime_get_hardware_status(void);
```

### Monitoring

```c
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats);
uint64_t lgx_get_counter(lgx_counter_t counter);
void lgx_reset_counters(void);
lgx_result_t lgx_runtime_health_check(lgx_health_status_t* status);
```

### Error Handling

```c
const char* lgx_result_to_string(lgx_result_t result);
```

#### Result Codes

| Code | Description |
|------|-------------|
| `LGX_SUCCESS` | Operation completed successfully |
| `LGX_ERROR_INVALID_PARAM` | Invalid parameter provided |
| `LGX_ERROR_NOT_INITIALIZED` | Runtime not initialized |
| `LGX_ERROR_OUT_OF_MEMORY` | Memory allocation failed |
| `LGX_ERROR_INVALID_STATE` | Invalid state for operation |
| `LGX_ERROR_IO_ERROR` | I/O operation failed |
| `LGX_ERROR_UNSUPPORTED` | Feature not supported on this platform |

## Architecture

### System Overview

```
+-----------------------------------------------------------+
|  Application Layer                                        |
|  - Game Binary                                            |
|  - Links against lgx_runtime.h                           |
+-----------------------------------------------------------+
                         |
                         v
+-----------------------------------------------------------+
|  LGX Runtime Core (liblgx_runtime.so)                     |
|  +-------------+  +--------------+  +---------------+     |
|  | ABI Layer   |  | Version Mgmt |  | Capability    |     |
|  |             |  |              |  | Detection     |     |
|  +-------------+  +--------------+  +---------------+     |
|  +-------------+  +--------------+  +---------------+     |
|  | Memory Mgmt |  | Lifecycle    |  | Platform      |     |
|  |             |  | Manager      |  | Services      |     |
|  +-------------+  +--------------+  +---------------+     |
|  +-----------------------------------------------+       |
|  | Observability & Monitoring                    |       |
|  +-----------------------------------------------+       |
+-----------------------------------------------------------+
                         |
                         v
+-----------------------------------------------------------+
|  Hardware Abstraction Layer                               |
|  - GPU (Vulkan), NUMA, Huge Pages, CPU Features          |
+-----------------------------------------------------------+
```

### Memory Allocator Design

The runtime employs a hybrid allocation strategy optimized for different allocation patterns:

**Allocation Path**:
1. Hot path cache (first 512 allocations per size class): Ultra-fast (~200-800ns)
2. Lock-free pool (after cache exhaustion): Fast (~1-5μs, single atomic operation)
3. Direct malloc (if pool exhausted): Still fast (~2-8μs)

**Size Classes**: 16 optimized size classes from 16B to 4KB
**Thread-Local Caching**: Per-thread caches to eliminate contention
**Lock-Free Design**: Zero mutex locks on hot paths

## Testing

### Running Tests

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run specific test suite
./test_csf1_comparison
./test_frame_arena
./test_gpu_pool
./test_persistent_heap

# Run performance benchmarks
./perf_test_allocation_latency
./perf_test_initialization_time
./perf_test_memory_overhead

# Run with memory leak detection
ASAN_OPTIONS=detect_leaks=1 ./test_csf1_comparison
```

### Test Categories

- **Phase 0 Tests** (44 tests): Core functionality, CSF validation, hardware adaptation
- **Unit Tests** (5 tests): Component-level testing
- **Integration Tests** (1 test): End-to-end scenarios
- **Performance Tests** (5 tests): Latency, throughput, memory overhead
- **ABI Tests** (3 tests): Binary compatibility, symbol versioning
- **Failure Injection** (1 test): Chaos testing, error handling

### Continuous Integration

Automated testing on:
- Ubuntu 22.04 LTS (GCC 11, Clang 14)
- Fedora 38 (GCC 13)
- Arch Linux (Rolling, latest toolchain)

All tests must pass before merging to main branch.

## Documentation

### User Documentation

- **[Getting Started](docs/01-getting-started/README.md)**: Quick start guide and tutorials
- **[API Reference](docs/02-api-reference/README.md)**: Complete API documentation
- **[Integration Guide](docs/03-integration-guide/)**: Build system integration
- **[Architecture](docs/04-architecture/)**: System design and internals

### Developer Documentation

- **[Requirements](docs/09-development/README.md)**: Detailed requirements and acceptance criteria
- **[Design Decisions](docs/09-development/phase1-implementation/)**: Architecture and design rationale
- **[Performance](docs/05-performance/README.md)**: Optimization reports and benchmarks
- **[Testing](docs/06-testing/README.md)**: Test strategy and coverage
- **[Security](docs/07-security/README.md)**: Security model and threat analysis

### Operations Documentation

- **[Deployment](docs/08-operations/README.md)**: Production deployment guide
- **[Monitoring](docs/08-operations/telemetry-verification.md)**: Observability and metrics
- **[Troubleshooting](docs/08-operations/graceful-degradation.md)**: Common issues and solutions

## Contributing

We welcome contributions from the community. Please read our [Contributing Guidelines](CONTRIBUTING.md) before submitting pull requests.

### Development Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Write tests for your changes
4. Implement your feature
5. Ensure all tests pass (`make test`)
6. Run static analysis (`make analyze`)
7. Commit your changes (`git commit -s -m 'Add new feature'`)
8. Push to the branch (`git push origin feature/your-feature`)
9. Create a Pull Request

### Coding Standards

- **Language**: C11 with GNU extensions
- **Style**: 4-space indentation, 100-character line limit
- **Naming**: `lgx_` prefix for public API, `lgx_<subsystem>_` for internal
- **Documentation**: Doxygen-style comments for all public functions
- **Testing**: Unit tests required for all new features
- **Performance**: Benchmarks required for performance-critical code

### Code Review Requirements

All submissions require:
- Passing CI builds on all platforms (Ubuntu, Fedora, Arch)
- Code review approval from at least one maintainer
- Test coverage for new functionality
- Documentation updates for API changes
- No memory leaks (verified with AddressSanitizer)
- Performance regression check for critical paths

## License

Copyright 2026 LGX Runtime Core Contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

See [LICENSE](LICENSE) file for full license text.

## Support

### Getting Help

- **Documentation**: https://github.com/your-org/lgx-runtime-core/tree/main/docs
- **Issue Tracker**: https://github.com/your-org/lgx-runtime-core/issues
- **Discussions**: https://github.com/your-org/lgx-runtime-core/discussions
- **Mailing List**: lgx-dev@example.com

### Reporting Issues

When reporting issues, please include:
- LGX Runtime version (`lgx_runtime_get_version()`)
- Operating system and kernel version
- Hardware configuration (CPU, RAM, GPU)
- Minimal reproduction case
- Relevant log output

### Security Issues

For security-related issues, please email security@example.com instead of using the public issue tracker. See [SECURITY.md](SECURITY.md) for our security policy and responsible disclosure process.

## Acknowledgments

This project builds upon research and techniques from:
- Lock-free data structures and concurrent programming
- Memory allocator design (jemalloc, tcmalloc, mimalloc)
- Game engine architecture (Unreal Engine, Unity, id Tech)
- Linux kernel memory management
- Hardware-aware optimization techniques

Special thanks to the open-source community and all contributors.

## Project Status

**Current Release**: v1.0.0 (Production-Ready)
**Release Date**: February 12, 2026
**Maintained By**: LGX Runtime Core Team

**Production Readiness**:
- Test Coverage: 100% (59/59 tests passing)
- Memory Safety: Zero leaks, AddressSanitizer clean
- Performance: All targets exceeded
- Security: Comprehensive hardening complete
- Documentation: Technical documentation complete

**Next Release**: v1.1.0 (Planned Q2 2026)
- Enhanced NUMA support
- Additional hardware vendor optimizations
- Extended telemetry capabilities
- Performance improvements

---

For more information, visit the [project website](https://github.com/your-org/lgx-runtime-core) or join our [community chat](https://discord.gg/lgx-runtime).
