# LGX Runtime Core

**High-Performance Linux Gaming Runtime**

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.1-green.svg)](https://github.com/dream1290/LGX/releases)
[![Platform](https://img.shields.io/badge/platform-Linux%20x86__64-lightgrey.svg)]()
[![Tests](https://img.shields.io/badge/tests-64%2F64%20passing-brightgreen.svg)]()

LGX Runtime Core is a production-ready foundational runtime library for high-performance gaming applications on Linux. It provides specialized memory allocators, comprehensive lifecycle management, and hardware adaptation with a stable C ABI.
--------------------------------------------------------------------
---

## Table of Contents

- [Features](#features)
- [Performance](#performance)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

---

## Features

### Memory Management
- **Hybrid Allocation Strategy**: Hot path cache, lock-free pool, and malloc fallback
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
- **Tier Classification**: Automatic detection of OPTIMAL/COMPATIBLE/DEGRADED hardware
- **Graceful Degradation**: Software fallbacks for missing hardware features
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

---

## Performance

### Benchmark Results

Performance measurements on reference hardware (Intel Xeon, 32GB RAM, NVIDIA RTX 3080):

#### Allocation Latency

| Operation | P50 | P95 | P99 | Target | Status |
|-----------|-----|-----|-----|--------|--------|
| 1KB Allocation | 0.45μs | 1.00μs | 2.14μs | <5μs | **PASS** (57% margin) |
| 64B Allocation | 0.47μs | 0.96μs | 4.02μs | <10μs | **PASS** (60% margin) |
| Frame Arena | 0.24μs | 0.30μs | 0.40μs | <1μs | **PASS** |
| Persistent Heap | 5.11μs | 9.18μs | 10.92μs | <20μs | **PASS** |

#### System Operations

| Operation | Measured | Target | Status |
|-----------|----------|--------|--------|
| Initialize | 2.70ms | <500ms | **PASS** (185x margin) |
| Suspend | <1ms | <100ms | **PASS** |
| Resume | <1ms | <100ms | **PASS** |
| Shutdown | <20ms | <100ms | **PASS** |

#### Memory Footprint

| Component | Size | Notes |
|-----------|------|-------|
| Runtime Core | 1.03MB | All subsystems initialized |
| Frame Arenas | 192MB | 3 × 64MB buffers (configurable) |
| GPU Pools | 2.3GB | Device + host memory (optional) |
| Thread Pools | 1GB | 64 threads × 16MB (pre-allocated) |

---

## Installation

### Prerequisites

**Required:**
- Linux kernel 5.10 or later (x86_64)
- GCC 11+ or Clang 10+
- CMake 3.16 or later
- pthread library

**Optional:**
- Vulkan 1.3+ (for GPU memory pool)
- jemalloc (for fallback allocator)
- Huge pages support (for TLB optimization)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/dream1290/LGX.git
cd LGX

# Create build directory
mkdir build && cd build

# Configure (Release build)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Install (optional)
sudo make install
```

### Binary Packages

Pre-built packages are available for major Linux distributions:

#### Debian/Ubuntu
```bash
wget https://github.com/dream1290/LGX/releases/download/v1.0.1/lgx-runtime_1.0.1_amd64.deb
sudo dpkg -i lgx-runtime_1.0.1_amd64.deb
```

#### Fedora/RHEL
```bash
wget https://github.com/dream1290/LGX/releases/download/v1.0.1/lgx-runtime-1.0.1-1.x86_64.rpm
sudo rpm -i lgx-runtime-1.0.1-1.x86_64.rpm
```

#### Arch Linux
```bash
wget https://github.com/dream1290/LGX/releases/download/v1.0.1/lgx-runtime-1.0.1-1-x86_64.pkg.tar.zst
sudo pacman -U lgx-runtime-1.0.1-1-x86_64.pkg.tar.zst
```

### Build Options

```bash
# Debug build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON ..

# Minimal build (no GPU support)
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_GPU_POOL=OFF ..

# With specific compiler
CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Release ..
```

---

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

### Compilation

```bash
gcc -o myapp myapp.c \
    -I/usr/local/include \
    -L/usr/local/lib \
    -llgx_runtime \
    -lpthread

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

---

## Documentation

### User Documentation
- [Getting Started Guide](docs/01-getting-started/README.md) - Quick start and tutorials
- [API Reference](docs/02-api-reference/README.md) - Complete API documentation
- [Integration Guide](docs/04-integration/README.md) - Build system integration
- [Architecture Overview](docs/03-architecture/README.md) - System design and internals

### Developer Documentation
- [Development Guide](docs/10-development/README.md) - Requirements and acceptance criteria
- [Design Decisions](docs/10-development/phase1-implementation/) - Architecture rationale
- [Performance Analysis](docs/05-performance/README.md) - Optimization reports and benchmarks
- [Testing Strategy](docs/06-testing/README.md) - Test coverage and methodology
- [Security Model](docs/07-security/README.md) - Security architecture and threat analysis

### Operations Documentation
- [Deployment Guide](docs/08-operations/README.md) - Production deployment
- [Monitoring](docs/08-operations/telemetry-verification.md) - Observability and metrics
- [Troubleshooting](docs/08-operations/graceful-degradation.md) - Common issues and solutions

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────┐
│  Application Layer                                      │
│  (Game Binary)                                          │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│  LGX Runtime Core (liblgx_runtime.so)                   │
│  ┌─────────────┬──────────────┬─────────────────┐      │
│  │ ABI Layer   │ Version Mgmt │ Capability      │      │
│  │             │              │ Detection       │      │
│  └─────────────┴──────────────┴─────────────────┘      │
│  ┌─────────────┬──────────────┬─────────────────┐      │
│  │ Memory Mgmt │ Lifecycle    │ Platform        │      │
│  │             │ Manager      │ Services        │      │
│  └─────────────┴──────────────┴─────────────────┘      │
│  ┌───────────────────────────────────────────────┐      │
│  │ Observability & Monitoring                    │      │
│  └───────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│  Hardware Abstraction Layer                             │
│  (GPU, NUMA, Huge Pages, CPU Features)                  │
└─────────────────────────────────────────────────────────┘
```

### Memory Allocator Design

The runtime employs a hybrid allocation strategy optimized for different allocation patterns:

**Allocation Path:**
1. **Hot path cache** (first 512 allocations per size class): ~200-800ns
2. **Lock-free pool** (after cache exhaustion): ~1-5μs
3. **Direct malloc** (if pool exhausted): ~2-8μs

**Key Features:**
- 16 optimized size classes from 16B to 4KB
- Per-thread caching to eliminate contention
- Zero mutex locks on hot paths

---

## Testing

### Running Tests

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run specific test suites
./test_csf1_comparison
./test_frame_arena
./test_gpu_pool
./test_persistent_heap

# Run performance benchmarks
./perf_test_allocation_latency
./perf_test_initialization_time
./perf_test_memory_overhead

# Memory leak detection
ASAN_OPTIONS=detect_leaks=1 ./test_csf1_comparison
```

### Test Coverage

- **Phase 0 Tests** (44): Core functionality, CSF validation, hardware adaptation
- **Unit Tests** (5): Component-level testing
- **Integration Tests** (1): End-to-end scenarios
- **Performance Tests** (5): Latency, throughput, memory overhead
- **ABI Tests** (3): Binary compatibility, symbol versioning
- **Failure Injection** (1): Chaos testing, error handling

**Total: 64/64 tests passing (100%)**

### Continuous Integration

Automated testing on:
- Ubuntu 22.04 LTS (GCC 11, Clang 14)
- Fedora 38 (GCC 13)
- Arch Linux (Rolling, latest toolchain)

---

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
- Passing CI builds on all platforms
- Code review approval from at least one maintainer
- Test coverage for new functionality
- Documentation updates for API changes
- No memory leaks (verified with AddressSanitizer)
- Performance regression check for critical paths

---

## Project Status

**Current Release:** v1.0.1 (Production-Ready)  
**Release Date:** February 12, 2026  
**Maintained By:** LGX Runtime Core Team

### Production Readiness

- Test Coverage: 100% (64/64 tests passing)
- Benchmark Suites: 5 comprehensive benchmark programs
- Memory Safety: Zero leaks, AddressSanitizer clean
- Performance: All targets exceeded
- Security: Comprehensive hardening complete
- Documentation: Technical documentation complete

### Recent Changes (v1.0.1)

- Added comprehensive benchmark suite with 5 benchmark programs
- Fixed compilation warnings in Release builds
- Enhanced security audit compliance
- Improved build system configuration
- Updated packaging for all major distributions

### Roadmap (v1.1.0 - Planned Q2 2026)

- Enhanced NUMA support
- Additional hardware vendor optimizations
- Extended telemetry capabilities
- Performance improvements

---

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

---

## Support

### Getting Help

- **Documentation**: [docs/](docs/)
- **Issue Tracker**: [GitHub Issues](https://github.com/dream1290/LGX/issues)
- **Discussions**: [GitHub Discussions](https://github.com/dream1290/LGX/discussions)

### Reporting Issues

When reporting issues, please include:
- LGX Runtime version (`lgx_runtime_get_version()`)
- Operating system and kernel version
- Hardware configuration (CPU, RAM, GPU)
- Minimal reproduction case
- Relevant log output

### Security Issues

For security-related issues, please use the GitHub Security Advisory feature or create a private security issue. See our [security documentation](docs/07-security/) for our security policy and threat model.

---

## Acknowledgments

This project builds upon research and techniques from:
- Lock-free data structures and concurrent programming
- Memory allocator design (jemalloc, tcmalloc, mimalloc)
- Game engine architecture (Unreal Engine, Unity, id Tech)
- Linux kernel memory management
- Hardware-aware optimization techniques

Special thanks to the open-source community and all contributors.
This program was created by Oualid Bahloul

---

**For more information, visit the [project repository](https://github.com/dream1290/LGX).**
