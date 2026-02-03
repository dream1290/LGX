# LGX Runtime Core - Requirements

## 1. Feature Overview

The LGX Runtime Core is the foundational layer that provides a deterministic, versioned runtime environment for games targeting the LGX platform. It manages the ABI boundary, lifecycle, and core platform services that games depend on.

**Revolutionary Enhancement**: The Runtime Core uses a **Telescoping Architecture** that delivers value incrementally across 4 layers over 48 months:

- **Layer 1 (Months 1-12)**: Determinism Engine - Mathematical performance guarantees
- **Layer 2 (Months 13-24)**: Intelligence Layer - Predictive optimization via pattern learning
- **Layer 3 (Months 25-36)**: Hardware Revolution - Direct hardware access via vendor partnerships
- **Layer 4 (Months 37-48)**: Formal Guarantees - Mathematical verification of critical components

**Key Innovation**: Intent-based API that captures **what** and **why** you need memory, not just **how much**. This enables future layers without breaking ABI.

See `REVOLUTIONARY_ARCHITECTURE.md` for complete vision.

## 2. User Stories

### 2.1 Game Developer Stories

**US-1: As a game developer, I want to initialize the LGX runtime with a single function call so that I can quickly integrate LGX into my game.**

**US-2: As a game developer, I want to query the LGX runtime version and capabilities so that I can adapt my game's behavior based on available features.**

**US-3: As a game developer, I want the runtime to manage memory pools efficiently so that I don't have to worry about Linux-specific memory management.**

**US-4: As a game developer, I want clear lifecycle hooks (init, suspend, resume, shutdown) so that I can properly manage resources across different runtime states.**

**US-5: As a game developer, I want the runtime to provide platform services (filesystem, timing, logging) through a stable ABI so that my code works consistently across Linux distributions.**

### 2.2 Platform Engineer Stories

**US-6: As a platform engineer, I want the runtime to enforce ABI stability across minor versions so that games don't break when users update LGX.**

**US-7: As a platform engineer, I want the runtime to use pinned library versions so that behavior is deterministic across different Linux distributions.**

**US-8: As a platform engineer, I want the runtime to have minimal overhead (<200MB memory, <5% CPU) so that it doesn't impact game performance.**

**US-9: As a platform engineer, I want the runtime to provide telemetry hooks (opt-in) so that I can monitor performance and reliability across the fleet.**

## 3. Acceptance Criteria

### 3.1 Functional Requirements

**AC-1: Runtime Initialization**
- The runtime SHALL provide an `lgx_runtime_init()` function that initializes all core services
- Initialization SHALL complete in <500ms on reference hardware
- Initialization SHALL fail gracefully with clear error codes if requirements are not met
- The runtime SHALL validate that all pinned libraries are present and at correct versions

**AC-2: Version Negotiation**
- The runtime SHALL provide `lgx_runtime_get_version()` returning major.minor.patch version
- The runtime SHALL provide `lgx_runtime_check_compatibility(required_version)` to validate game requirements
- The runtime SHALL support games built for any minor version within the same major version
- The runtime SHALL reject games requiring a newer major version with a clear error message

**AC-3: Capability Detection**
- The runtime SHALL provide `lgx_runtime_query_capabilities()` to enumerate available features
- Capabilities SHALL include: DX translation level, security module availability, GPU vendor, driver version
- The runtime SHALL allow games to query individual capabilities by name

**AC-4: Memory Management**
- The runtime SHALL provide memory pool allocation functions: `lgx_alloc()`, `lgx_free()`
- The runtime SHALL use separate pools for short-lived and long-lived allocations
- The runtime SHALL maintain resident memory usage <200MB for core services
- The runtime SHALL support huge pages (2MB) when available, with fallback to standard pages

**AC-5: Lifecycle Management**
- The runtime SHALL provide lifecycle hooks: `lgx_runtime_suspend()`, `lgx_runtime_resume()`, `lgx_runtime_shutdown()`
- Suspend SHALL complete in <100ms and preserve all critical state
- Resume SHALL restore state and resume operations in <100ms
- Shutdown SHALL clean up all resources and terminate gracefully

**AC-6: Platform Services**
- The runtime SHALL provide filesystem abstraction: `lgx_fs_open()`, `lgx_fs_read()`, `lgx_fs_write()`, `lgx_fs_close()`
- The runtime SHALL provide timing services: `lgx_time_now()`, `lgx_time_sleep()`
- The runtime SHALL provide logging: `lgx_log(level, message)`
- All platform services SHALL use the pinned library environment

**AC-7: Telemetry (Opt-in)**
- The runtime SHALL provide `lgx_telemetry_enable(opt_in)` for user consent
- When enabled, the runtime SHALL collect: frame-time distribution, memory usage, crash events
- Telemetry data SHALL be anonymized (no user data, file paths, or process names)
- The runtime SHALL provide `lgx_telemetry_export()` to retrieve collected data

### 3.2 Non-Functional Requirements

**AC-8: Performance**
- The runtime SHALL contribute <0.5ms to p99 frame-time variance
- The runtime SHALL have <5% CPU overhead in steady-state operation
- The runtime SHALL have <200MB resident memory footprint
- The runtime SHALL have zero heap allocations in frame-critical paths

**AC-9: Reliability**
- The runtime SHALL achieve >99.9% crash-free rate (measured over 1M game hours)
- The runtime SHALL handle all error conditions gracefully without crashing
- The runtime SHALL provide detailed error messages for all failure modes
- The runtime SHALL validate all inputs and reject invalid parameters

**AC-10: ABI Stability**
- The runtime SHALL maintain binary compatibility across minor versions (X.Y → X.Y+1)
- The runtime SHALL use symbol versioning to support multiple ABI versions simultaneously
- The runtime SHALL never remove or change existing function signatures within a major version
- The runtime SHALL deprecate functions with 2 major version notice (minimum 24 months)

**AC-11: Determinism**
- The runtime SHALL produce identical behavior given identical inputs across different Linux distributions
- The runtime SHALL use pinned versions of: glibc 2.35+, libstdc++, Vulkan loader 1.3.x
- The runtime SHALL isolate the game from host system libraries using namespaces
- The runtime SHALL validate library versions at startup and fail if mismatches detected

**AC-12: Security**
- The runtime SHALL validate all function pointers before invocation
- The runtime SHALL use memory-safe practices (bounds checking, no buffer overflows)
- The runtime SHALL provide secure random number generation via `lgx_random()`
- The runtime SHALL integrate with the LGX security module for attestation

**AC-13: Resource Limits (DoS Prevention)**
- The runtime SHALL enforce maximum memory pool size of 16GB per game instance
- The runtime SHALL limit open file handles to 1024
- The runtime SHALL limit log file size to 100MB with automatic rotation
- The runtime SHALL limit telemetry buffer to 10MB
- The runtime SHALL rate-limit allocations to 1M per second

**AC-14: Error Context and Debugging**
- The runtime SHALL provide `lgx_get_last_error()` returning thread-local error details
- The runtime SHALL provide `lgx_set_error_handler()` for custom error callbacks
- The runtime SHALL maintain error context stack showing where errors occurred
- The runtime SHALL distinguish between assertions (programming errors) and recoverable errors

**AC-15: Observability Integration**
- The runtime SHALL integrate with Linux perf for CPU profiling
- The runtime SHALL integrate with ftrace for kernel-level tracing
- The runtime SHALL provide performance counter API exposing allocation counts, cache hit rates
- The runtime SHALL provide health check API: `lgx_runtime_health_check()`

## 4. Technical Constraints

### 4.1 Platform Requirements
- Linux kernel 5.10+ (for namespace and cgroup support)
- x86_64 architecture (ARM64 future consideration)
- glibc 2.35+ or compatible
- Vulkan 1.3+ capable GPU and drivers

### 4.2 Dependencies
- Pinned glibc 2.35+
- Pinned libstdc++ (GCC 11+)
- Pinned Vulkan loader 1.3.x
- Optional: systemd for service management

### 4.3 Performance Budgets
- Memory: <200MB resident for runtime core
- CPU: <5% overhead in steady-state
- Latency: <0.5ms p99 frame-time contribution
- Startup: <500ms initialization time (breakdown: library loading <100ms, memory pools <50ms, GPU detection <100ms, buffer <250ms)

### 4.4 Resource Limits
- Maximum memory pool size: 16GB per game instance
- Maximum open file handles: 1024
- Maximum log file size: 100MB (with rotation)
- Maximum telemetry buffer: 10MB
- Allocation rate limit: 1M allocations per second (DoS prevention)

## 5. Failure Mode Analysis

### 5.1 Initialization Failures
**Scenario**: `lgx_runtime_init()` fails due to missing pinned libraries
- **Behavior**: Return `LGX_ERROR_LIBRARY_VERSION_MISMATCH` with detailed error message
- **Recovery**: None - game must not proceed
- **Logging**: Log exact library versions found vs expected

**Scenario**: `lgx_runtime_init()` fails due to insufficient memory
- **Behavior**: Return `LGX_ERROR_OUT_OF_MEMORY`
- **Recovery**: None - system cannot support LGX
- **Logging**: Log requested vs available memory

**Scenario**: GPU driver not responding during init
- **Behavior**: Return `LGX_ERROR_GPU_UNAVAILABLE` after 5-second timeout
- **Recovery**: None - game requires GPU
- **Logging**: Log GPU vendor, driver version, error details

### 5.2 Runtime Failures
**Scenario**: `lgx_alloc()` fails mid-frame due to pool exhaustion
- **Behavior**: Return NULL, set last error to `LGX_ERROR_OUT_OF_MEMORY`
- **Recovery**: Game must handle NULL return and degrade gracefully
- **Logging**: Log allocation size, pool state, total memory usage

**Scenario**: GPU driver stops responding during gameplay
- **Behavior**: Detect via timeout (5 seconds), trigger telemetry event
- **Recovery**: Attempt driver reset (if supported), otherwise fail gracefully
- **Logging**: Log GPU state, last successful operation, driver version

**Scenario**: File I/O fails due to disk full
- **Behavior**: Return `LGX_ERROR_IO_ERROR`, set last error with details
- **Recovery**: Game must handle error (disable logging, use memory buffer)
- **Logging**: Log filesystem path, error code, available space

### 5.3 Resource Exhaustion
**Scenario**: Game attempts to allocate beyond resource limits
- **Behavior**: Return error, do not crash
- **Recovery**: Game must reduce memory usage or fail gracefully
- **Logging**: Log limit exceeded, current usage, requested amount

## 6. Out of Scope

- DX→Vulkan translation (separate component)
- Security/anti-cheat module (separate component)
- Audio/input platform shims (separate component)
- Shader compilation and pipeline caching (separate component)
- Distribution and packaging tools (separate component)

## 7. Open Questions

1. Should we support multiple concurrent LGX runtime versions on the same system?
2. What is the strategy for handling library conflicts with system-installed versions?
3. How do we handle GPU driver updates that might break compatibility?
4. Should telemetry be enabled by default with opt-out, or disabled by default with opt-in?
5. What is the upgrade path for games when a new major version breaks ABI?
6. Should memory allocation patterns be analyzed at runtime to optimize pool sizes dynamically?
7. What is the privilege level required for Runtime Core? (root, capabilities, unprivileged?)
8. Should we use a separate process for telemetry to prevent games from bypassing opt-out?

## 8. Success Metrics

- Time to integrate LGX runtime into existing game: <2 hours
- Runtime initialization time: <500ms (measured on reference hardware)
- Memory overhead: <200MB (validated via prototype)
- CPU overhead: <5% (measured under realistic game load)
- Frame-time contribution: <0.5ms p99 (measured with instrumentation)
- Crash-free rate: >99.9% (measured over 1M game hours)
- ABI compatibility: 100% within major version (validated via compatibility tests)
- Allocation latency: <1μs for cached allocations (measured via microbenchmarks)
