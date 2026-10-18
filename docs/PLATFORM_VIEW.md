# LGX — Platform View (as of this commit)

## Executive Summary

LGX is a C11 memory-runtime library for Linux games. The platform layer (threading, graphics, audio, etc.) exists as headers and stubs; only the memory core has substance. This document describes what exists, what works, and what is aspirational.

## 1. Module Map

```
┌──────────────────────────────────────────────────────────────────────┐
│                    APPLICATION (your game)                           │
└──────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌──────────────────────────────────────────────────────────────────────┐
│  LGX RUNTIME CORE (liblgx_runtime)  —  the only real module          │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                   │
│  │ Frame Arena │ Persistent    │ GPU Pool          │               │
│  │ (frame-reset│ (long-lived   │ (Vulkan-backed   │               │
│  │  allocator) │   allocations)│ buddy allocator)│               │
│  └─────────────┘ └─────────────┘ └─────────────┘                   │
│              ↑                                                  │
│              │ intent-based routing                              │
└──────────────────────────────────────────────────────────────────────┘
                              │
┌──────────────────────────────────────────────────────────────────────┐
│  OTHER MODULES  — headers exist; implementation status varies        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐│
│  │ lgx_th   │ lgx_gfx  │ lgx_aud  │ lgx_net  │ lgx_tool │       │
│  │ threading│ graphics │ audio    │ network  │ tools    │       │
│  │   .c/.h  │   .c/.h  │   .c/.h  │   .c/.h  │   .c/.h  │       │
│  │ threads/j│ Vulkan   │ ALSA     │ UDP sockets│ perf mem │       │
│  │  ob/lock │ wrapper  │ spatial  │ sockets  │ tracker  │       │
│  │    free  │          │          │          │          │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘│
└──────────────────────────────────────────────────────────────────────┘
```

## 2. What Is Implemented (Verified)

| Component | Status | Evidence |
|-----------|--------|----------|
| Frame arena | Implemented | `src/runtime/lgx_frame_arena.c` — bump-pointer, bump-reset per frame |
| Persistent heap | Implemented | `src/runtime/lgx_persistent_heap.c` |
| GPU pool | Implemented (Vulkan) | `src/runtime/lgx_gpu_pool.c` — buddy allocator |
| Intent routing | Implemented | `src/runtime/lgx_intent_allocator.c` — routes alloc by lifetime hint |
| Runtime init/shutdown | Implemented | `lgx_runtime_init`, `lgx_runtime_shutdown` |
| Threading | Stub | `src/threading/lgx_threading.c` — job system + fibers exist but untested |
| Graphics | Stub | `src/graphics/lgx_graphics.c` — Vulkan wrapper skeleton |
| Input | Stub | `src/input/lgx_input.c` |
| Audio | Stub | `src/audio/lgx_audio.c` |
| Profiling | Stub | `src/profile/lgx_profile.c` |
| Networking | Stub | `src/net/lgx_net.c` |
| Asset pipeline | Stub | `src/asset/lgx_asset.c` — async loading is sync stub |
| Tooling | Stub | `src/tools/lgx_tools.c` — perf memory tracker |

**Bottom line:** One real module (memory core). Eight satellite modules exist as headers + stubs.

## 3. Test Coverage (Actual, Not Claimed)

| Metric | Value |
|--------|-------|
| Tests registered in CTest | 77 |
| Tests that can actually fail (have assertions/exit codes) | ~51 |
| Tests that pass unconditionally | ~26 |
| Tests disabled/not compiled | ~12 |
| Known memory-safety defects | 3 |
| Chaos testing | Cannot run (self-deadlocks) |

Source: `tests/` directory audit. See `docs/STATE.md` for details.

## 4. Build System

- **Build system:** CMake
- **Libraries:** `liblgx_runtime.so` (core), plus 8 satellite `.so` targets that may or may not compile depending on optional deps (Vulkan, ALSA)
- **pkg-config:** `lgx_runtime.pc` is generated at install time
- **Build directory:** `build/` was previously committed to git; it is now untracked and gitignored

## 5. What Is NOT True (Retracted)

| Claim | Status |
|-------|--------|
| "76/76 tests passing" | False — 26 tests cannot fail |
| "Production-ready" | False — 3 known memory-safety bugs |
| "200× faster than malloc" | Retracted — measured ~1.1–1.6× slower than glibc at small sizes |
| "Zero memory leaks" | Unverified — leak detector itself has a mutex bug |

See `docs/AUDIT_REPORT.md` for the full audit.

---

# What Next — Prioritized Roadmap

## Immediate (This Week)

1. **Fix the 3 memory-safety bugs** — GPU pool handle use-after-free, frame-arena mid-frame growth, chaos-testing self-deadlock. Each needs a regression test that fails first, then passes.
2. **Fix `.gitignore`** — stop tracking `build/` artifacts (already done locally, needs push).
3. **Push to origin** — `main` is 24 commits ahead of `origin/main`; nothing has been pushed since the refactor.

## Phase 1: Correctness (This Month)

| Task | Description | Dependencies |
|------|-------------|--------------|
| Fix GPU pool use-after-free | Store block index, not raw pointer, in public handle | None |
| Fix frame-arena mid-frame growth | Retire old mapping until frame end | None |
| Fix chaos-testing self-deadlock | Release mutex before calling chaos RNG | None |
| Fix intent router GPU routing | Check GPU hint before lifetime | None |
| Guard GPU pool for non-Vulkan builds | Wrap `lgx_gpu_alloc` in `#if HAVE_VULKAN` | None |
| Fix net receive timeout | Refresh `last_recv_ms` on data receive | None |
| Fix leak detector mutex wipe | Don't `memset` over a live mutex | None |
| Add regression tests | One test per bug above | Each bug fix |

## Phase 2: Measurement (Parallel with Phase 1)

| Task | Description | Deliverable |
|------|-------------|-------------|
| Fragmentation matrix | Build+test on Debian 11, 12; Ubuntu 24.04; Fedora 40; Arch. Record glibc version, Vulkan presence, NUMA, `vm.max_map_count`, etc. | `tests/failure_injection/` matrix results |
| Arena performance deep-dive | Count `lock` instructions and calls in the fast path; compare against bump-floor and atomic bump floors already in benchmarks | `docs/05-performance/arena-hotpath-profile.md` |
| Compile-only smoke test | Verify all 9 modules compile on all 4 distros (even if some are stubs) | CI matrix results |

## Phase 3: Platform Decision (After Phase 1 & 2)

Decide based on data:

| If fragmentation matrix shows... | And arena perf shows... | Then |
|----------------------------------|------------------------|------|
| Distro differences are minor | Arena is 2×–8× slower than bump | LGX is a memory library, not a platform |
| Distro differences are major | Arena matches bump floor | LGX is a platform worth shipping |

**Do not** build graphics, input, audio, networking, or asset modules until one of these paths is validated.

## Immediate Next Step

Run the fragmentation matrix on the 4 available distros and read the `lgx_frame_alloc` fast path with `objdump -d` to count `lock` instructions. The data from both will tell you whether LGX is a library or a platform.

---

**Note:** This document reflects the actual state of the codebase as audited. Claims in older docs (`docs/LGX_PLATFORM_VISION.md`, `docs/LAUNCH_BLOG_POST.md`, etc.) may be outdated — they predate this audit.