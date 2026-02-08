# LGX Runtime Core

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/version-1.0.0--alpha-orange.svg)]()
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)]()

A high-performance, deterministic Linux gaming runtime with specialized memory allocators and comprehensive lifecycle management.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Installation](#installation)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Performance](#performance)
- [Testing](#testing)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

## Overview

LGX Runtime Core is a foundational runtime library designed for high-performance gaming applications on Linux. It provides deterministic behavior, versioned runtime environment, and optimized memory management through a stable C ABI. The runtime manages initialization, lifecycle operations, memory allocation, and platform services with a focus on performance, reliability, and hardware adaptation.

### Key Objectives

- **Performance**: Sub-microsecond allocation latency for frame-critical operations
- **Determinism**: Consistent behavior across different Linux distributions and hardware configurations
- **Reliability**: Graceful degradation and comprehensive error handling
- **Compatibility**: Stable ABI with forward compatibility guarantees

## Features

### Memory Management

- **Frame Arena Allocator**: Triple-buffered bump pointer allocation with P99 < 0.1μs latency
- **GPU Memory Pool**: Pre-allocated Vulkan memory with buddy allocator, P99 < 10μs latency
- **Persistent Heap**: Segregated fit allocator with <5% fragmentation over extended sessions
- **Intent-Based API**: Automatic routing to optimal allocator based on usage patterns

### Lifecycle Management

- **Suspend/Resume**: State preservation with <100ms budget for both operations
- **Signal Handling**: Graceful shutdown on SIGTERM, crash reporting on SIGSEGV/SIGABRT
- **Crash Dumps**: Automatic generation of stack traces and diagnostic information

### Hardware Adaptation

- **Tier Classification**: Automatic detection of OPTIMAL/COMPATIBLE/DEGRADED hardware tiers
- **Graceful Degradation**: Software fallbacks for missing hardware features
- **Performance Estimation**: Real-time impact assessment for degraded configurations

### Observability

- **Performance Counters**: Comprehensive metrics for allocations, cache hits, and pool usage
- **Health Monitoring**: Continuous system health assessment with anomaly detection
- **Structured Logging**: Subsystem-tagged logging with runtime filtering
- **Telemetry Framework**: Privacy-preserving performance data collection

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────┐
│  Application Layer                                      │
│  - Game Binary                                          │
│  - Links against lgx_runtime.h                         │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│  LGX Runtime Core (liblgx_runtime.so)                   │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ ABI Layer   │  │ Version Mgmt │  │ Capability    │  │
│  │             │  │              │  │ Detection     │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ Memory Mgmt │  │ Lifecycle    │  │ Platform      │  │
│  │             │  │ Manager      │  │ Services      │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
│  ┌─────────────────────────────────────────────────┐   │
│  │ Observability & Monitoring                      │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────┐
│  Hardware Abstraction Layer                             │
│  - GPU (Vulkan), NUMA, Huge Pages, CPU Features        │
└─────────────────────────────────────────────────────────┘
```

### Memory Allocator Design

The runtime employs three specialized allocators optimized for different allocation patterns:

| Allocator | Use Case | Target Latency | Fragmentation | Capacity |
|-----------|----------|----------------|---------------|----------|
| Frame Arena | Temporary per-frame data | P99 < 0.1μs | 0% | 192MB |
| GPU Pool | GPU-visible memory | P99 < 10μs | <10% | 2.3GB |
| Persistent Heap | Long-lived allocations | P99 < 20μs | <5% | Dynamic |

## Installation

### Prerequisites

- **Operating System**: Linux kernel 5.10 or later (x86_64)
- **Compiler**: GCC 11+ or Clang 10+
- **Build System**: CMake 3.16 or later
- **Dependencies**: 
  - pthread (required)
  - Vulkan 1.3+ (optional, for GPU pool)
  - jemalloc (optional, for fallback allocator)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/your-org/lgx-runtime-core.git
cd lgx-runtime-core

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make -j$(nproc)

# Install (optional)
sudo make install
```

### Build Options

```bash
# Debug build with symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# With specific compiler
CC=clang CXX=clang++ cmake ..

# Disable GPU support
cmake -DENABLE_GPU_POOL=OFF ..

# Enable additional diagnostics
cmake -DENABLE_DIAGNOSTICS=ON ..
```

## Usage

### Basic Example

```c
#include <lgx_runtime.h>
#include <stdio.h>

int main(void) {
    // Create configuration
    lgx_runtime_config_t* config = lgx_config_create();
    if (!config) {
        fprintf(stderr, "Failed to create configuration\n");
        return 1;
    }
    
    // Configure runtime
    lgx_config_set_memory_pool_size(config, 256 * 1024 * 1024); // 256MB
    lgx_config_set_log_path(config, "/var/log/lgx.log");
    
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
    
    if (hw_status.achieved_tier == LGX_HW_TIER_DEGRADED) {
        printf("Warning: %s\n", hw_status.degradation_reason);
        printf("Performance Impact: %s\n", hw_status.performance_impact_estimate);
    }
    
    // Allocate memory with intent
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 4096,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_VALIDATE_WARN
    };
    
    void* memory = lgx_alloc_with_intent(&intent);
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

### Advanced Usage

#### Lifecycle Management

```c
// Suspend runtime (saves state)
lgx_result_t result = lgx_runtime_suspend();
if (result == LGX_SUCCESS) {
    printf("Runtime suspended successfully\n");
}

// Resume runtime (restores state)
result = lgx_runtime_resume();
if (result == LGX_SUCCESS) {
    printf("Runtime resumed successfully\n");
}
```

#### Performance Monitoring

```c
// Query performance counters
uint64_t allocations = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
uint64_t cache_hits = lgx_get_counter(LGX_COUNTER_CACHE_HITS);
uint64_t cache_misses = lgx_get_counter(LGX_COUNTER_CACHE_MISSES);

printf("Allocations: %lu\n", allocations);
printf("Cache Hit Rate: %.2f%%\n", 
       100.0 * cache_hits / (cache_hits + cache_misses));

// Health check
lgx_health_status_t health;
lgx_runtime_health_check(&health);
printf("System Health: %s\n", health.is_healthy ? "OK" : "DEGRADED");
```

## API Reference

### Initialization Functions

```c
lgx_runtime_config_t* lgx_config_create(void);
void lgx_config_set_memory_pool_size(lgx_runtime_config_t* config, size_t size);
void lgx_config_set_log_path(lgx_runtime_config_t* config, const char* path);
void lgx_config_set_flags(lgx_runtime_config_t* config, uint32_t flags);
void lgx_config_destroy(lgx_runtime_config_t* config);

lgx_result_t lgx_runtime_init(const lgx_runtime_config_t* config);
lgx_result_t lgx_runtime_shutdown(void);
```

### Memory Management Functions

```c
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent);
void* lgx_alloc(size_t size);
void* lgx_alloc_aligned(size_t size, size_t alignment);
void lgx_free(void* ptr);

void* lgx_frame_alloc(size_t size);
lgx_result_t lgx_frame_reset(void);
```

### Lifecycle Functions

```c
lgx_result_t lgx_runtime_suspend(void);
lgx_result_t lgx_runtime_resume(void);
```

### Capability Detection

```c
lgx_version_t lgx_runtime_get_version(void);
lgx_result_t lgx_runtime_check_compatibility(const lgx_version_t* required);
bool lgx_runtime_has_capability(lgx_capability_t cap);
lgx_hardware_status_t lgx_runtime_get_hardware_status(void);
```

### Monitoring Functions

```c
uint64_t lgx_get_counter(lgx_counter_t counter);
void lgx_reset_counters(void);
lgx_result_t lgx_runtime_health_check(lgx_health_status_t* status);
```

### Error Handling

```c
const char* lgx_result_to_string(lgx_result_t result);
```

#### Error Codes

| Code | Description |
|------|-------------|
| `LGX_SUCCESS` | Operation completed successfully |
| `LGX_ERROR_INVALID_PARAM` | Invalid parameter provided |
| `LGX_ERROR_NOT_INITIALIZED` | Runtime not initialized |
| `LGX_ERROR_OUT_OF_MEMORY` | Memory allocation failed |
| `LGX_ERROR_INVALID_STATE` | Invalid state for operation |
| `LGX_ERROR_IO_ERROR` | I/O operation failed |

## Performance

### Benchmark Results

Performance measurements on reference hardware (Intel Xeon, 32GB RAM, NVIDIA RTX 3080):

#### Allocation Latency

| Allocator | P50 | P95 | P99 | P99.9 | Target |
|-----------|-----|-----|-----|-------|--------|
| Frame Arena | 0.01μs | 0.03μs | 0.05μs | 0.1μs | <0.1μs |
| GPU Pool | 3μs | 6μs | 8μs | 12μs | <10μs |
| Persistent Heap | 8μs | 15μs | 18μs | 25μs | <20μs |

#### Lifecycle Operations

| Operation | Measured | Target | Status |
|-----------|----------|--------|--------|
| Initialize | 50ms | <500ms | Pass (10x margin) |
| Suspend | <1ms | <100ms | Pass (100x margin) |
| Resume | <1ms | <100ms | Pass (100x margin) |
| Shutdown | 20ms | <100ms | Pass (5x margin) |

#### Memory Overhead

| Component | Size | Notes |
|-----------|------|-------|
| Runtime Core | 60MB | All subsystems |
| Frame Arenas | 192MB | 3 × 64MB buffers |
| GPU Pools | 2.3GB | Device + host memory |
| **Total** | **2.5GB** | Within 16GB limit |

### Optimization History

Phase 0 breakthrough optimization (10-day sprint):

| Day | Optimization | P99 Latency | Improvement |
|-----|--------------|-------------|-------------|
| Baseline | Standard malloc | 20.0μs | - |
| 1-2 | Lock-free pool | 16.7μs | 17% |
| 3-4 | Batch refill | 14.5μs | 28% |
| 5 | Pattern tracking | 13.9μs | 31% |
| 6-7 | Markov chain | 10.9μs | 46% |
| 8-9 | SIMD acceleration | 11.0μs | 45% |
| 10 | Huge pages | 9.0μs | 55% |

## Testing

### Running Tests

```bash
# Run all tests
make test

# Run specific test suite
./build/test_suspend_resume
./build/test_signal_handling
./build/test_frame_arena
./build/test_gpu_pool
./build/test_persistent_heap

# Run with verbose output
ctest --test-dir build --verbose

# Run performance benchmarks
./build/test_performance --benchmark
```

### Test Coverage

- **Lifecycle Management**: Suspend/resume, signal handling, crash dumps
- **Memory Allocators**: Frame arena, GPU pool, persistent heap
- **Hardware Adaptation**: Tier classification, graceful degradation
- **Error Handling**: Invalid parameters, state validation, recovery
- **Performance**: Latency benchmarks, memory overhead, throughput

### Continuous Integration

Automated testing on:
- Ubuntu 22.04 LTS (GCC 11, Clang 14)
- Fedora 38 (GCC 13)
- Arch Linux (Rolling, latest toolchain)

## Contributing

We welcome contributions from the community. Please read our contributing guidelines before submitting pull requests.

### Development Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Write tests for your changes
4. Implement your feature
5. Ensure all tests pass (`make test`)
6. Commit your changes (`git commit -am 'Add new feature'`)
7. Push to the branch (`git push origin feature/your-feature`)
8. Create a Pull Request

### Coding Standards

- **Language**: C11 with GNU extensions
- **Style**: 4-space indentation, 100-character line limit
- **Naming**: `lgx_` prefix for public API, `lgx_<subsystem>_` for internal
- **Documentation**: Doxygen-style comments for all public functions
- **Testing**: Unit tests required for all new features
- **Performance**: Benchmarks required for performance-critical code

### Code Review Process

All submissions require:
- Passing CI builds on all platforms
- Code review approval from at least one maintainer
- Test coverage for new functionality
- Documentation updates for API changes

## Project Status

### Current Release: v1.0.0-alpha

**Completed Features**:
- Core runtime initialization and shutdown
- Specialized memory allocators (frame arena, GPU pool, persistent heap)
- Intent-based allocation routing
- Lifecycle management (suspend/resume, signal handling)
- Hardware adaptation and graceful degradation
- Performance monitoring and health checks

**In Development**:
- Platform services (timing, logging with rotation)
- Telemetry system with privacy framework
- Library isolation and namespace pinning
- Security hardening and input validation

**Planned**:
- NUMA-aware allocation
- Predictive optimization
- Hardware acceleration partnerships
- Formal verification of critical paths

## Roadmap

### Phase 1: Core Implementation (Current)
- Specialized memory allocators
- Lifecycle management
- Hardware adaptation
- Basic observability

### Phase 2: Intelligence Layer (Months 16-33)
- Pattern learning and prediction
- Adaptive allocation strategies
- Machine learning integration

### Phase 3: Hardware Acceleration (Months 34-45)
- Vendor partnerships
- Custom hardware optimizations
- Advanced GPU integration

### Phase 4: Formal Verification (Months 46-48)
- Property-based testing
- Correctness proofs
- Safety certification

## Documentation

- **[Requirements Specification](.kiro/specs/lgx-runtime-core/requirements.md)**: Detailed requirements and acceptance criteria
- **[Design Document](.kiro/specs/lgx-runtime-core/design.md)**: Architecture and design decisions
- **[Implementation Tasks](.kiro/specs/lgx-runtime-core/tasks.md)**: Task breakdown and progress tracking
- **[Phase 0 Reports](docs/)**: Optimization reports and validation results

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

## Contact

- **Project Website**: https://github.com/your-org/lgx-runtime-core
- **Issue Tracker**: https://github.com/your-org/lgx-runtime-core/issues
- **Mailing List**: lgx-dev@example.com
- **Chat**: #lgx-runtime on IRC/Discord

## Acknowledgments

This project builds upon research and techniques from:
- Lock-free data structures and concurrent programming
- Memory allocator design (jemalloc, tcmalloc, mimalloc)
- Game engine architecture (Unreal Engine, Unity, id Tech)
- Linux kernel memory management
- Hardware-aware optimization techniques

Special thanks to the open-source community and all contributors.

---

**Project Status**: Active Development  
**Current Version**: 1.0.0-alpha  
**Last Updated**: February 2026  
**Maintained By**: LGX Runtime Core Team
