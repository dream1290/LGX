# LGX — The Codebase

*A guided tour of what actually exists in the repository: every module, every test, every tool.*

---

## 1. Overview

LGX is a C11 project (~17,000 lines of runtime source, 39 source files, 14 public headers, 100 test files) built with CMake 3.16+, targeting Linux x86_64 (kernel 5.10+), compiled with GCC 11+/Clang 10+.

**Layout:**

```
src/           39 .c files across 10 module directories
include/       14 public headers
tests/         100 .c files across 16 categories
benchmarks/    5 benchmark programs + framework
docs/          10 numbered doc directories + strategy docs
packaging/     debian, rpm, arch packaging
scripts/       release, security, benchmark tooling
tools/         lifetime-profiler
```

---

## 2. The Core Runtime (`src/runtime/` + `include/`)

`liblgx_runtime.so` (v1.0.1) is the heart: memory management, lifecycle, observability, and security. 28 files:

### Memory subsystem (the crown jewel)
| File | Lines | Role |
|---|---|---|
| `lgx_frame_arena.c` | 1493 | Triple-buffered bump-pointer arena. P99 0.04 μs (~200× malloc). Handles ~80% of game allocations |
| `lgx_persistent_heap.c` | 1648 | Segregated-fit + buddy allocator for long-lived data (P99 < 20 μs) |
| `lgx_gpu_pool.c` | 1044 | Vulkan-backed buddy allocator for GPU memory (P99 ~5 μs) |
| `lgx_memory_manager.c` | 1972 | Top-level manager, intent-based routing, stats |
| `lgx_intent_allocator.c` | 438 | Declare lifetime → auto-route to optimal allocator |
| `lgx_lockfree_pool.c` | 236 | Lock-free object pool |
| `lgx_hugepages.c` | 364 | Transparent huge-page utilization |
| `lgx_simd_ops.c` | 267 | SIMD-accelerated operations (AVX2 in debug) |

### Lifecycle & platform services
`lgx_runtime_core.c` (774) — init/shutdown state machine, subsystem wiring. `lgx_lifecycle_manager.c` (426) — suspend/resume, signal handling. `lgx_platform_services.c` (740) — timing, filesystem, process services. `lgx_hardware_adapter.c` (405) + `lgx_capability_detector.c` (152) — tier classification, NUMA detection, graceful degradation.

### Observability
`lgx_telemetry.c` (1076) + `lgx_telemetry_process.c` (343) — privacy-preserving opt-in telemetry. `lgx_performance_counters.c` (217), `lgx_trace_events.c` (475), `lgx_health_monitor.c` (537), `lgx_leak_detector.c` (278), `lgx_memory_monitor.c` (332).

### Security & robustness
`lgx_error_handler.c` (389), `lgx_input_validation.c` (374), `lgx_memory_safety.c` (509) — guard pages, canaries. `lgx_chaos_testing.c` (329), `lgx_namespace_isolation.c` (283), `lgx_library_manifest.c` (295), `lgx_resource_limits.c` (412).

### Public headers
- `lgx_runtime.h` (382) — main API + convenience inlines (`lgx_alloc_frame`, `lgx_alloc_persistent`, `lgx_alloc_gpu_shared`)
- `lgx_types.h` (453), `lgx_version.h` (143, v1.0.1, `LGX_ABI_VERSION 1`)
- `lgx_integration.h` (223) — cross-module contracts
- `lgx/lgx_runtime_internal.h` (561) — internal subsystem interfaces

---

## 3. The Nine Platform Modules

| Module | Library | Version | What it provides |
|---|---|---|---|
| **Threading** | `lgx_threading.so` | 1.1.0 | Mutex/spinlock, MPMC/SPSC lock-free queues, work-stealing job system, fiber scheduler, Treiber stack, striped hash map, deadlock detection |
| **Graphics** | `lgx_graphics.so` | 1.2.0 | Vulkan wrapper: device/swapchain/pipeline management, command recording, resource management (shader cache exists in header docs) |
| **Input** | `lgx_input.so` | 1.3.0 | Unified gamepad/keyboard/mouse |
| **Audio** | `lgx_audio.so` | 1.4.0 | 3D spatialization, mixing, ALSA output |
| **Profiling** | `lgx_profile.so` | 1.5.0 | Hierarchical zones, counters, frame history, FPS tracking |
| **Networking** | `lgx_net.so` | 2.0.0 | UDP sockets, serialization, delta compression |
| **Asset** | `lgx_asset.so` | 2.1.0 | File loading, hot reload, compression |
| **Tooling** | `lgx_tools.so` | 2.2.0 | Perf analyzer, memory tracker, CSV export |

Build details: graphics is built only when Vulkan is found; audio only when ALSA is found; jemalloc is used as fallback allocator when available. Threading ships with its own symbol version map (`lgx_threading.map`).

**Legacy code:** `src/lgx_runtime.c` (1,107 lines, "Phase 0") is excluded from the build and stale against current headers. `src/lgx_allocator_prototype.c` (143) is compiled as a deprecated compat layer delegating to the new runtime.

---

## 4. Tests — 76 Registered, 100 Files

| Category | What it covers |
|---|---|
| `tests/phase0/` (~60 files) | Allocators, telemetry, signal handling, GPU pool, persistent heap, intent allocator, SIMD fallback, logging, timing, tiered performance, ABI stability |
| `tests/unit/` (8) | Error handler, health monitor, init/shutdown, memory allocation & protection, platform services, resource limits, version compatibility |
| `tests/integration/` (6) | End-to-end init, component integration, memory stress, suspend/resume, telemetry collection, AAA workload simulation |
| `tests/abi/` (3) | Symbol versioning, struct evolution, game-v1.0 binary compat |
| `tests/performance/` (5) | Allocation latency, frame time contribution, init time, memory footprint & overhead |
| `tests/failure_injection/` | OOM injection, telemetry crash, TOCTOU races, version mismatch (4 of 5 disabled: "not implemented") |
| `tests/threading/` (5) | Basic, stress, fibers, advanced, benchmark |
| Module tests | graphics, input, audio, profile, net, asset, tools (one `test_*_basic` each) |
| `tests/fuzzing/` | AFL/libFuzzer harnesses + testcases for allocator, lifecycle, API inputs |
| `tests/manual/` (8) | Frame arena profiler, stress, validation, tagging, debugging, dump, overflow handling, adaptive sizing |

Also: `tests/` root has a standalone `Makefile` + `test_framework.h` and 4 test programs (`test_basic.c`, `test_alignment.c`, `test_statistics.c`, `test_overflow.c`) written for **lgx_arena.h** — which does not exist yet.

Test infrastructure: AddressSanitizer + UBSan in debug builds, LeakSanitizer suppressions (`lsan.supp`), 300s timeouts for phase0, custom ctest targets (`run_unit_tests`, `run_integration_tests`, `run_performance_tests`, `run_threading_tests`).

---

## 5. Benchmarks

Five programs on a shared framework (`benchmarks/benchmark_framework.c/h`, static `libbenchmark_framework.a`):
allocation throughput, memory patterns, hardware adaptation, intent accuracy, telemetry overhead — each writing CSV results. Threading has its own `benchmark_threading.c`.

---

## 6. Tooling & Scripts

- **scripts/** (15): `release.sh`, `version.sh`, `changelog.sh`, security audit (`run_security_audit.sh`), static analysis (`run_static_analysis.sh`), clang-tidy, Coverity scan, ABI test matrix, chaos tests, benchmark comparison (`compare_benchmarks.py`), frame arena visualization (`visualize_frame_arena.py`), baseline capture, compatibility tests
- **tools/lifetime-profiler/** — LD_PRELOAD probe (`lgx_lifetime_probe.so`) + Python lifetime analyzer for allocation lifetime profiling

---

## 7. Packaging

- **Debian**: `packaging/debian/` — control file, rules, changelog; built `.deb` packages exist in `packaging/output/` (`lgx-runtime_1.0.1-1_amd64.deb`, `lgx-runtime-dev_1.0.1-1_amd64.deb`)
- **RPM / Arch**: packaging directories with build scripts
- pkg-config file (`lgx_runtime.pc`), version-scripted symbol exports, `cmake --install` support

---

## 8. CI

`.github/workflows/` — a main CI workflow (currently a simplified Linux build + test + example-run flow) plus compatibility-matrix workflows. **Note:** the current CI checks reference `./lgx_arena_example` and `lgx_arena.h` which do not exist — CI would fail today.

---

## 9. Honest Code Quality Assessment

The codebase is a large, ambitious, self-taught-engineered codebase with real strengths and real debts:

**Strengths**
- Clean, consistently named public API with versioning discipline (symbol maps, `struct_size` evolution)
- Impressive breadth: 9 modules, 28 runtime subsystems, 100 test files, fuzzing, chaos testing, packaging
- Several genuinely clean subsystems: `lgx_gpu_capability`, `lgx_runtime_core`, `lgx_lockfree_pool`, `lgx_simd_ops`, `lgx_capability_detector`, `lgx_input_validation`, buddy split/coalesce logic

**Known defects (from close code audit — see STATE.md)**
- Memory-safety bugs: GPU pool handle use-after-free on array realloc; frame arena mid-frame growth invalidates outstanding pointers; chaos-testing mutex self-deadlock; leak detector mutex wipe
- Logic bugs: intent allocator never routes to GPU pool (order-of-checks); memory-manager leak tracking never matches (compares array slot address); health monitor reads uninitialized fields; telemetry reports version "1.0.0"
- Stubs and gaps: ~15 functions declared in `lgx_integration.h` with no implementation; asset async loading is synchronous; net timeout never refreshes (every connection times out); tools ring-buffer percentile math corrupt after wraparound; fiber scheduler has stale-pointer and no-wakeup bugs
- Hygiene: foreign placeholder text in a doc comment; commented-out slab pool; dead code in trace events

The gap between the documented "Production-Ready / 76/76 passing" story and the audited reality is the central finding of the current state — see `STATE.md`.
