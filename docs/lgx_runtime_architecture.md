# LGX Runtime Core — Architecture Documentation

## System Overview

LGX Runtime Core is the foundation layer of the LGX Platform — a stable gaming ABI for Linux. It provides specialized memory management, hardware adaptation, observability, and platform services that game engines and games depend on.

```
┌─────────────────────────────────────────────────────────┐
│  Game / Game Engine                                     │
├─────────────────────────────────────────────────────────┤
│  LGX Platform API                                       │
│  ┌──────────────┬──────────────┬──────────────┐         │
│  │ lgx_runtime  │ lgx_threading│  (future)    │         │
│  │    v1.0      │    v1.1      │  graphics,   │         │
│  │              │              │  input, audio │         │
│  └──────────────┴──────────────┴──────────────┘         │
├─────────────────────────────────────────────────────────┤
│  Linux Kernel (any distro, kernel 5.10+)                │
└─────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### Memory Subsystem

```
                ┌──────────────────────────────┐
                │   Intent-Based Routing API   │
                │  lgx_alloc_with_intent()     │
                └─────────┬────────────────────┘
                          │ routes by lifetime + hint
          ┌───────────────┼───────────────────┐
          ▼               ▼                   ▼
  ┌──────────────┐ ┌──────────────┐  ┌──────────────────┐
  │ Frame Arena  │ │  GPU Pool    │  │ Persistent Heap  │
  │ (80% allocs) │ │ (15% allocs) │  │   (5% allocs)    │
  │              │ │              │  │                  │
  │ Triple-buffer│ │ Buddy alloc  │  │ Segregated fit   │
  │ Bump pointer │ │ Vulkan sub-  │  │ 16 size classes  │
  │ Huge pages   │ │ allocation   │  │ Slab + buddy     │
  │ P99 < 0.1 μs│ │ P99 < 10 μs │  │ P99 < 20 μs     │
  └──────────────┘ └──────────────┘  └──────────────────┘
```

**Design decisions**:
- **Why triple-buffered arenas?** Frame N allocates from arena A while frame N-1's arena B may still be in-flight on GPU. Triple buffering guarantees no conflicts.
- **Why intent-based routing?** The game knows allocation semantics at call time. By declaring intent, the runtime routes to the optimal allocator without the developer manually choosing.
- **Why not replace malloc?** Replacing the system allocator is fragile and breaks third-party libraries. LGX provides a parallel, opt-in allocation system.

### Hardware Adaptation Layer

```
  ┌──────────────────────────────────┐
  │     Capability Detection         │
  │  NUMA, huge pages, GPU, SIMD     │
  └────────────────┬─────────────────┘
                   │
  ┌────────────────▼─────────────────┐
  │        Tier Classification       │
  │  OPTIMAL → COMPATIBLE → DEGRADED│
  └────────────────┬─────────────────┘
                   │
  ┌────────────────▼─────────────────┐
  │     Graceful Degradation         │
  │  Software fallbacks + guidance   │
  └──────────────────────────────────┘
```

Games never crash due to missing hardware features. The runtime provides software fallbacks with human-readable remediation steps.

### Observability Stack

```
  ┌─────────────────────────────────────────────┐
  │              Game Process                    │
  │  ┌────────────┬──────────┬────────────┐     │
  │  │ Counters   │ Logging  │ Trace Ring │     │
  │  │ (atomic)   │ (tagged) │ (lock-free)│     │
  │  └────────────┴──────────┴──────┬─────┘     │
  │                                  │ IPC       │
  └──────────────────────────────────┼───────────┘
                                     ▼
                    ┌────────────────────────────┐
                    │    Telemetry Process        │
                    │  (separate PID, opt-in)     │
                    │  Correlation, anomalies,    │
                    │  JSON export, anonymization │
                    └────────────────────────────┘
```

**Design decisions**:
- **Separate telemetry process**: Zero impact on game frame time. Even if telemetry crashes, the game continues.
- **Observability levels**: From `NONE` (0% overhead) to `EXHAUSTIVE` (>5%). Games choose the right trade-off.

### Error Handling Architecture

```
  API Call
    │
    ├─ Input validation (null, bounds, enum range)
    │
    ├─ Operation attempt
    │     │
    │     ├─ Success → return LGX_SUCCESS
    │     │
    │     └─ Failure → set thread-local error context
    │                  ├─ severity (WARNING / ERROR / FATAL)
    │                  ├─ recovery action (RETRY / DEGRADE / ABORT)
    │                  ├─ recovery steps (human-readable)
    │                  └─ invoke error callback (if registered)
    │
    └─ return error code
```

Every error includes actionable recovery guidance. Games don't just get "error code 5" — they get "memory pressure detected, try reducing frame arena usage by 20%, or increase via `lgx_config_set_frame_arena_size()`".

---

## Security Model

### Trust Boundaries

```
  ┌──────────────────────────────────────────┐
  │         TRUSTED (LGX Runtime)            │
  │  - Input validation on all API calls     │
  │  - Guard pages (debug builds)            │
  │  - Memory canaries                       │
  │  - Delayed reclamation (3-frame)         │
  │  - Allocation tracking                   │
  │  - Rate limiting (1M allocs/sec max)     │
  ├──────────────────────────────────────────┤
  │       UNTRUSTED (Game Code)              │
  │  - May pass invalid pointers             │
  │  - May double-free                       │
  │  - May overflow buffers                  │
  └──────────────────────────────────────────┘
```

### Security features:
- **Guard pages**: PROT_NONE pages after allocations detect overflow (debug)
- **Canary values**: Detect heap corruption at free time
- **Delayed reclamation**: Pointers remain valid for 3 frames to catch use-after-free
- **Secure wiping**: Optional zeroing on free for sensitive data

---

## ABI Stability Strategy

### Versioning
- **Semantic versioning**: MAJOR.MINOR.PATCH
- **ELF symbol versioning**: `LGX_RUNTIME_1.0`, `LGX_RUNTIME_1.1`
- **Struct evolution**: First field is always `struct_size` for forward compatibility

### Compatibility guarantee
- v1.0 binary → works with v1.x runtime (any minor/patch)
- v1.0 binary → does NOT work with v2.0 runtime (major break)
- New API functions added in minor versions (additive only)
- Struct fields appended (never reordered or removed)

---

## Module Interaction

### Runtime Core ↔ Threading Module

```c
// Threading module uses runtime core for:
// - Memory allocation (lgx_alloc_* for job data, fiber stacks)
// - Timing services (lgx_time_now_ns for benchmarks)
// - Error handling (lgx_result_t error codes)
// - Logging (lgx_log for debug output)

// Runtime core uses threading for:
// - Parallel initialization (thread pool)
// - Lock-free data structures (MPMC queues)
// - Fiber support for async operations
```

### Future Module Dependencies

```
lgx_runtime (v1.0) ─── foundation for all modules
    │
    ├── lgx_threading (v1.1) ─── required by graphics, input, audio
    │       │
    │       ├── lgx_graphics (v1.2) ─── Vulkan wrapper, GPU resources
    │       │
    │       ├── lgx_input (v1.3) ─── gamepad, keyboard, mouse
    │       │
    │       └── lgx_audio (v1.4) ─── 3D audio, mixing
    │
    └── lgx_networking (v2.0) ─── multiplayer primitives
```

---

## Performance Architecture

### Allocation Fast Path

```
lgx_alloc_frame(size)
    │
    ├─ Get thread-local arena pointer         [1 load]
    ├─ Align size to 16 bytes                 [1 AND]
    ├─ Bump pointer (offset += aligned_size)  [1 ADD]
    ├─ Check overflow (offset > capacity)     [1 CMP]
    ├─ Return base + old_offset               [1 ADD]
    │
    Total: 5 instructions, < 0.01 μs
```

### Key Optimizations
- **Huge pages**: 2 MB pages reduce TLB misses by 32×
- **Cache-line alignment**: All hot data aligned to 64 bytes
- **Lock-free structures**: No mutex contention on allocation paths
- **SIMD**: AVX2 for buddy allocator free-block search
- **Prefetching**: `__builtin_prefetch` for sequential arena access

---

## File Structure

```
LGX/
├── include/
│   ├── lgx_runtime.h          # Public API (383 lines)
│   ├── lgx_types.h            # Type definitions (453 lines)
│   ├── lgx_version.h          # Version macros
│   ├── lgx_integration.h      # Component integration contracts
│   └── lgx_threading.h        # Threading module public API
├── src/
│   ├── runtime/
│   │   ├── lgx_runtime_core.c      # Init/shutdown, config
│   │   ├── lgx_frame_arena.c       # Frame allocator
│   │   ├── lgx_gpu_pool.c          # GPU memory pool
│   │   ├── lgx_persistent_heap.c   # Persistent allocator
│   │   ├── lgx_intent_allocator.c  # Intent routing
│   │   ├── lgx_hardware_adapter.c  # Tier classification
│   │   ├── lgx_health_monitor.c    # Health checks
│   │   ├── lgx_telemetry.c         # Telemetry collection
│   │   ├── lgx_trace_events.c      # Trace ring buffer
│   │   └── ... (28 source files)
│   └── threading/
│       └── lgx_threading.c         # Threading module (1500+ lines)
├── tests/
│   ├── runtime/                    # 49+ tests
│   └── threading/                  # 20+ tests
├── benchmarks/                     # Performance regression tests
├── docs/
│   ├── lgx_runtime_api.md          # API reference
│   ├── lgx_runtime_integration_guide.md  # Quick start + patterns
│   └── lgx_runtime_architecture.md       # This document
└── lgx_threading.map               # Symbol version script
```
