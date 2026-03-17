# LGX Platform Architecture Specification

**Version**: 1.0  
**Status**: Active  
**Modules Defined**: Memory (v1.0), Threading (v1.1), Graphics (v1.2), Input (v1.3), Audio (v1.4)

---

## 1. Platform Overview

LGX Runtime Platform is a modular, stable-ABI gaming platform for Linux. Each module is a separate shared library with well-defined contracts, allowing games to adopt incrementally.

```
┌─────────────────────────────────────────────────────────────────┐
│  Game / Game Engine                                             │
│  Includes: <lgx/platform.h>                                    │
├─────────────────────────────────────────────────────────────────┤
│  LGX Platform Unified API                                       │
│  ┌───────────┬───────────┬───────────┬───────────┬───────────┬────────────┐ │
│  │ lgx_memory│lgx_thread │lgx_gfx    │ lgx_input │ lgx_audio │lgx_profile │ │
│  │   v1.0 ✅ │  v1.1 ✅  │  v1.2 ✅  │  v1.3 ✅  │  v1.4 ✅  │  v1.5 ✅   │ │
│  │           │           │           │           │           │            │ │
│  │ liblgx_   │ liblgx_   │ liblgx_   │ liblgx_   │ liblgx_   │ liblgx_    │ │
│  │ runtime.so│threading.so│graphics.so│ input.so  │ audio.so  │ profile.so │ │
│  └───────────┴───────────┴───────────┴───────────┴───────────┘ │
├─────────────────────────────────────────────────────────────────┤
│  Linux Kernel 5.10+  │  Vulkan 1.3  │  PipeWire  │  evdev     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Unified API Design Principles

All LGX modules follow these rules to ensure consistency across the platform:

### 2.1 Naming Convention

```c
// Pattern: lgx_<module>_<action>(<object>, ...)
// Module prefixes (short, memorable):
//   lgx_      - runtime core (memory, lifecycle)
//   lgx_th_   - threading
//   lgx_gfx_  - graphics
//   lgx_in_   - input
//   lgx_aud_  - audio
//   lgx_net_  - networking

// Examples:
lgx_alloc_frame(size);                    // memory
lgx_th_job_submit(system, func, data);    // threading
lgx_gfx_cmd_begin(cmd_buf);              // graphics
lgx_in_gamepad_get_state(pad, &state);   // input
lgx_aud_source_play(source);             // audio
```

### 2.2 Handle Pattern

All complex objects use **opaque handles** (pointers to forward-declared structs):

```c
// Create / Destroy pattern (RAII-like lifecycle)
lgx_th_job_system_t* system = lgx_th_job_system_create(&config);
// ... use system ...
lgx_th_job_system_destroy(system);

// Rules:
// - Create returns NULL on failure (sets error context)
// - Destroy is safe to call with NULL (no-op)
// - Objects are thread-safe unless documented otherwise
```

### 2.3 Error Handling

Every module uses the same error system:

```c
lgx_result_t result = lgx_gfx_pipeline_create(&pipeline, &desc);
if (result != LGX_SUCCESS) {
    lgx_error_context_ex_t err = lgx_get_last_error_ex();
    // err.severity       → WARNING, ERROR, FATAL
    // err.suggested_action → RETRY, DEGRADE, ABORT, SHUTDOWN
    // err.recovery_steps   → human-readable fix
}
```

### 2.4 Configuration Pattern

All modules use the same opaque-config approach:

```c
lgx_<module>_config_t* cfg = lgx_<module>_config_create();
lgx_<module>_config_set_<option>(cfg, value);
lgx_<module>_init(cfg);
lgx_<module>_config_destroy(cfg);
```

### 2.5 Struct Versioning

Every public struct has `struct_size` as its first field:

```c
typedef struct lgx_whatever {
    size_t struct_size;  // ALWAYS FIRST — enables forward compat
    // ... fields ...
} lgx_whatever_t;

// Usage:
lgx_whatever_t thing = { .struct_size = sizeof(lgx_whatever_t), ... };
```

New fields are always appended. Old binaries seeing new structs safely ignore extra fields via `struct_size` comparison.

---

## 3. ABI Stability Guarantees

### 3.1 Versioning

- **Semantic versioning**: MAJOR.MINOR.PATCH
- **Major version** = ABI break (v1 → v2). Games must recompile.
- **Minor version** = additive API (v1.0 → v1.1). New functions, no breaks.
- **Patch version** = bug fixes only

### 3.2 Symbol Versioning

Each module maintains an ELF symbol version map:

```
LGX_RUNTIME_1.0 {
    global: lgx_runtime_init; lgx_alloc; lgx_free; ...
};
LGX_RUNTIME_1.1 {
    global: lgx_alloc_aligned; lgx_frame_arena_get_recommended_size;
} LGX_RUNTIME_1.0;

LGX_THREADING_1.1 {
    global: lgx_mutex_*; lgx_job_*; lgx_fiber_*; ...
};
```

### 3.3 Backward Compatibility Rules

| Rule | Description |
|------|-------------|
| Never remove a function | Only deprecate (keep symbol, add warning) |
| Never change function signature | Add new function with `_ex` or `_v2` suffix |
| Never reorder struct fields | Only append new fields at end |
| Never change enum values | Only append new values at end |
| Never change struct size | Use `struct_size` for runtime detection |

### 3.4 Deprecation Process

```c
// 1. Mark deprecated in header
__attribute__((deprecated("Use lgx_alloc_with_intent() instead")))
void* lgx_alloc_old(size_t size);

// 2. Keep implementation for 2 minor versions (e.g., 1.0 → 1.2)
// 3. Remove in next major version (v2.0)
```

---

## 4. Module Dependency Graph

```
lgx_runtime (v1.0) ─── Required by all modules
    │
    ├── lgx_threading (v1.1) ─── Required by graphics, optional for input/audio
    │       │
    │       ├── lgx_graphics (v1.2) ─── Requires threading for command buffers
    │       │       │
    │       │       └── lgx_audio (v1.4) ─── Requires graphics for 3D spatial
    │       │
    │       └── lgx_input (v1.3) ─── Uses threading for event pump
    │
    └── lgx_networking (v2.0) ─── Requires threading for async I/O
```

### Dependency Rules

1. **Runtime is always required** — memory, error handling, logging
2. **Threading is strongly recommended** — graphics won't work without it
3. **Everything else is optional** — games pick modules they need
4. **No circular dependencies** — strict DAG ordering

---

## 5. Inter-Module Communication

### 5.1 Shared Services (via Runtime)

All modules access shared services through the runtime core:

```c
// Memory — every module allocates through LGX
void* buf = lgx_alloc_frame(size);        // frame-scoped scratch
void* data = lgx_alloc_persistent(size);   // long-lived

// Error handling — every module uses the same error system
lgx_error_context_ex_t err = lgx_get_last_error_ex();

// Logging — every module uses tagged logging
lgx_log_tagged(LGX_SUBSYSTEM_GPU, LGX_LOG_INFO, "Pipeline created");

// Timing — every module uses the same clock
uint64_t now = lgx_time_now_ns();

// Counters — every module can register custom counters
lgx_custom_counter_t my_counter = lgx_register_counter("draw_calls");
lgx_increment_counter(my_counter);
```

### 5.2 Module Integration Contracts

Modules communicate through **integration contracts** defined in `lgx_integration.h`:

```c
// Graphics registers its memory needs with runtime
lgx_result_t lgx_integration_register_gpu_allocator(
    lgx_gpu_alloc_fn alloc_fn,
    lgx_gpu_free_fn free_fn,
    void* user_data
);

// Threading provides job submission for graphics command recording
lgx_result_t lgx_integration_register_job_system(
    lgx_job_submit_fn submit_fn,
    lgx_job_wait_fn wait_fn,
    void* user_data
);
```

### 5.3 Frame Lifecycle

All modules participate in the frame lifecycle:

```
┌─ Frame Begin ────────────────────────────────────────────────┐
│                                                              │
│  1. lgx_frame_reset()        ← Memory: reset frame arenas   │
│  2. lgx_in_poll()            ← Input: poll events           │
│  3. lgx_aud_update()         ← Audio: update 3D listener    │
│  4. Game update               ← Game logic, physics         │
│  5. lgx_gfx_cmd_begin()      ← Graphics: record commands    │
│  6. lgx_gfx_cmd_end()        ← Graphics: submit to GPU     │
│  7. lgx_gfx_present()        ← Graphics: swap buffers      │
│                                                              │
└─ Frame End ──────────────────────────────────────────────────┘
```

---

## 6. Module Specifications

### 6.1 Graphics Module (v1.2) — Design Preview

**Purpose**: Thin Vulkan wrapper providing pipeline management, command recording, and resource lifecycle.

```c
// Core types
typedef struct lgx_gfx_device lgx_gfx_device_t;
typedef struct lgx_gfx_swapchain lgx_gfx_swapchain_t;
typedef struct lgx_gfx_pipeline lgx_gfx_pipeline_t;
typedef struct lgx_gfx_cmd_buffer lgx_gfx_cmd_buffer_t;

// Device creation (wraps VkInstance + VkDevice)
lgx_gfx_device_t* lgx_gfx_device_create(const lgx_gfx_device_config_t* config);
void lgx_gfx_device_destroy(lgx_gfx_device_t* device);

// Command recording (uses threading job system)
lgx_gfx_cmd_buffer_t* lgx_gfx_cmd_begin(lgx_gfx_device_t* device);
void lgx_gfx_cmd_draw(lgx_gfx_cmd_buffer_t* cmd, uint32_t vertex_count, uint32_t instance_count);
void lgx_gfx_cmd_end(lgx_gfx_cmd_buffer_t* cmd);

// Resource management (uses runtime GPU pool)
lgx_gfx_buffer_t* lgx_gfx_buffer_create(lgx_gfx_device_t* device, size_t size, uint32_t usage);
lgx_gfx_image_t* lgx_gfx_image_create(lgx_gfx_device_t* device, const lgx_gfx_image_desc_t* desc);

// Presentation
lgx_gfx_swapchain_t* lgx_gfx_swapchain_create(lgx_gfx_device_t* device, void* window);
lgx_result_t lgx_gfx_present(lgx_gfx_swapchain_t* swapchain);
```

**Integration with other modules**:
- **Memory**: Uses `lgx_alloc_gpu_shared()` for buffer/image backing
- **Threading**: Command recording parallelized via job system
- **Runtime**: Error handling, logging, health monitoring

### 6.2 Input Module (v1.3) — Design Preview

```c
typedef struct lgx_in_system lgx_in_system_t;

lgx_in_system_t* lgx_in_create(const lgx_in_config_t* config);
void lgx_in_destroy(lgx_in_system_t* system);
void lgx_in_poll(lgx_in_system_t* system);  // Called once per frame

// Gamepad
bool lgx_in_gamepad_connected(lgx_in_system_t* system, uint32_t pad_id);
lgx_result_t lgx_in_gamepad_get_state(lgx_in_system_t* system, uint32_t pad_id,
                                       lgx_in_gamepad_state_t* state);

// Keyboard & Mouse
bool lgx_in_key_pressed(lgx_in_system_t* system, lgx_in_key_t key);
lgx_result_t lgx_in_mouse_get_state(lgx_in_system_t* system, lgx_in_mouse_state_t* state);
```

### 6.3 Audio Module (v1.4) — Design Preview

```c
typedef struct lgx_aud_system lgx_aud_system_t;
typedef struct lgx_aud_source lgx_aud_source_t;

lgx_aud_system_t* lgx_aud_create(const lgx_aud_config_t* config);
void lgx_aud_destroy(lgx_aud_system_t* system);
void lgx_aud_update(lgx_aud_system_t* system);  // Called once per frame

// 3D audio
lgx_aud_source_t* lgx_aud_source_create(lgx_aud_system_t* system, const lgx_aud_buffer_t* buffer);
void lgx_aud_source_set_position(lgx_aud_source_t* source, float x, float y, float z);
void lgx_aud_source_play(lgx_aud_source_t* source);
void lgx_aud_source_stop(lgx_aud_source_t* source);
void lgx_aud_listener_set_position(lgx_aud_system_t* system, float x, float y, float z);
```

---

## 7. Build System

### 7.1 Modular CMake

```cmake
# Games choose which modules to use
find_package(lgx_runtime 1.0 REQUIRED)
find_package(lgx_threading 1.1 REQUIRED)
find_package(lgx_graphics 1.2)  # Optional

target_link_libraries(my_game
    lgx_runtime::lgx_runtime
    lgx_threading::lgx_threading
    $<$<TARGET_EXISTS:lgx_graphics::lgx_graphics>:lgx_graphics::lgx_graphics>
)
```

### 7.2 pkg-config

```bash
# Each module has its own .pc file
pkg-config --cflags --libs lgx_runtime lgx_threading lgx_graphics
```

### 7.3 Feature Flags

```cmake
# Build-time options per module
option(LGX_ENABLE_GPU_POOL "Enable GPU memory pool (requires Vulkan)" ON)
option(LGX_ENABLE_NUMA "Enable NUMA-aware allocation" ON)
option(LGX_ENABLE_TELEMETRY "Enable opt-in telemetry" ON)
option(LGX_ENABLE_CHAOS_TESTING "Enable chaos test framework" OFF)
```

---

## 8. Performance Budgets

Each module has a strict performance budget to ensure the platform never becomes a bottleneck:

| Module | Operation | P99 Budget | Current |
|--------|-----------|------------|---------|
| Memory | Frame alloc | < 0.1 μs | 0.04 μs ✅ |
| Memory | Persistent alloc | < 20 μs | 0.09 μs ✅ |
| Memory | GPU alloc | < 10 μs | 5.1 μs ✅ |
| Threading | Job submit | < 1 μs | — |
| Threading | Job steal | < 5 μs | — |
| Graphics | Command record | < 10 μs | — |
| Graphics | Present | < 1 ms | — |
| Input | Poll | < 100 μs | — |
| Audio | Update | < 500 μs | — |
| **Total platform overhead** | **Per frame** | **< 2 ms** | — |

**Rule**: No module may exceed its budget. If it does, it's a blocker for release.

---

## 9. Testing Strategy

### Per-Module Requirements

| Requirement | Minimum |
|-------------|---------|
| Unit test coverage | 100% of public API |
| Integration tests | Module pairs (memory+threading, threading+graphics) |
| ABI compat tests | Old binary → new library |
| Performance tests | P50, P95, P99 for all hot paths |
| Failure injection | Error paths exercised |
| Chaos testing | Random failures during stress |

### Platform-Level Tests

- **Full integration**: Initialize all modules, run simulated game loop
- **Cross-module**: Graphics submits jobs to threading, allocates from memory
- **Stress**: 8-hour session with fragmentation, leak detection
- **Regression**: Performance baseline comparison on every PR

---

## 10. Distribution

### Package Structure

```
lgx-runtime        ← Memory, lifecycle, platform services (v1.0)
lgx-threading       ← Threading, jobs, fibers (v1.1)
lgx-graphics        ← Vulkan wrapper (v1.2)
lgx-input           ← Input handling (v1.3)
lgx-audio           ← Audio engine (v1.4)
lgx-dev             ← All headers + cmake configs + pkg-config
lgx-platform        ← Meta-package: all modules
```

Games depend on specific modules. Distributions ship the meta-package.

---

*This architecture ensures every LGX module feels like part of the same platform — consistent APIs, shared infrastructure, guaranteed stability.*
