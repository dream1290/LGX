# Architecture Documentation

## Overview

This section documents the architectural design and implementation details of the LGX Runtime Core.

## Contents

### System Architecture

- **[Memory Management Architecture](memory-architecture.md)** - Hybrid allocation strategy, allocator design
- **[Lifecycle Management](lifecycle-architecture.md)** - Initialization, suspend/resume, shutdown
- **[Hardware Adaptation](hardware-adaptation.md)** - Tier detection, graceful degradation
- **[Observability System](observability-architecture.md)** - Logging, metrics, telemetry

### Component Design

- **[Frame Arena Allocator](frame-arena-design.md)** - Triple-buffered bump pointer allocation
- **[Persistent Heap](persistent-heap-design.md)** - Segregated fit allocator
- **[GPU Memory Pool](gpu-pool-design.md)** - Vulkan memory management
- **[Lock-Free Pool](lockfree-pool-design.md)** - Zero-contention allocation

### Design Decisions

- **[Hybrid Allocation Strategy](decisions/hybrid-allocation.md)** - Why multiple allocators
- **[Intent-Based API](decisions/intent-based-api.md)** - Automatic allocator routing
- **[ABI Stability](decisions/abi-stability.md)** - Forward compatibility guarantees
- **[Security Model](decisions/security-model.md)** - Defense in depth approach

## System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│                  (Game Binary / Client)                      │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              LGX Runtime Core (liblgx_runtime.so)           │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  ABI Layer   │  │   Version    │  │  Capability  │     │
│  │              │  │  Management  │  │  Detection   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │   Memory     │  │  Lifecycle   │  │   Platform   │     │
│  │  Management  │  │   Manager    │  │   Services   │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │      Observability & Monitoring                     │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│           Hardware Abstraction Layer                         │
│  GPU (Vulkan) │ NUMA │ Huge Pages │ CPU Features           │
└─────────────────────────────────────────────────────────────┘
```

## Key Architectural Principles

### 1. Performance First
- Sub-5μs allocation latency for critical paths
- Lock-free data structures on hot paths
- Thread-local caching to eliminate contention
- Hardware-aware optimizations (NUMA, huge pages)

### 2. Deterministic Behavior
- Consistent behavior across distributions
- Predictable memory usage patterns
- Reproducible performance characteristics
- Stable ABI across versions

### 3. Graceful Degradation
- Software fallbacks for missing hardware features
- Automatic tier detection and adaptation
- Performance impact estimation
- Clear error reporting

### 4. Security by Design
- Input validation at all boundaries
- Memory safety (guard pages, canaries)
- Resource limits and rate limiting
- Namespace isolation

### 5. Observability
- Comprehensive performance counters
- Structured logging with filtering
- Health monitoring and anomaly detection
- Privacy-preserving telemetry (opt-in)

## Memory Allocator Architecture

The runtime uses a hybrid allocation strategy with multiple specialized allocators:

### Allocation Path Decision Tree

```
Allocation Request
    │
    ├─ Intent: FRAME → Frame Arena (bump pointer)
    ├─ Intent: PERSISTENT → Persistent Heap (segregated fit)
    ├─ Intent: LEVEL → Level Allocator (pool-based)
    └─ Generic → Hybrid Allocator
                    │
                    ├─ Size ≤ 4KB → Lock-Free Pool
                    │                  │
                    │                  ├─ Hot path cache hit → O(1)
                    │                  ├─ Pool allocation → O(1) atomic
                    │                  └─ Pool exhausted → malloc
                    │
                    └─ Size > 4KB → Direct malloc
```

### Performance Characteristics

| Allocator | Latency (P99) | Use Case | Thread Safety |
|-----------|---------------|----------|---------------|
| Frame Arena | <1μs | Per-frame data | Thread-local |
| Hot Path Cache | <1μs | Small frequent allocs | Thread-local |
| Lock-Free Pool | <5μs | General purpose | Lock-free |
| Persistent Heap | <20μs | Long-lived data | Mutex-protected |
| Direct malloc | Variable | Large allocations | System-dependent |

## Lifecycle State Machine

```
┌─────────┐
│ CREATED │
└────┬────┘
     │ lgx_runtime_init()
     ▼
┌──────────────┐
│ INITIALIZED  │◄─────────────┐
└──┬───────┬───┘              │
   │       │                  │
   │       │ lgx_runtime_suspend()
   │       ▼                  │
   │  ┌───────────┐           │
   │  │ SUSPENDED │           │
   │  └─────┬─────┘           │
   │        │                 │
   │        │ lgx_runtime_resume()
   │        └─────────────────┘
   │
   │ lgx_runtime_shutdown()
   ▼
┌──────────┐
│ SHUTDOWN │
└──────────┘
```

## Hardware Tier Classification

The runtime automatically detects and adapts to hardware capabilities:

### Tier Levels

1. **OPTIMAL** - All features available, maximum performance
   - Vulkan 1.3+ GPU
   - NUMA-aware memory
   - Huge pages enabled
   - Modern CPU (AVX2+)

2. **COMPATIBLE** - Core features available, good performance
   - Vulkan 1.1+ GPU or software rendering
   - Standard memory allocation
   - Regular pages
   - SSE2+ CPU

3. **DEGRADED** - Minimal features, reduced performance
   - No GPU acceleration
   - Limited memory
   - Software fallbacks
   - Basic CPU features

### Adaptation Strategy

```c
if (hardware_tier == OPTIMAL) {
    // Use all optimizations
    enable_gpu_pool();
    enable_huge_pages();
    enable_numa_awareness();
} else if (hardware_tier == COMPATIBLE) {
    // Use core features
    disable_gpu_pool();
    use_regular_pages();
} else {
    // Minimal mode
    use_software_fallbacks();
    reduce_memory_footprint();
}
```

## Thread Safety Model

### Lock-Free Components
- Hot path cache (thread-local)
- Lock-free pool (atomic operations)
- Performance counters (atomic)

### Mutex-Protected Components
- Persistent heap (coarse-grained lock)
- Configuration changes (rare)
- Lifecycle state transitions

### Thread-Local Components
- Frame arenas (per-thread)
- Allocation caches (per-thread)
- Thread-specific metrics

## Error Handling Strategy

### Error Categories

1. **Recoverable Errors** - Return error code, continue operation
   - Out of memory (try alternative allocator)
   - Invalid parameters (reject request)
   - Resource limits (throttle)

2. **Fatal Errors** - Log and terminate gracefully
   - Corrupted internal state
   - Unrecoverable system errors
   - Security violations

3. **Warnings** - Log but continue
   - Performance degradation
   - Resource pressure
   - Configuration issues

## Related Documentation

- [Performance Documentation](../05-performance/README.md)
- [Testing Strategy](../06-testing/README.md)
- [Security Model](../07-security/README.md)
- [Operations Guide](../08-operations/README.md)
