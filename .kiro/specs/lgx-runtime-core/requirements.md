# LGX Runtime Core - Requirements

## 1. Feature Overview

The LGX Runtime Core is the foundational layer that provides a deterministic, versioned runtime environment for games targeting the LGX platform. It manages the ABI boundary, lifecycle, and core platform services that games depend on.

**Revolutionary Enhancement**: The Runtime Core uses a **Telescoping Architecture** that delivers value incrementally across 4 layers over 48 months:

- **Layer 1 (Months 1-15)**: Determinism Engine - Probabilistic performance guarantees with graceful degradation
- **Layer 2 (Months 16-33)**: Intelligence Layer - Predictive optimization via pattern learning
- **Layer 3 (Months 34-45)**: Hardware Revolution - Best-effort hardware acceleration with software fallbacks
- **Layer 4 (Months 46-48)**: Formal Guarantees - Selective verification of critical components

**Timeline Adjustment**: Based on engineering analysis, Phase 1 extended to 15 months to account for lock-free allocator complexity, NUMA implementation challenges, and comprehensive testing requirements.

**Key Innovation**: Intent-based API that captures **what** and **why** you need memory, not just **how much**. This enables future layers without breaking ABI.

See `REVOLUTIONARY_ARCHITECTURE.md` for complete vision.

## 2. User Stories

### 2.1 Game Developer Stories

**US-1: As a game developer, I want to initialize the LGX runtime with a single function call so that I can quickly integrate LGX into my game.**

**US-2: As a game developer, I want to query the LGX runtime version and capabilities so that I can adapt my game's behavior based on available features.**

**US-3: As a game developer, I want a frame arena allocator that gives me sub-microsecond allocations for per-frame temporary data so that I can allocate freely without worrying about performance.**

**US-4: As a game developer, I want a GPU memory pool that pre-allocates and manages GPU-visible memory so that I don't have to deal with complex GPU memory management.**

**US-5: As a game developer, I want a persistent heap allocator that handles long-lived allocations without fragmenting so that my game doesn't crash after hours of gameplay.**

**US-6: As a game developer, I want clear lifecycle hooks (init, suspend, resume, shutdown) so that I can properly manage resources across different runtime states.**

**US-7: As a game developer, I want the runtime to provide platform services (filesystem, timing, logging) through a stable ABI so that my code works consistently across Linux distributions.**

### 2.2 Platform Engineer Stories

**US-6: As a platform engineer, I want the runtime to enforce ABI stability across minor versions so that games don't break when users update LGX.**

**US-7: As a platform engineer, I want the runtime to use pinned library versions so that behavior is deterministic across different Linux distributions.**

**US-8: As a platform engineer, I want the runtime to have minimal overhead with tiered performance targets so that I can choose appropriate performance/resource tradeoffs.**

**US-9: As a platform engineer, I want the runtime to provide comprehensive telemetry with strong privacy guarantees so that I can monitor performance while respecting user privacy.**

**US-10: As a platform engineer, I want the runtime to gracefully degrade when hardware features are unavailable so that games work across diverse hardware configurations.**

## 3. Acceptance Criteria

### 3.1 Functional Requirements

**AC-1: Runtime Initialization**
- The runtime SHALL provide an `lgx_runtime_init()` function that initializes all core services
- Initialization SHALL complete in <1000ms on reference hardware (Tier 1), <500ms target (Tier 2)
- Initialization SHALL fail gracefully with clear error codes and recovery guidance if requirements are not met
- The runtime SHALL validate that all pinned libraries are present and at correct versions
- The runtime SHALL support graceful degradation when optional features are unavailable

**AC-2: Version Negotiation**
- The runtime SHALL provide `lgx_runtime_get_version()` returning major.minor.patch version
- The runtime SHALL provide `lgx_runtime_check_compatibility(required_version)` to validate game requirements
- The runtime SHALL support games built for any minor version within the same major version
- The runtime SHALL reject games requiring a newer major version with a clear error message

**AC-3: Capability Detection and Hardware Adaptation**
- The runtime SHALL provide `lgx_runtime_query_capabilities()` to enumerate available features
- Capabilities SHALL include: DX translation level, security module availability, GPU vendor, driver version, hardware tier
- The runtime SHALL allow games to query individual capabilities by name
- The runtime SHALL provide hardware tier classification (OPTIMAL, COMPATIBLE, DEGRADED) with fallback strategies
- The runtime SHALL report which features are degraded and provide remediation guidance

**AC-4: Specialized Memory Allocators**

**Frame Arena Allocator:**
- The runtime SHALL provide a frame arena allocator: `lgx_frame_alloc()`, `lgx_frame_reset()`
- Frame allocations SHALL use bump pointer allocation (O(1), no free list traversal)
- Frame allocations SHALL achieve P99 < 0.1 μs (100 nanoseconds)
- Frame arena SHALL support triple-buffering (3-frame rotation) to prevent use-after-free
- Frame arena SHALL automatically reset at frame boundaries
- Frame arena SHALL handle 80% of typical game allocations

**GPU Memory Pool:**
- The runtime SHALL provide GPU memory pool: `lgx_gpu_alloc()`, `lgx_gpu_free()`
- GPU allocations SHALL use pre-allocated GPU-visible memory (no runtime allocation)
- GPU allocations SHALL achieve P99 < 10 μs
- GPU pool SHALL support different memory types (device-local, host-visible, host-cached)
- GPU pool SHALL handle memory alignment requirements (256-byte for buffers, 4KB for images)
- GPU pool SHALL integrate with Vulkan memory management

**Persistent Heap Allocator:**
- The runtime SHALL provide persistent heap: `lgx_heap_alloc()`, `lgx_heap_free()`
- Persistent heap SHALL use buddy allocator or segregated fit to minimize fragmentation
- Persistent heap SHALL achieve P99 < 20 μs
- Persistent heap SHALL handle long-lived allocations (level data, assets, caches)
- Persistent heap SHALL support defragmentation during loading screens
- Persistent heap SHALL maintain <5% fragmentation over 8-hour gameplay sessions

**Unified API:**
- The runtime SHALL provide intent-based allocation: `lgx_alloc_with_intent(intent)`
- Intent SHALL specify lifetime (FRAME, LEVEL, SESSION) and usage (CPU, GPU, SHARED)
- Runtime SHALL automatically route to appropriate allocator based on intent
- The runtime SHALL maintain resident memory usage <300MB for core services (Tier 1), <200MB target (Tier 2)

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

**AC-7: Telemetry (Opt-in with Privacy Framework)**
- The runtime SHALL provide `lgx_telemetry_enable(opt_in)` for explicit user consent
- When enabled, the runtime SHALL collect: frame-time distribution, memory usage, allocation patterns, crash events
- Telemetry data SHALL be anonymized with formal privacy policy (no user data, file paths, or process names)
- The runtime SHALL provide `lgx_telemetry_export()` to retrieve collected data for transparency
- The runtime SHALL support adaptive sampling to prevent buffer overflow and maintain <1% CPU overhead
- The runtime SHALL provide correlation analysis linking performance issues to root causes

### 3.2 Non-Functional Requirements

**AC-8: Performance (Tiered Targets)**

**Tier 1 - Minimum Viable Product:**
- Frame arena SHALL achieve P99 < 0.5 μs (500 nanoseconds)
- GPU pool SHALL achieve P99 < 20 μs
- Persistent heap SHALL achieve P99 < 50 μs
- The runtime SHALL have <10% CPU overhead in steady-state operation
- The runtime SHALL have <300MB resident memory footprint

**Tier 2 - Competitive Product (Target):**
- Frame arena SHALL achieve P99 < 0.1 μs (100 nanoseconds)
- GPU pool SHALL achieve P99 < 10 μs
- Persistent heap SHALL achieve P99 < 20 μs
- The runtime SHALL have <5% CPU overhead in steady-state operation
- The runtime SHALL have <200MB resident memory footprint

**Tier 3 - Best-in-Class (Aspirational):**
- Frame arena SHALL achieve P99 < 0.05 μs (50 nanoseconds)
- GPU pool SHALL achieve P99 < 5 μs
- Persistent heap SHALL achieve P99 < 10 μs
- The runtime SHALL have <2% CPU overhead in steady-state operation
- The runtime SHALL have <100MB resident memory footprint

**Decision Framework:** Ship when Tier 1 met, market as competitive when Tier 2 met

**AC-9: Reliability and Error Recovery**
- The runtime SHALL achieve >99% crash-free rate (Tier 1), >99.9% target (Tier 2) measured over 1M game hours
- The runtime SHALL handle all error conditions gracefully without crashing
- The runtime SHALL provide detailed error messages with recovery guidance for all failure modes
- The runtime SHALL validate all inputs and reject invalid parameters with actionable error context
- The runtime SHALL provide structured error recovery recommendations (retry, degrade, abort, shutdown)

**AC-10: ABI Stability**
- The runtime SHALL maintain binary compatibility across minor versions (X.Y → X.Y+1)
- The runtime SHALL use symbol versioning to support multiple ABI versions simultaneously
- The runtime SHALL never remove or change existing function signatures within a major version
- The runtime SHALL deprecate functions with 2 major version notice (minimum 24 months)

**AC-11: Determinism and Predictability**
- The runtime SHALL produce identical behavior given identical inputs across different Linux distributions
- The runtime SHALL use pinned versions of: glibc 2.35+, libstdc++, Vulkan loader 1.3.x
- The runtime SHALL isolate the game from host system libraries using namespaces (with container fallback)
- The runtime SHALL validate library versions at startup and fail with clear guidance if mismatches detected
- The runtime SHALL provide probabilistic performance characteristics with confidence intervals, not mathematical proofs

**AC-12: Security and Side-Channel Protection**
- The runtime SHALL validate all function pointers before invocation
- The runtime SHALL use memory-safe practices (bounds checking, no buffer overflows)
- The runtime SHALL provide secure random number generation via `lgx_random()`
- The runtime SHALL integrate with the LGX security module for attestation
- The runtime SHALL implement side-channel mitigations (constant-time operations, speculation barriers)
- The runtime SHALL provide separate allocation pools for sensitive data with guaranteed zeroing

**AC-13: Resource Limits (Dynamic and DoS Prevention)**
- The runtime SHALL enforce dynamic memory limits based on system capacity (default: 25% of system RAM, max 16GB)
- The runtime SHALL limit allocation rate with adaptive rate limiting based on system load
- The runtime SHALL limit open file handles to 10% of ulimit or 1024, whichever is lower
- The runtime SHALL limit log file size to 100MB with automatic rotation
- The runtime SHALL limit telemetry buffer to 10MB with adaptive sampling on overflow
- The runtime SHALL provide resource usage monitoring and early warning before limits are reached

**AC-14: Hardware Diversity and Graceful Degradation**
- The runtime SHALL detect hardware capabilities and classify into tiers (OPTIMAL, COMPATIBLE, DEGRADED)
- The runtime SHALL provide software fallbacks for missing hardware features
- The runtime SHALL report degraded features with performance impact estimates and remediation steps
- The runtime SHALL adapt allocation strategies based on NUMA topology and GPU-CPU affinity
- The runtime SHALL handle hardware fragmentation (different GPU vendors, NUMA configurations) gracefully

**AC-15: Intent-Based API with Validation**
- The runtime SHALL provide intent-based allocation API that captures access patterns, lifetime, and performance hints
- The runtime SHALL validate intent accuracy in debug builds and provide mismatch warnings
- The runtime SHALL support hierarchical intent structures for layer-specific extensions
- The runtime SHALL provide usage statistics to help developers improve intent accuracy
- The runtime SHALL gracefully handle incorrect intent by adapting allocations based on observed patterns

**AC-16: Frame Arena Adaptive Sizing and Overflow Handling (NEW)**
- The runtime SHALL support configurable frame arena size via `lgx_config_set_frame_arena_size()`
- The runtime SHALL implement adaptive arena growth when overflow detected (double size, max 256MB)
- The runtime SHALL provide early warning when arena usage exceeds 80% of capacity
- The runtime SHALL track allocation patterns and recommend optimal arena size
- The runtime SHALL fall back to persistent heap on overflow with <5% performance penalty
- The runtime SHALL rate-limit overflow warnings (max 1 per second) to prevent log spam
- The runtime SHALL provide detailed overflow telemetry (frame number, allocation size, call site)
- The runtime SHALL verify `lgx_frame_reset()` is called at frame boundaries
- The runtime SHALL detect and report frame arena memory leaks (allocations not reset)
- The runtime SHALL provide debugging tools: allocation histogram, call site tracking, usage visualization
- The runtime SHALL gracefully handle incorrect intent by adapting allocations based on observed patterns

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

## 6. Critical Success Factors (Go/No-Go Criteria)

Before proceeding to Phase 1 implementation, the following 10 Critical Success Factors must be validated:

### 6.1 Technical Feasibility (5 factors)

**CSF-1: Hybrid Allocator Performance**
- Target: <5μs p99 allocation latency under contention
- Validation: Implement prototype with 50 threads, measure across size classes
- Go/No-Go: If >10μs p99, redesign allocation strategy

**CSF-2: NUMA-Aware Allocation Benefit**
- Target: >10% performance improvement on NUMA systems
- Validation: Benchmark on 2-socket and 4-socket systems
- Go/No-Go: If <5% improvement, deprioritize NUMA features

**CSF-3: Namespace Isolation Compatibility**
- Target: Works on Ubuntu, Fedora, Arch without root privileges
- Validation: Automated testing on all 3 distributions
- Go/No-Go: If unprivileged namespaces fail, pivot to container-based isolation

**CSF-4: ABI Stability Validation**
- Target: 100% compatibility within major version across compiler versions
- Validation: Automated ABI test matrix (GCC 9-13, Clang 10-17)
- Go/No-Go: If <95% compatibility, redesign ABI strategy

**CSF-5: Telemetry Overhead**
- Target: <1% CPU overhead for telemetry collection
- Validation: Benchmark in-process vs separate-process telemetry
- Go/No-Go: If >2% overhead, simplify telemetry or make optional

### 6.2 Business Viability (5 factors)

**CSF-6: Customer Commitment**
- Target: At least 1 paying customer (cloud gaming provider or OEM partnership)
- Validation: Complete sales cycle with signed LOI or contract
- Go/No-Go: If no customer after 6 months, pivot to open-source model

**CSF-7: Funding Security**
- Target: $500K secured for 15-month Phase 1
- Validation: Investor commitment or partner funding agreement
- Go/No-Go: If <$300K, reduce scope or extend timeline

**CSF-8: Competitive Differentiation**
- Target: >10% performance improvement over Steam Runtime/Proton
- Validation: Head-to-head benchmarks with 5 representative games
- Go/No-Go: If <5% improvement, insufficient value proposition

**CSF-9: Developer Adoption Feasibility**
- Target: <4 hours integration time for existing games
- Validation: 5 developers integrate LGX into existing projects
- Go/No-Go: If >8 hours, improve tooling and documentation

**CSF-10: Legal and IP Clearance**
- Target: No patent conflicts with major vendors (Valve, NVIDIA, AMD)
- Validation: Professional IP review and freedom-to-operate analysis
- Go/No-Go: If patent conflicts found, redesign affected components

### 6.3 Decision Matrix

```
All 10 factors ✅: PROCEED to Phase 1 as planned
7-9 factors ✅: PROCEED with risk mitigation for failed factors
5-6 factors ✅: PAUSE and address critical gaps before proceeding
<5 factors ✅: NO-GO, fundamental redesign or pivot required
```

## 7. Risk Mitigation Strategies

### 7.1 Layer 3 Vendor Partnership Contingency

**Risk**: Layer 3 success depends entirely on vendor partnerships

**Mitigation Plan:**
- **Plan A (Ideal)**: Full vendor partnerships with direct hardware access
- **Plan B (Realistic)**: Partial vendor support using existing APIs and extensions
- **Plan C (Fallback)**: Software-based optimizations, rebrand as "Advanced Optimization Layer"

**Implementation**: Start vendor conversations in Phase 1, not Phase 3

### 7.2 Performance Budget Validation

**Risk**: Aggressive performance targets may not be achievable

**Mitigation Plan:**
- Implement tiered performance targets (Tier 1: MVP, Tier 2: Competitive, Tier 3: Best-in-class)
- Validate budgets with realistic workloads in Phase 0.5
- Adjust targets based on empirical measurements, not theoretical estimates

### 7.3 Timeline and Resource Management

**Risk**: 48-month timeline with complex dependencies

**Mitigation Plan:**
- Extend Phase 1 to 15 months (from 12) based on engineering analysis
- Plan for 6-7 engineers by Phase 4 (~$3.3M total budget)
- Implement incremental delivery within each phase to reduce risk

## 8. Success Metrics (Refined)

### 8.1 Integration and Usability
- **Tier 1 (MVP)**: <4 hours integration time
- **Tier 2 (Target)**: <2 hours integration time  
- **Tier 3 (Best-in-class)**: <1 hour integration time

### 8.2 Performance Metrics
See AC-8 for detailed performance tiers

### 8.3 Reliability Metrics
- **Tier 1 (MVP)**: >99% crash-free rate
- **Tier 2 (Target)**: >99.9% crash-free rate
- **Tier 3 (Best-in-class)**: >99.99% crash-free rate

### 8.4 Business Metrics
- **Phase 1**: 1 paying customer, break-even on development costs
- **Phase 2**: 3 customers, $500K annual revenue
- **Phase 3**: 10 customers, $2M annual revenue
- **Phase 4**: Market leadership in Linux gaming runtime space
