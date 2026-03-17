# LGX Runtime Core — Careful Refactoring Plan

> Goal: Fix real bugs, eliminate technical debt, and harden code quality — **without breaking any existing tests**.
> Strategy: Ordered phases from highest-risk/lowest-effort fixes to lowest-risk structural improvements.
> Evidence: Every change below is grounded in a specific file and line number from the actual codebase.

---

## Overview of Phases

| Phase | Scope | Risk | Effort |
|-------|-------|------|--------|
| **P0** — Repo Hygiene | [.gitignore](file:///home/karl/Projects/LGX/.gitignore), binary/large files | Zero | 30 min |
| **P1** — Correctness Bugs | Real bugs that silently misbehave | Medium | 2-3 hrs |
| **P2** — Code Cleanliness | Duplicate declarations, dead code | Low | 2-3 hrs |
| **P3** — Hardening | Compiler flags, API surface | Low | 1-2 hrs |

---

## Phase 0 — Repo Hygiene (Do This Today)

These are zero-risk changes affecting only the repository state, not the compiled binary.

---

### [MODIFY] [.gitignore](file:///home/karl/Projects/LGX/.gitignore)

**Problem:** [.gitignore](file:///home/karl/Projects/LGX/.gitignore) already lists [test_results.txt](file:///home/karl/Projects/LGX/tests/test_results.txt) on line 44, but the file **is already committed** to the repository. Adding it to [.gitignore](file:///home/karl/Projects/LGX/.gitignore) doesn't remove an already-tracked file.

**Fix:** Remove it from git tracking with `git rm --cached`.

```bash
git rm --cached tests/test_results.txt
git commit -m "chore: remove 275MB test_results.txt from tracking"
```

Also add these missing patterns to [.gitignore](file:///home/karl/Projects/LGX/.gitignore):

```diff
+# Binary build outputs in repo root
+/test_lgx
+/test_frame_debug
+/test_frame_debug_full
+/test_frame_stress
+/test_adaptive
+*.tar.gz
```

> **Note:** [lgx-runtime-1.0.1.tar.gz](file:///home/karl/Projects/LGX/lgx-runtime-1.0.1.tar.gz) (3.4MB) in the root is already partially covered by `*.tar.gz` but the pattern only appears in the packaging section. Confirm whether this tarball should be tracked or generated.

---

### [MODIFY] Directory: `lead-engineer-new-verdict/`

**Problem:** Confirmed in [.gitignore](file:///home/karl/Projects/LGX/.gitignore) line 59 as `lead-engineer-new-verdict/` — so this is correctly ignored from future commits. **But confirm it's not tracked:**

```bash
git ls-files lead-engineer-new-verdict/
```

If any output appears, run:
```bash
git rm -r --cached lead-engineer-new-verdict/
git commit -m "chore: remove planning docs from tracking"
```

---

## Phase 1 — Real Correctness Bugs (Highest Priority)

These are actual bugs that cause incorrect silent behavior.

---

### Bug 1: `use_huge_pages` Flag Is Set Then Immediately Overridden

**File:** [lgx_memory_manager.c](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c)
**Lines:** 347–349 (detects and sets flag correctly), then 390 (immediately overrides it to `false`)

```c
// Line 347-349: Correct — sets flag based on hardware
} else {
    mgr->use_huge_pages = lgx_hugepages_available();
}

// Line 390-391: BUG — throws the result away
mgr->use_huge_pages = false; // Will be set based on hardware detection  ← DELETE THIS
mgr->numa_aware = false;     // Will be set based on hardware detection  ← DELETE THIS
```

**Fix:** Remove lines 390-391 entirely. The TODO comment indicates this was a placeholder that was never replaced with real logic, but the real logic (lines 347-349) already exists above it.

**Impact:** Users on huge-page-capable systems (most modern Linux) silently lose the TLB optimization that the README advertises.

---

### Bug 2: [lgx_free()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685) Silently Does Nothing for Thread-Pool Pointers

**File:** [lgx_memory_manager.c](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c)
**Lines:** 746–755

```c
// If address falls in thread pool range, just return without free
// Comment: "this will show as a 'leak' but it's expected"
```

**The actual problem:** This is architecturally correct (bump allocators don't support per-pointer free), but [lgx_alloc()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#611-622) can silently serve pointers from the thread pool without any indication to the caller. When the caller calls [lgx_free()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685), it silently does nothing. This means:

1. Memory stats (allocation/deallocation counters) become inaccurate — `total_allocations` increments but `total_deallocations` never does for these pointers.
2. There is no way for a developer to know whether their [lgx_free()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685) actually freed memory.

**Fix: Track deallocation count correctly for pool-origin pointers:**

```c
if (thread_pools[0].base && ptr >= thread_pools[0].base && 
    ptr < (void*)((char*)thread_pools[0].base + total_pool_size)) {
    // Pool bump allocations are reclaimed as a block at shutdown.
    // Track the "free" so stats remain consistent.
    atomic_fetch_add(&manager->total_deallocations, 1);  // ADD THIS
    return;
}
```

And **add a comment to [lgx_runtime_internal.h](file:///home/karl/Projects/LGX/include/lgx/lgx_runtime_internal.h)** near the thread pool definition explaining this behavior.

---

### Bug 3: [lgx_free()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685) for Aligned Allocations Goes Through Wrong Path

**File:** [lgx_memory_manager.c](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c)  
**Lines:** 806-826 ([lgx_memory_manager_alloc_aligned](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#803-827)) vs. 653-684 ([lgx_free](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685))

[lgx_alloc_aligned()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#623-634) uses `posix_memalign()` directly (line 815). But [lgx_free()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685) checks whether a pointer is from the frame arena, then the persistent heap, then falls through to [lgx_memory_manager_free()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#733-802). [lgx_memory_manager_free()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#733-802) then checks if the pointer is from thread pools.

A pointer from `posix_memalign` will eventually reach [free(ptr)](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685) at line 800 — which is correct. But it will also go through the expensive hot-path cache scan first (lines 758-772). This is wasteful but not a bug.

**Fix (minor):** In [lgx_memory_manager_alloc_aligned()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#803-827), call `atomic_fetch_add(&manager->total_allocations, 1)` **before** `posix_memalign`, consistent with how [lgx_memory_manager_alloc()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#686-732) does it. Currently it increments after (line 824), inconsistent with the hot-path counter increment at line 695.

---

### Bug 4: Unit Test Uses Wrong Field Names on `lgx_memory_stats_t`

**File:** [tests/unit/test_memory_allocation.c](file:///home/karl/Projects/LGX/tests/unit/test_memory_allocation.c)  
**Lines:** 241-243

```c
printf("    Total freed: %zu bytes\n", stats.total_freed);    // ← field doesn't exist
printf("    Current usage: %zu bytes\n", stats.current_usage);  // ← field doesn't exist
```

But `lgx_memory_stats_t` in [lgx_types.h](file:///home/karl/Projects/LGX/include/lgx_types.h) defines:
```c
uint64_t total_deallocated;   // NOT total_freed
uint64_t current_allocated;   // NOT current_usage
```

Similarly at line 254-256 for `lgx_frame_arena_stats_t`:
```c
printf("    Current usage: %zu bytes\n", stats.current_usage);  // ← no such field
printf("    Peak usage: %zu bytes\n", stats.peak_usage);        // ← no such field
```

The frame_arena_stats_t (internal) has [current_frame](file:///home/karl/Projects/LGX/src/runtime/lgx_frame_arena.c#928-934), `total_allocations`, etc., but not [current_usage](file:///home/karl/Projects/LGX/src/runtime/lgx_frame_arena.c#935-948).

**Fix:** Correct all field names in the unit test to match the actual struct definitions. This was likely masked because the test doesn't fail to compile — the field access is guarded by `if (result == LGX_SUCCESS)`.

> **UPDATE:** These tests may be compiled but not actually run if they're referencing types not in the public header (`lgx_allocation_intent_t` at line 172 is not a type in [lgx_types.h](file:///home/karl/Projects/LGX/include/lgx_types.h) — the public type is `lgx_allocation_intent_base_t`). Verify compilation with: `cd build-test && cmake .. && make unit_test_memory_allocation`

---

### Bug 5: [test_gpu_alloc()](file:///home/karl/Projects/LGX/tests/unit/test_memory_allocation.c#149-167) Calls `lgx_gpu_free()` with Wrong Signature

**File:** [tests/unit/test_memory_allocation.c](file:///home/karl/Projects/LGX/tests/unit/test_memory_allocation.c)  
**Line:** 163

```c
lgx_gpu_free(alloc, 0);  // ← "0" is passed as second arg
```

But in [lgx_runtime_internal.h](file:///home/karl/Projects/LGX/include/lgx/lgx_runtime_internal.h) line 292:
```c
void lgx_gpu_free(lgx_gpu_allocation_t* alloc);  // ← takes only ONE argument
```

**Fix:** Remove the second argument: `lgx_gpu_free(alloc);`

---

## Phase 2 — Code Cleanliness

These don't cause runtime bugs but create maintenance hazards and signal poor discipline to contributors.

---

### [MODIFY] [lgx_memory_manager.c](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c) — Remove Duplicate Forward Declarations

**Lines with duplicates:**
- Block 1 (correct, canonical): Lines 200–215
- Block 2 (first duplicate): Lines 238–246 — exact repeat of [track_allocation](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#1786-1816), [untrack_allocation](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#1937-1954), [allocate_small](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#1551-1615), [allocate_medium](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#1616-1634), [allocate_large](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#1635-1654)
- Block 3 (second duplicate): Lines 290–300 — exact repeat of same 5 declarations plus [get_thread_cache](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#936-1007), [get_size_class_index](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#1446-1454)

**Fix:** Delete lines 238–246 and lines 290–300 entirely. Keep only the canonical block at 200–215 which includes all necessary declarations.

**Why this happened:** The file was almost certainly built by copy-pasting sections during iterative development. Each new section of code came with its own declarations pasted from an earlier template.

---

### [MODIFY] [lgx_memory_manager.c](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c) — Resolve [pool_alloc()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#248-290) Dead Code

**Lines:** 248-289

[pool_alloc()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#248-290) is marked `__attribute__((unused))` and is never called anywhere in the codebase. It represents an earlier design (per-thread bump allocator) that was partially superseded by the hot-path cache.

**Decision required — two options:**

**Option A (Remove):** If this design has been superseded, delete the function entirely. The [allocate_ultra_fast()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#1502-1550) + hot path cache handles what [pool_alloc](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#248-290) was meant to do.

**Option B (Integrate):** If this is intentionally kept for future use (it is a legitimate zero-lock bump allocator pattern), remove `__attribute__((unused))` and wire it into the hot path as a prefix before the thread cache. Add a detailed comment explaining *why* it's there.

Recommendation: **Option A** — the hot path already pre-allocates from thread pools internally. This function is a dead copy.

---

### [MODIFY] [lgx_frame_arena.c](file:///home/karl/Projects/LGX/src/runtime/lgx_frame_arena.c) — Remove Duplicate Forward Declarations

**Lines:** 207-210 are duplicates of lines 197-200:

```c
// Line 197-200: First declaration (correct)
static void free_huge_page_arena(void* ptr, size_t size);
static void track_allocation_histogram(lgx_frame_arena_t* arena, size_t size);
static void track_allocation_call_site(...);
static size_t get_histogram_bucket(size_t size);

// Lines 207-210: EXACT duplicates — delete these
static void free_huge_page_arena(void* ptr, size_t size);
static void track_allocation_histogram(lgx_frame_arena_t* arena, size_t size);
static void track_allocation_call_site(...);
static size_t get_histogram_bucket(size_t size);
```

**Fix:** Delete lines 207–210.

---

### [MODIFY] [lgx_runtime.h](file:///home/karl/Projects/LGX/include/lgx_runtime.h) — Remove Duplicate Function Declarations

**Lines:** 32-35 declare `lgx_alloc_with_intent`, [lgx_free](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#637-685), [lgx_memory_stats](file:///home/karl/Projects/LGX/src/runtime/lgx_runtime_core.c#291-305).
**Lines:** 40-43 declare **the exact same functions again**.

```c
// Line 33: first declaration
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent);
void lgx_free(void* ptr);
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats);

// Line 40-43: duplicates — delete these
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent);   // ← duplicate
void* lgx_alloc_with_intent_ex(const void* intent, size_t intent_type_id); // ← keep this one
void lgx_free(void* ptr);                                                    // ← duplicate
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats);                  // ← duplicate
```

**Fix:** Remove duplicated declarations (lines 40, 42, 43). Keep `lgx_alloc_with_intent_ex` (line 41) as it's unique.

---

### [MODIFY] [lgx_runtime_internal.h](file:///home/karl/Projects/LGX/include/lgx/lgx_runtime_internal.h) — Move Resource Limits & Memory Safety Outside `extern "C"`

**Lines:** 480–565 define `lgx_resource_limits_config_t`, `lgx_resource_limits_stats_t`, `lgx_memory_safety_config_t` and their function declarations — but they appear **after the `#endif // LGX_RUNTIME_INTERNAL_H` guard at line 479**.

This means these types are:
1. Not protected by the include guard (will be double-declared if the header is included twice)
2. Not inside the `extern "C"` block (could cause C++ linkage issues)

**Fix:** Move lines 480–565 to be *inside* the `#ifndef LGX_RUNTIME_INTERNAL_H` guard, before the closing `#endif`.

---

## Phase 3 — Compiler Hardening

Minor but meaningful improvements to catch future regressions at compile time.

---

### [MODIFY] [CMakeLists.txt](file:///home/karl/Projects/LGX/CMakeLists.txt)

**Line 14:** Current flags:
```cmake
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -fPIC")
```

**Add these warning flags:**
```cmake
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -fPIC \
    -Wredundant-decls \
    -Wshadow \
    -Wstrict-prototypes \
    -Wmissing-prototypes")
```

- `-Wredundant-decls`: Would have caught the duplicate forward declarations in Phase 2 at compile time
- `-Wshadow`: Catches local variables shadowing outer scope (common in large C files)
- `-Wstrict-prototypes` / `-Wmissing-prototypes`: Enforces that all functions are declared before use

> **Important:** Only add these flags **after Phase 2 cleanup** — these flags will fail the build on the existing duplicate declarations.

---

## Summary of Changes

| File | Change | Phase |
|------|--------|-------|
| [tests/test_results.txt](file:///home/karl/Projects/LGX/tests/test_results.txt) | `git rm --cached` (275MB file) | P0 |
| [.gitignore](file:///home/karl/Projects/LGX/.gitignore) | Add missing binary patterns | P0 |
| `lgx_memory_manager.c:390-391` | Delete overriding `use_huge_pages = false` | P1 |
| `lgx_memory_manager.c:746-755` | Add `atomic_fetch_add(total_deallocations)` | P1 |
| `lgx_memory_manager.c:824` | Move stats increment before `posix_memalign` | P1 |
| `tests/unit/test_memory_allocation.c:241-243` | Fix wrong field names | P1 |
| `tests/unit/test_memory_allocation.c:163` | Fix `lgx_gpu_free` signature | P1 |
| `lgx_memory_manager.c:238-246, 290-300` | Delete duplicate forward declarations | P2 |
| `lgx_memory_manager.c:248-289` | Delete dead [pool_alloc()](file:///home/karl/Projects/LGX/src/runtime/lgx_memory_manager.c#248-290) | P2 |
| `lgx_frame_arena.c:207-210` | Delete duplicate forward declarations | P2 |
| `lgx_runtime.h:40, 42, 43` | Delete duplicate function declarations | P2 |
| `lgx_runtime_internal.h:480-565` | Move inside include guard | P2 |
| `CMakeLists.txt:14` | Add `-Wredundant-decls -Wshadow` etc. | P3 |

---

## Verification Plan

All verification uses the existing test suite. No new tests need to be written for this refactoring — the existing tests are sufficient to confirm correctness is preserved.

### Step 1: Build (after each phase)

```bash
cd /home/karl/Projects/LGX
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON ..
make -j$(nproc) 2>&1 | tail -20
```

A clean build (zero warnings, zero errors) after each phase confirms the changes are non-breaking.

### Step 2: Run the Full Test Suite

```bash
cd /home/karl/Projects/LGX/build
ctest --output-on-failure 2>&1 | tail -40
```

Expected: all 64 tests passing before and after each phase.

### Step 3: Targeted Unit Tests

```bash
cd /home/karl/Projects/LGX/build
make unit_test_memory_allocation && ./unit_test_memory_allocation
make unit_test_health_monitor && ./unit_test_health_monitor
make unit_test_init_shutdown && ./unit_test_init_shutdown
```

These specifically exercise the code paths touched in Phase 1.

### Step 4: Address Sanitizer Validation (P1 bugs specifically)

The huge-pages bug (Bug 1) and pool stats bug (Bug 2) need to be verified not to introduce memory errors:

```bash
cd /home/karl/Projects/LGX/build
ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=../lsan.supp \
  ctest --output-on-failure -R "unit_" 2>&1 | grep -E "(PASS|FAIL|ERROR|leak)"
```

### Step 5: Manual Verification of Bug 1 (huge pages fix)

After fixing the `use_huge_pages` override, verify it's actually running on the correct path:

```bash
cd /home/karl/Projects/LGX/build
# Run a test that prints hardware status
./unit_test_init_shutdown 2>&1 | grep -i "huge"
```

Or add a temporary `fprintf(stderr, "use_huge_pages=%d\n", mgr->use_huge_pages)` after line 391 (now deleted) to confirm the correct value propagates.

### Step 6: After Phase 3 — Confirm New Warnings Catch Future Issues

```bash
cd /home/karl/Projects/LGX/build
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc) 2>&1 | grep -i "warning"
```

Expected: zero warnings with the new flags (since Phase 2 already removed the duplicates they would flag).

---

## What This Plan Does NOT Cover

To keep scope realistic and non-breaking:

- **No API changes**: All public function signatures stay identical.
- **No algorithmic changes**: The memory allocator logic, frame arena logic, and hot path are untouched.
- **No test additions**: The existing 64 tests provide sufficient coverage for what's being changed.
- **No strategic decisions**: The platform vision question (memory-only runtime vs. complete gaming platform) is architectural and out of refactoring scope.

These can be tackled layer by layer *after* this refactoring baseline is clean.
