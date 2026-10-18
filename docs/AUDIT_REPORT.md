# LGX — Technical Audit Report

**Auditor:** Independent review (adversarial, evidence-based)
**Date:** 2026-10-10
**Commit audited:** `a9a68af` (working tree)
**Method:** Fresh out-of-tree build, full test-suite execution (4 runs), benchmark execution, direct source inspection of every critical path. No claim below is taken from documentation — every finding was reproduced or read at the cited line.

---

## 0. Executive Summary

The architecture *thinking* in this project is genuinely above average for its size. The *implementation discipline* is not, and there is a large, systematic gap between what the documentation asserts and what the code does.

Three findings are severe enough to block any external launch:

1. **The public C++ header does not compile.** `include/lgx_runtime.h` fails with a hard syntax error in any C++ translation unit. For a project whose entire premise is a stable C ABI consumed by C++ game engines, this is fatal to the stated model.
2. **The flagship performance claim is contradicted by the project's own benchmark.** The published "200× faster than malloc / 0.04 μs" figure is not reproduced by `benchmarks/benchmark_allocation_throughput`. Measured: frame-arena median **30 ns vs glibc malloc 19 ns** — the arena is *slower* than malloc at the median, not 200× faster.
3. **The project does not build from a clean checkout.** `test_csf2_numa_awareness` fails to link because `CMakeLists.txt` never references `numa` despite the test calling `numa_*`.

None of this means the project should be abandoned. It means the *verification layer* must be rebuilt before the *marketing layer* is extended — the current order is inverted.

---

## 1. Build & Test — Verified Reproduction

### 1.1 Clean build: FAILS

```
cmake -S . -B build-audit -DCMAKE_BUILD_TYPE=Release && cmake --build build-audit
```

Configure succeeds (Vulkan, jemalloc, ALSA all found). Compilation is **warning-clean** — the `-Wall -Wextra -Werror -Wredundant-decls -Wshadow -Wstrict-prototypes -Wmissing-prototypes` posture is real and holding. That is a genuine strength.

Link fails:

```
undefined reference to `numa_free'
undefined reference to `numa_alloc_onnode'
undefined reference to `numa_available'   (… 10 symbols)
collect2: error: ld returned 1 exit status
```

**Root cause:** `tests/phase0/test_csf2_numa_awareness.c` includes `<numa.h>` and calls `numa_*`, but `CMakeLists.txt` contains **zero** references to `numa` (verified by grep across the root and all `tests/*/CMakeLists.txt`). `libnuma` is present on this machine, so this is not an environment issue — it is a missing link dependency in the build system. The project only "builds" today because `build/` contains a **stale prebuilt binary** committed to the repository. That artifact is masking the break.

### 1.2 Test results: 74/76, with an asterisk

| Run | Result |
|---|---|
| Full run 1 | 74/76 — `test_csf2_numa_awareness` (Not Run), `test_threading_basic` (**Failed**) |
| Full run 2 | 75/76 — numa only |
| Full run 3 | 75/76 — numa only |
| Full run 4 | 75/76 — numa only |

`test_threading_basic` failed once, then passed. Isolated: 5/5 pass. Under 8-way CPU load: pass. **Deliberate repeat: 1 failure in 60 runs (exit code 1).** This is a genuine intermittent failure — a ~1.7% flake rate in the threading module. A concurrency primitive that fails 1-in-60 under light conditions is not shippable into a job system; this is the highest-signal correctness defect in the codebase and it is currently invisible to CI because it does not reproduce reliably.

### 1.3 The suite cannot fail — 37 of 100 test files are structurally incapable of reporting failure

A file was classified "cannot-fail" if it contains no non-zero exit path and no failure counter. Result: **37/100**, including:

- **All 4 performance tests** (`test_allocation_latency`, `test_frame_time_contribution`, `test_initialization_time`, `test_memory_footprint`, `test_memory_overhead`) — these "pass" by construction and measure nothing
- **All 7 module smoke tests** (`graphics`, `input`, `audio`, `net`, `profile`, `asset`, `tools`)
- **4 of 5 threading tests** (`basic`, `stress`, `fiber`, `advanced`)
- **`tests/phase0/test_frame_arena.c`** — 363 lines, **0 assertions, 0 exit codes**
- `test_csf4_abi_stability`, `test_memory_safety`, `test_namespace_isolation`, `test_suspend_resume`

By contrast, `test_init_shutdown.c` (38 assertions), `test_persistent_heap.c` (27), and `test_intent_allocator.c` (26) are real tests. So the suite is bimodal: a solid core of genuine tests surrounded by a large ring of print-only programs.

**Consequence:** the headline "76/76 passing (100%)" — repeated in `docs/README.md`, `LGX_PLATFORM_VISION.md`, and `docs/PROJECT_STRUCTURE.md` — is not a quality signal. Roughly 30 of those 76 are structurally incapable of failing. The correct statement is "43 verified tests, 0 known failures, significant unverified surface."

### 1.4 Test-suite rot: the tests were written against an API that no longer exists

Twelve tests are commented out of the build:

| Category | Disabled | Reason given in source |
|---|---|---|
| `tests/unit/` | 3 of 8 | "Compilation errors"; "Uses non-existent `lgx_version_info_t` API" |
| `tests/integration/` | 5 of 6 | "Compilation errors"; "API mismatches"; "non-existent API" |
| `tests/failure_injection/` | 4 of 5 | "Not implemented"; "Compilation errors" |

Integration coverage is **1 test out of 6 files**. The stated causes — non-existent API, compilation errors — are not environmental; they mean the public API changed and the tests were not migrated. Commenting out a test that fails to compile is how a suite silently stops testing anything.

---

## 2. The Performance Claim — Verified Against the Project's Own Benchmark

Executed: `./benchmarks/benchmark_benchmark_allocation_throughput` (10,000 iterations/size).

### 16-byte allocations

| Allocator | Median | P99 | Throughput | vs. malloc |
|---|---|---|---|---|
| `malloc` | 19 ns | 20 ns | 51.5 M ops/s | baseline |
| `lgx_alloc` (general) | **1694 ns** | **1834 ns** | 0.58 M ops/s | **~89× SLOWER** |
| `lgx_frame` (frame arena) | **30 ns** | 40 ns | 28.1 M ops/s | **~1.6× slower** |
| `lgx_persistent` | 73 ns | 83 ns | 11.0 M ops/s | ~3.8× slower |

### 64-byte allocations

| Allocator | Median | P99 |
|---|---|---|
| `malloc` | 19 ns | 20 ns |
| `lgx_frame` | 37 ns | **1015 ns** |

### What this means

- **The general allocator `lgx_alloc` is roughly 89× slower than glibc malloc.** It is on the default path for persistent allocations and is the function a developer reaches for by name. Whatever the intent-based router is supposed to prevent, a developer who calls `lgx_alloc` directly gets a severe regression.
- **The frame arena is not 200× faster than malloc.** At the median it is ~1.6× *slower* than glibc's hot fastbin path. At 64 bytes its P99 degrades to ~1 μs — a 50× tail-latency penalty versus malloc, in a project whose stated purpose is eliminating frame-time spikes.
- **The "200×" figure is a comparison against a number that this benchmark does not produce.** The published claim implicitly compares LGX's ~40 ns against an asserted malloc cost of 10–30 μs. The measured malloc cost for a small hot allocation is ~19–20 ns. The 200× is therefore not a measurement; it is a comparison to a straw-man baseline.

### The benchmark methodology is also unsound for the claim it carries

```c
static void bench_lgx_frame(void* ctx) {
    void* ptr = lgx_alloc_frame(actx->size);
    if (ptr) { memset(ptr, 0, actx->size); }
    // Frame allocations are not freed individually
}
```

The frame-arena benchmark **never calls `lgx_frame_reset()`**. Across 10,000 iterations it therefore performs monotonically growing bump allocation over ever-fresh memory, with a `memset` touching every byte. Real frame-arena usage is the opposite pattern: allocate → use → **reset** → reuse the same hot, already-resident memory.

Consequently the benchmark does not measure the three things a frame arena actually buys:
1. elision of `free()`,
2. reuse of hot cache-resident memory,
3. elimination of fragmentation and long-run variance.

It measures cold monotonic growth — the pattern least favorable to the technique being sold. **Even if the technique is sound, this benchmark cannot demonstrate it.** This is the most consequential methodological defect in the repository, because it is the evidence base for every published number.

*In fairness:* the underlying thesis — that eliding `free()` and reusing hot memory beats general-purpose allocation for frame-scoped data — is a well-established and correct idea in real-time systems. The thesis is not the problem. The measurement is.

---

## 3. Memory Safety — Direct Source Verification

### 3.1 CRITICAL — `lgx_runtime.h` does not compile as C++

```
$ g++ -I include -c test.cpp        # test.cpp: #include "lgx_runtime.h"
include/lgx_runtime.h:379:1: error: expected declaration before '}' token
  379 | }
$ gcc -I include -c test.c           # C: OK
```

**Root cause:** one `extern "C" {` opener at line 11, but **two** closers — line 311 and line 379. Lines 50–144 declare the inline convenience functions as "outside `extern "C"` for C++ compatibility" but never open a corresponding block, so they remain inside the C-linkage region and the brace accounting is off by one.

This is a hard compile error, not a linkage warning. Consequences:
- No C++ game can include the primary public header.
- The architecture specification (`docs/LGX_PLATFORM_ARCHITECTURE.md` §2) mandates a "C API with C++ wrappers" as a core principle. The C++ half of the contract is inoperable.
- The published launch case study claims a working SuperTuxKart integration (SuperTuxKart is C++) that adds `#include <lgx_runtime.h>`. **That integration could not have compiled against this header.** The claimed proof point did not occur.

Severity: **blocker**. This is a 10-character fix (`extern "C++" {` at line 51, or delete the stray `}` at 379), but it invalidates the project's central published claim.

### 3.2 CRITICAL — GPU pool use-after-free

Verified by reading the full allocation/free lifecycle:

- `lgx_gpu_alloc` (`lgx_gpu_pool.c:935`): `alloc->block = block;` — stores a **raw pointer** into `allocator->all_blocks[]` into a handle that outlives the lock.
- `all_blocks` is allocated with `max_blocks = 1024` (`:165`). On split, when `num_blocks >= max_blocks`, `:248` executes `realloc(allocator->all_blocks, new_max * sizeof(...))` — **the array moves**.
- `lgx_gpu_free` (`:975`) dereferences the stale pointer: `g_gpu_pool.used_budget[type] -= alloc->block->size;`
- `lgx_gpu_free` (`:978`) passes it to `buddy_free`, which computes `block - allocator->all_blocks` (`:495`) on freed memory to derive a garbage index.

**Trigger:** allocate enough blocks to force a `realloc` (>1024 buddy splits), retain an earlier handle, free it → read-after-free and buddy-list corruption.

Note the internal discipline is genuinely good — the buddy allocator itself saves and restores `block_index` around every `realloc`, with explicit `// CRITICAL: Save block index before any operations (realloc may move memory)` comments. **The internals are index-safe; the public handle boundary is not.** This is a well-implemented core with an unsafe API surface — the most dangerous class of defect, because the careful code creates false confidence.

### 3.3 CRITICAL — Frame arena mid-frame growth invalidates outstanding pointers

`grow_arena` (`lgx_frame_arena.c:359–391`): allocates a new base, `memcpy`s the contents, then calls `free_huge_page_arena(arena->base, arena->capacity)` — unmapping the old region — and republishes `arena->base`.

Every pointer previously returned by `lgx_frame_alloc()` **during the current frame** points into the unmapped region. Any live reference is a dangling pointer. A frame that grows its arena mid-frame silently invalidates its own allocations, with no error returned.

### 3.4 HIGH — Chaos-testing self-deadlock

- `chaos_random()` (`lgx_chaos_testing.c:63–65`) locks `g_chaos_state.mutex`.
- `lgx_chaos_should_fail_allocation()` (`:178`) **already holds** `g_chaos_state.mutex`, then calls `chaos_random()` at `:188`.
- The mutex is initialized with `pthread_mutex_init(&g_chaos_state.mutex, NULL)` (`:44`) — default attributes, i.e. **non-recursive**.

Result: guaranteed self-deadlock (undefined behavior; in practice a hang) on the first chaos check. This repeats at `:212–224`, `:263–270`, `:291–298`.

The feature is dormant because chaos testing is off by default — which is precisely why 74/76 tests pass. **The chaos-testing subsystem cannot be enabled at all**; it is a landmine that is also a core claimed feature ("chaos testing" appears in the security and testing documentation).

### 3.5 HIGH — `lgx_alloc_gpu_shared()` never allocates GPU memory

- `include/lgx_runtime.h:133–143`: the public inline sets `intent.lifetime = LGX_LIFETIME_FRAME` **and** `intent.hint = LGX_HINT_GPU_SHARED`.
- `lgx_intent_allocator.c:268–278`: the router evaluates `if (lifetime == LGX_LIFETIME_FRAME)` **first**; the `else if (hint == LGX_HINT_GPU_SHARED)` branch is only reached when lifetime is *not* FRAME.

Because the public API sets both fields, `lgx_alloc_gpu_shared()` **always routes to the frame arena**. The GPU-hint branch is unreachable from the public entry point. A caller asking for GPU-visible shared memory receives ordinary CPU frame-arena memory that will be unmapped in three frames and was never host-visible.

This is a silent wrong-behavior defect on a flagship API, in a header marked `static inline` for performance. The comment "Priority: lifetime > performance hint" documents the intent, but the intent itself is wrong: a GPU-sharing hint must dominate a frame lifetime.

### 3.6 HIGH — Non-Vulkan builds cannot link

`lgx_intent_allocator.c:278` calls `lgx_gpu_alloc(...)` with **no** `#if HAVE_VULKAN` guard, while lines 152 and 195 in the same file *are* guarded. `lgx_gpu_pool.c` is only compiled when Vulkan is found (`CMakeLists.txt:91–94`). Therefore on any machine without Vulkan development headers the library fails to link. The build only succeeds here because Vulkan is installed. The declared feature — "Vulkan not found - GPU memory pool features will be disabled" — is not actually implemented.

### 3.7 HIGH — 21 declared public functions have no implementation

`include/lgx_integration.h` is installed as a `PUBLIC_HEADER` (`CMakeLists.txt:109`). Cross-referencing every `lgx_*` declaration against all of `src/`, **21 functions have zero definitions**, including:

`lgx_translate_alloc`, `lgx_translate_free`, `lgx_translate_alloc_aligned`, `lgx_runtime_get_translate_context`, `lgx_runtime_register_security_hooks`, `lgx_runtime_unregister_security_hooks`, `lgx_runtime_configure_shader_cache`, `lgx_runtime_load_component`, `lgx_runtime_unload_component`, `lgx_runtime_is_component_loaded`, `lgx_runtime_register_plugin`, `lgx_runtime_unregister_plugin`, `lgx_runtime_send_message`, `lgx_runtime_register_message_handler`, `lgx_runtime_register_event_handler`, `lgx_runtime_unregister_event_handler`, `lgx_runtime_emit_event`, `lgx_runtime_share_resource`, `lgx_runtime_get_shared_resource`, `lgx_runtime_unshare_resource`.

Any consumer that includes the installed header and calls any of these gets a link error at integration time. The architecture document (`§5.2`) presents these registration functions as the mechanism by which modules integrate — they are the published extension model, and they are unimplemented.

### 3.8 MEDIUM — Networking connections always time out

`last_recv_ms` is written in exactly one place — `lgx_net_connect` (`:212`) — and read in exactly one place — the timeout check in `lgx_net_update` (`:242`). It is never refreshed when data arrives. Every connection is therefore marked inactive after `timeout_ms` regardless of traffic. Any feature built on `lgx_net` receive paths is non-functional.

Adjacent: `:244` emits `printf("[LGX NET] Connection %u timed out…")` from library code — an unsolicited write to stdout from a shared library, which is unacceptable in a library that other people's games link.

### 3.9 MEDIUM — Leak detector corrupts its own mutex

`lgx_leak_detector.c:60–69`: `pthread_mutex_init()` is followed by `memset(&g_leak_detector, 0, sizeof(...))`, which zeroes the initialized mutex. Every subsequent lock operates on a zeroed (uninitialized) mutex object.

---

## 4. Test & Verification Practice — Assessment

| Practice | Status |
|---|---|
| Compiler warnings as errors | **Genuinely enforced.** Zero warnings across a 72-target build. Real strength. |
| Sanitizers | ASan + UBSan wired into Debug (`CMakeLists.txt:17`) |
| Unit/integration separation | Structure exists; integration coverage is 1 of 6 files |
| ABI testing | Present (`tests/abi/`, 3 files) but `test_csf4_abi_stability` cannot fail |
| Fuzzing | 3 libFuzzer/AFL harnesses with 4 seed corpora — genuinely above average |
| Failure injection | 4 of 5 disabled; the enabled one (`test_library_version_mismatch`) is the least interesting of the five |
| Test migration discipline | **Failed** — 12 tests disabled for "compilation errors"/"API mismatch" rather than migrated |
| Assertion discipline | **Failed** — 37% of files cannot report failure |

The pattern across the whole repository is consistent: **verification is performed at the moment of writing and then not maintained.** Warnings are clean because `-Werror` enforces them continuously. Tests are stale because nothing enforces them continuously. Documentation is aspirational because nothing verifies it against the code. The project has strong *setup-time* rigor and weak *maintenance-time* rigor.

---

## 5. Candid Assessment — Depth and Substance

The user asked for an unvarnished judgment. Here it is.

### What is genuinely deep

- **The architectural reasoning is real.** Stable-ABI discipline (`struct_size`-first structs, append-only evolution, ELF symbol versioning via `.map` files, a documented deprecation process) is the correct set of instincts for a platform, and it is applied consistently in the headers and build system. Most projects at this size have none of this.
- **The buddy allocator is well-engineered.** Index-based pointer discipline across `realloc`, explicit free-list rebuild, split/coalesce correctness, comments that explain *why* rather than *what*. This is professional-grade code.
- **The intent-based allocation API is a genuinely novel idea** — declaring lifetime and letting the system route to the right allocator. It is the most interesting design idea in the project and it is under-exploited and under-tested relative to its importance.
- **Fuzzing harnesses, chaos testing, leak detection, guard pages, canaries** — the *instinct* for production hardening is well ahead of most projects of this size. The problem is execution (see chaos deadlock), not the intent.
- **Scale of ambition is legitimate.** Nine modules, 28 runtime subsystems, ~17k lines. That is real engineering volume.

### What is shallow

- **The verification layer is decorative.** 37% of tests cannot fail. The pass rate is not evidence. The performance benchmark does not measure the technique it sells. The ABI test cannot fail. This is the deepest problem in the project.
- **Documentation has decoupled from reality.** Three different test counts (59/59, 64/64, 76/76) across three documents. "Production-Ready" in banners. A launch case study describing a C++ integration that cannot compile against the shipped header. Performance tables whose numbers the in-repo benchmark does not reproduce. This is not "simplistic claims" — it is a documentation layer that was allowed to run ahead of the code with nothing to catch up.
- **The codebase has a bimodal quality distribution.** A few files are careful and professional; several contain defects that would be caught in the first hour of review at any company (unbalanced braces, non-recursive mutex self-deadlock, raw pointers across `realloc`, a `memset` over a live mutex). There is no consistent standard — it is a strong engineer at speed, without review, without CI gates that actually run.
- **CI is decorative in the same way.** It was reduced from 427 to 126 lines; the formatting and static-analysis jobs were removed; and the remaining checks reference `lgx_arena_example` and `lgx_arena.h`, which do not exist. CI cannot currently pass.

### The verdict

**This is an ambitious, well-conceived prototype whose verification apparatus has failed to keep pace with its ambition.** The design intelligence is real and above the norm for the project's size. The engineering *practice* — specifically, the discipline of maintaining tests, benchmarks, and claims against a moving codebase — has not kept up, and the gap is now large enough that the project's central commercial claim is contradicted by its own tooling.

The most dangerous property is not any individual bug. It is that **the project's own instruments report success.** The build is clean, the tests pass 97%, the documentation is comprehensive, the benchmarks produce plausible-looking tables — and underneath, the header doesn't compile for C++, the flagship allocator is 89× slower than the baseline it claims to beat, a third of the tests cannot fail, and a third of the integration surface doesn't exist. An organization shipping this would not learn about any of it until a user did.

This is fixable, and quickly — several of the most severe items are single-line fixes. But it requires inverting the current order of operations: **stop extending the story, rebuild the ability to know what is true.**

---

## 6. Severity-ranked Remediation

### Blockers (before any external claim)

| # | Issue | Location | Effort |
|---|---|---|---|
| B1 | `lgx_runtime.h` fails to compile as C++ (stray `}` / missing `extern "C++" {`) | `include/lgx_runtime.h:379` | ~10 min |
| B2 | Clean-checkout build fails; `numa` never linked | `CMakeLists.txt` (add `find_library(NUMA)`/`target_link_libraries`) | ~20 min |
| B3 | Retract or re-derive the "200× faster" claim; fix benchmark to use the real allocate→use→reset pattern | `benchmarks/benchmark_allocation_throughput.c`, all docs | ~1 day |
| B4 | GPU pool use-after-free — store block **index** in the handle, not a pointer | `lgx_gpu_pool.c:935, 975–978` | ~2 h |
| B5 | Frame-arena growth invalidates live pointers — allocate new base without freeing old until frame end | `lgx_frame_arena.c:359–391` | ~4 h |

### High

| # | Issue | Location |
|---|---|---|
| H1 | Chaos self-deadlock (recursive mutex or hoist RNG state) | `lgx_chaos_testing.c:44, 63, 188` |
| H2 | `lgx_alloc_gpu_shared` routes to frame arena — check hint before lifetime | `lgx_intent_allocator.c:268`; `lgx_runtime.h:133` |
| H3 | Guard `lgx_gpu_alloc` with `#if HAVE_VULKAN` | `lgx_intent_allocator.c:278` |
| H4 | Implement or remove the 21 declared functions; stop installing a header that lies | `include/lgx_integration.h` |
| H5 | Refresh `last_recv_ms` on receive; remove `printf` from library | `src/net/lgx_net.c:212, 242, 244` |
| H6 | Investigate the 1-in-60 threading flake (likely the fiber/job paths) | `src/threading/lgx_threading.c` |

### Medium

| # | Issue | Location |
|---|---|---|
| M1 | Leak detector `memset` over live mutex; `size_t` underflow | `lgx_leak_detector.c:60–69, 136` |
| M2 | Persistent heap: slab bitmap/`used` never updated on free; TOCTOU read in `lgx_heap_free` | `lgx_persistent_heap.c:1393–1436` |
| M3 | Give the 37 cannot-fail tests real assertions and non-zero exits | `tests/` |
| M4 | Re-enable and migrate the 12 disabled tests; fix or delete them | `tests/*/CMakeLists.txt` |
| M5 | Memory-manager leak tracking compares slot address, not user pointer | `lgx_memory_manager.c:1911, 1961` |
| M6 | Health monitor reads uninitialized `huge_pages_available`; hardcoded `memory_usage_mb` | `lgx_health_monitor.c:161, 187` |
| M7 | Graphics may pass NULL extension pointers to `vkCreateInstance` | `src/graphics/lgx_graphics.c:294–298` |
| M8 | Tools ring-buffer percentile math corrupts after wrap | `src/tools/lgx_tools.c:182` |

### Low / hygiene

- Telemetry hardcodes `"lgx_version": "1.0.0"` (actual 1.0.1) — `lgx_telemetry.c:746`
- `printf` in library code (net module, others)
- Foreign placeholder text in `lgx_memory_manager_shutdown` doc comment (`lgx_memory_manager.c:419–437`)
- ~150 lines of commented-out slab pool; dead `tls_trace_stack`; dead `pool_alloc` declaration
- Untrack `build/` (hundreds of committed artifacts masking the build break); restore a correct `.gitignore`
- Restore `README.md` / `CHANGELOG.md` / `CONTRIBUTING.md` (deleted in working tree)
- Reconcile the three conflicting test counts across documentation

---

## 7. What Would Change This Verdict

The project's fundamentals are sound enough that this is a **practice** problem, not a **conception** problem. The evidence that it is recoverable:

- The build is already warning-clean under `-Werror` — the toolchain discipline exists.
- The buddy allocator, lock-free pool, SIMD ops, and capability detection are clean, well-commented, professional code.
- The Fuzzing harnesses and sanitizers are already in place and unused — the verification instinct is present.
- Several blockers are single-line fixes.

The single highest-leverage action is not any individual bug fix. It is to **make the instruments tell the truth**: fix the benchmark methodology, give the tests the ability to fail, restore CI so it actually runs, and let those numbers — whatever they turn out to be — drive the roadmap. The 200× figure may well survive an honest measurement of the right workload. It certainly cannot survive anything else.

---

*Verification artifacts: fresh out-of-tree build at `/tmp/opencode/lgx-audit`; 4 full ctest runs; 60-iteration flake reproduction; benchmark execution; manual source inspection of all cited lines.*
