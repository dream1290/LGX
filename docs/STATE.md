# LGX — The State

*Where the project actually stands right now: git, build, tests, and the honest gap between the story and the code.*

---

## 1. Executive Summary

The project is at a **strategic fork**. The vision is fully formed, nine modules are built and link, 76 tests are registered — but the repository is not launch-ready, the advertised entry point (`lgx_arena.h`) does not exist, the CI would fail today, and a close audit of the runtime code found multiple memory-safety and correctness bugs that contradict the "Production-Ready" claim.

---

## 2. Git State (dirty, diverged, artifacts committed)

```
Branch:      main
Diverged:    1 commit ahead, 1 commit behind origin/main
Commits:     41 (HEAD: a9a68af "feat: Add LGX Tooling v2.2")
```

**Working tree anomalies:**

| Item | State |
|---|---|
| `LGX_ARENA_SINGLE_HEADER_SPEC.md` | Staged as new, then **deleted from the working tree** (recoverable via `git show :LGX_ARENA_SINGLE_HEADER_SPEC.md`) |
| `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md` | **Deleted, unstaged** (exist in git history) |
| `build/` | **Hundreds of build artifacts are tracked in git** and show as modified — the build directory was committed at some point |
| `.gitignore` | Gutted: removed `/build/`, `packaging/output/`, `/scripts/`, security-audit patterns — while dozens of build artifacts remain tracked |
| `.github/workflows/ci.yml` | Drastically simplified (427 → 126 lines); formatting/static-analysis/job jobs removed |
| `packaging/debian/control` | Homepage/VCS switched from `lgx-platform/LGX` → `dream1290/LGX`; added `libc6-dev` build-dep |
| `src/` modifications | Minor: extra log line in `lgx_runtime_core.c`, completion-guarantee comment + acquire fence in `lgx_threading.c`, small fixes in graphics/input/tools |

**Untracked but present:**
- `scripts/` (15 scripts) and `tools/lifetime-profiler/`
- `tests/Makefile` + `test_basic.c`, `test_alignment.c`, `test_statistics.c`, `test_overflow.c`, `test_framework.h` — arena tests that `#include "../lgx_arena.h"` (**file does not exist**)
- `packaging/output/*.deb` (two built packages)
- `docs/09-development/CAD_3D_EXPANSION_ANALYSIS.md`, `docs/LAUNCH_BLOG_POST.md`, `docs/LAUNCH_SOCIAL_MEDIA.md`

---

## 3. Build State

**It builds.** All nine shared libraries compile and link in `build/` (Vulkan and ALSA were found; GPU pool compiled):

```
liblgx_runtime.so.1.0.1   liblgx_threading.so.1.1.0   liblgx_graphics.so.1.2.0
liblgx_input.so.1.3.0     liblgx_audio.so.1.4.0      liblgx_profile.so.1.5.0
liblgx_net.so.2.0.0       liblgx_asset.so.2.1.0      liblgx_tools.so.2.2.0
```

- Release: `-O2 -DNDEBUG -march=native`; Debug: ASan + UBSan + AVX2
- `-Wall -Wextra -Werror` + `-Wredundant-decls -Wshadow` + strict/missing prototypes on `lgx_runtime` and `lgx_threading`
- Symbol versioning via `lgx_runtime.map` / `lgx_threading.map`
- 76 tests registered in ctest

**Hidden build risk:** `lgx_intent_allocator.c` calls `lgx_gpu_alloc` unconditionally, but `lgx_gpu_pool.c` is only compiled when Vulkan is found → **linking would fail in non-Vulkan builds** (not currently exercised).

---

## 4. Test State

- **76 tests registered** across unit, integration, abi, performance, failure-injection, threading, phase0, module tests, and benchmarks.
- **Broken by design:** `tests/failure_injection/CMakeLists.txt` disables 4 of 5 tests ("Not implemented"); the arena tests in `tests/` root reference a header that doesn't exist; **CI checks for `./lgx_arena_example` and `lgx_arena.h` which do not exist** — CI would fail today.
- The docs claim "64/64" and "76/76 passing" and "0 bytes leaked" in different places; the honest current number is unverified since the tree is mid-refactor.

---

## 5. The Code Audit — What "Production-Ready" Actually Looks Like

A line-by-line audit (documented in `docs/CODEBASE.md`) found the following known defects:

### Memory safety (critical)
- **`lgx_gpu_pool.c` use-after-free** — public handles store a raw `buddy_block_t*` into the `all_blocks` array; a later split `realloc`s that array, dangling every outstanding handle → `lgx_gpu_free` reads freed memory.
- **`lgx_frame_arena.c` mid-frame growth** — `grow_arena` munmaps the old base; all pointers handed out that frame become dangling.
- **`lgx_chaos_testing.c` self-deadlock** — holds the chaos mutex while calling `chaos_random()`, which locks the same non-recursive mutex.
- **`lgx_leak_detector.c`** — `memset` after `pthread_mutex_init` wipes the mutex; `size_t` underflow when frame-arena bytes decrease.
- **`lgx_persistent_heap.c`** — small-alloc frees never clear the slab bitmap or update `used`; `slab_free` is dead code; `lgx_heap_free` reads header fields without the mutex (TOCTOU).

### Correctness (high)
- **Intent allocator never uses the GPU pool** — the `lifetime == FRAME` check precedes the `hint == LGX_HINT_GPU_SHARED` check, so `lgx_alloc_gpu_shared()` always routes to the frame arena.
- **Leak tracking never matches** — compares the user pointer against the address of the tracking-array *slot*, not the allocation.
- **Health monitor** reads `huge_pages_available` that hardware adapter never sets (uninitialized), and hardcodes `memory_usage_mb = 150`.
- **Net module** — `last_recv_ms` is never updated on receive; every connection times out after 5s regardless of traffic.
- **Tools ring buffer** — percentile math reads stale data after the history ring wraps.
- **Threading fibers** — scheduler stores a pointer to a stack-local ucontext (resumed later against a stale address); `lgx_fiber_wait_job` has no wake path.
- **Graphics** — Vulkan extension list can pass NULL pointers to `vkCreateInstance` when platform macros are missing.
- **Telemetry** — hardcoded `"lgx_version": "1.0.0"` (actual 1.0.1).
- **`lgx_runtime.h`** — `extern "C"` block closes mid-header, leaving ~60 declarations C++-mangled in C++ builds.

### Gaps
- **~15 functions declared in `lgx_integration.h` have zero implementations** (translate allocator, security hooks, plugin/message/event registration) — and the header is installed, so consumers get link errors.
- **Asset async loading is a stub** (sync with a LOADING state).
- **Namespace isolation** — `has_namespace_privileges` actually calls `unshare(CLONE_NEWNS)` (self-acknowledged hack).

### Hygiene
- Foreign placeholder text ("Toosilan Kartano (Suinula, Kangasala)…") in `lgx_memory_manager_shutdown`'s doc comment.
- ~150 lines of commented-out slab pool code in `lgx_memory_manager.c`; dead code in `lgx_trace_events.c`.
- Three mutually inconsistent API drafts for the arena (spec's global API vs. test files' handle API vs. runtime's `lgx_frame_alloc(size)`), and the name `lgx_alloc` in the arena spec would collide semantically with the runtime's persistent `lgx_alloc`.

### Verified working well
The clean subsystems deserve credit: `lgx_gpu_capability.c`, `lgx_runtime_core.c`, `lgx_lockfree_pool.c`, `lgx_simd_ops.c`, `lgx_capability_detector.c`, `lgx_input_validation.c`, buddy split/coalesce in the persistent heap, the lock-free queue / job-wait acquire ordering in threading.

---

## 6. Documentation State

- **Strong**: vision, architecture spec, API reference, integration guides, testing docs, packaging docs, 10 numbered directories.
- **Current work**: launch content already drafted (`docs/LAUNCH_BLOG_POST.md` — the SuperTuxKart integration story; `docs/LAUNCH_SOCIAL_MEDIA.md` — HN/Reddit/Twitter posts for the arena launch), plus a 3D/CAD market expansion analysis (`CAD_3D_EXPANSION_ANALYSIS.md`).
- **Contradictions**: docs claim 59/59, 64/64, and 76/76 tests in different places; README/CHANGELOG/CONTRIBUTING deleted from the tree; docs still reference `../CONTRIBUTING.md` and `CHANGELOG.md` links.

---

## 7. What This Means

The project has **real engineering substance** and **real momentum in strategy**, but it is in an inconsistent intermediate state:

1. The **platform code** was written with ambition and mostly works, but carries memory-safety bugs that must be fixed before any external claim of "production-ready" is made.
2. The **arena pivot** (the chosen path to adoption) has docs, marketing, tests, and CI hooks — but no `lgx_arena.h`. The repo currently cannot launch as-is.
3. The **repository hygiene** (tracked build artifacts, deleted root docs, gutted `.gitignore`, simplified CI) is mid-cleanup and needs to be completed or reversed deliberately.

Every item in this document is actionable. The next file — `DIRECTION.md` — lays out the paths forward.
