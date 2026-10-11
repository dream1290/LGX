# Plan 01 — Correctness & Arena Performance Investigation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the three memory-safety defects that make "production-ready" false, and determine — by measurement, not assumption — why the frame arena costs more than a bare bump pointer, so the honest performance claim is known before any public artifact is built on it.

**Architecture:** This plan fixes defects in `src/runtime/`. It does not add features and does not change public API signatures. Tasks 1-3 are correctness (use-after-free, dangling pointers, self-deadlock) and each lands with a regression test that fails before the fix. Tasks 4-6 are measurement: profile the arena's real hot path, quantify instrumentation overhead, and build a workload that is not biased toward glibc. Task 7 corrects routing so a GPU-sharing intent reaches the GPU pool.

**Tech Stack:** C11, CMake 3.16+, ctest, AddressSanitizer/UBSan (Debug config), `perf` for profiling.

## Global Constraints

- Plan 00 must be merged first. The clean build, the C++ header gate, and the benchmark methodology gate are assumed present and green.
- Every defect fix MUST land with a regression test that **fails before the fix and passes after**. The existing suite caught none of these defects; a fix without a reproducing test is not accepted.
- Zero new warnings. `-Wall -Wextra -Werror -Wredundant-decls -Wshadow` must survive. Never weaken a flag to make a test compile.
- No public API signature may change. `include/lgx_runtime.h` and the `.map` symbol version files are frozen except where a task says otherwise explicitly.
- Every performance number recorded in a file MUST come from a run performed in that task, with the command and the machine recorded alongside it. No carried-over numbers, no estimates.
- Profile before optimising. Do not remove an observer (instrumentation, safety check, guard) until its cost is measured and recorded.
- Known pre-existing failures that are NOT this plan's to fix: `test_telemetry_anonymization`, `perf_test_frame_time_contribution` (SIGSEGV), and the intermittent threading/benchmark flakes already allowlisted in `.github/ci/known-test-failures.txt`.

---

## Prioritised findings this plan addresses

| # | Defect | Severity | Task |
|---|--------|----------|------|
| 1 | GPU pool use-after-free: handles store a raw `buddy_block_t*` into an array that `realloc` moves | Critical | 1 |
| 2 | Frame arena mid-frame growth unmaps the old base, invalidating live pointers | Critical | 2 |
| 3 | Chaos testing self-deadlocks on a non-recursive mutex | High | 3 |
| 4 | `lgx_alloc_gpu_shared()` never reaches the GPU pool (lifetime checked before hint) | High | 4 |
| 5 | Unguarded `lgx_gpu_alloc` breaks non-Vulkan link (the red `no-vulkan` CI job) | High | 5 |
| 6 | Net connections time out because `last_recv_ms` is never refreshed | High | 6 |
| 7 | Per-allocation instrumentation cost in the frame arena hot path | Perf | 7 |
| 8 | The benchmark is same-size tight-loop — glibc's best case | Perf | 8 |

---

## File Structure

| File | Action | Responsibility after this plan |
|---|---|---|
| `src/runtime/lgx_gpu_pool.c` | Modify | Handles store a stable block index; no pointer survives `realloc` |
| `src/runtime/lgx_frame_arena.c` | Modify | Growth defers the old mapping until the frame ends |
| `src/runtime/lgx_chaos_testing.c` | Modify | Single lock discipline; no recursive acquisition |
| `src/runtime/lgx_intent_allocator.c` | Modify | Hint evaluated before lifetime; GPU path compiled only with Vulkan |
| `src/net/lgx_net.c` | Modify | Receive path refreshes the activity timestamp |
| `benchmarks/benchmark_allocation_throughput.c` | Modify | Adds a mixed-size, many-live-objects workload alongside the tight loop |
| `tests/phase0/test_gpu_pool_handle_safety.c` | Create | Regression test for defect 1 |
| `tests/phase0/test_frame_arena_growth.c` | Create | Regression test for defect 2 |
| `tests/phase0/test_chaos_locking.c` | Create | Regression test for defect 3 |
| `tests/unit/test_intent_routing.c` | Create | Regression test for defect 4 |
| `docs/05-performance/arena-hotpath-profile.md` | Create | Measured cost of the arena hot path |

**Interfaces produced by this plan:**

- `lgx_gpu_allocation_t` gains a `size_t block_index` field; no public function signature changes.
- `benchmarks/benchmark_benchmark_allocation_throughput` emits `FRAME_RESET_PATTERN` (unchanged contract) plus a new `MIXED_WORKLOAD` line per size.
- `.github/ci/run-tests.sh` gains zero new labels; the `no-vulkan` job goes green when Task 5 lands.

---

## Provenance of Task 7's starting point

Task 7 begins from two measurements taken before it was written.

### Measurement 1 — cost of the three unconditional observers

An out-of-tree copy with the observers removed from the allocation success path only, same harness, same machine, 3 runs each, medians compared:

| size | observers present | observers removed | delta | per-allocation |
|------|--------------------|-------------------|-------|----------------|
| 16B  | 1706 ns | 1544 ns | 162 ns | 2.5 ns |
| 64B  | 1675 ns | 1503 ns | 172 ns | 2.7 ns |
| 256B | 1837 ns | 1574 ns | 263 ns | 4.1 ns |
| 1KB  | 2502 ns | 2408 ns | 94 ns | 1.5 ns |
| 4KB  | 7411 ns | 7230 ns | 181 ns | 2.8 ns |

(64 allocations per frame, so delta/64 = cost per allocation.)

Three observers run on every successful allocation: `check_high_usage_warning`, `track_allocation_histogram`, `track_allocation_call_site`. Combined cost: **1.5-4.1 ns per allocation**.

### Measurement 2 — the arena measured against a bare bump-pointer floor

An external control established what this workload *should* cost, on a different machine. It implemented the same shape (64 allocations per frame, each genuinely touched with `memset`, then a reset) against progressively more realistic allocators. Its floor for a plain bump allocator was **~3 ns per allocation**, with a 3-buffer rotation adding nothing measurable at this working-set size, and adding one acquire load costing ~7 ns on that host.

That control is not this project's code and its absolute numbers are not transferable. What it does establish is the shape of the answer: the workload permits a few nanoseconds, so the arena should be measured against that floor, not against malloc.

So the permanent benchmark now carries two reference rows alongside `malloc_*` and `lgx_frame_*`:

- **`bump_floor_*`** — a static bump pointer over one region: offset advance and `memset`, no atomics, no bookkeeping, no reset, no safety checks.
- **`atomic_floor_*`** — the same, plus the acquire load of the frame index that the real arena performs once per allocation.

Measured on this machine (ns per frame of 64 allocations):

| size | bump floor | atomic floor | lgx_frame | vs bump | overhead per allocation |
|------|-----------|--------------|-----------|---------|--------------------------|
| 16B  | 305 ns | 341 ns | 1945 ns | 6.4× | 25.6 ns |
| 64B  | 152 ns | 201 ns | 1632 ns | 10.7× | 23.1 ns |
| 256B | 315 ns | 325 ns | 1800 ns | 5.7× | 23.2 ns |
| 1KB  | 1515 ns | 1518 ns | 2473 ns | 1.6× | 15.0 ns |
| 4KB  | 6632 ns | 6731 ns | 7436 ns | 1.1× | 12.6 ns |

**What this already rules out.** The arena does no per-byte work on either the allocation or the reset path: every `memset`/`memcpy` in `lgx_frame_arena.c` is in an init, stats, or dump routine, and the `0xDEADBEEF` magic check is inside `#if LGX_DEBUG_BUILD`, which is 0 under `-DNDEBUG`. Poisoning on reset is therefore **not** the cause, which was the leading hypothesis before this was checked.

**What this also shows.** The acquire load is free here (341 vs 305 ns at 16B) because x86-TSO does not need a fence for an acquire load; an isolated microbenchmark confirmed a zero delta between acquire and relaxed on this host. It would cost something on ARM or RISC-V, which is why `atomic_floor` is measured rather than assumed.

**So the budget that remains.** ~23-25 ns per allocation unexplained at small sizes, against a floor of ~3-5 ns. The observers account for 1.5-4.1 ns of it.

### Acceptance bar for Task 7

Set from the control, not chosen to be convenient:

- **within 2× of `bump_floor` at ≤256B**, and
- **faster than `malloc` at every size.**

Meeting the first at 16B/64B/256B requires getting from ~24 ns to under ~8 ns per allocation. If the arena cannot meet that, its design is not delivering on its own premise and Task 7 must say so plainly rather than explain the gap away.

**Time-box: one task.** If the remaining ~20 ns is not identified by profiling in this task, record it as an open defect with the profile data attached, and hand it to a dedicated follow-up. Do not spend more than this task on it, and do not tune anything toward the bar without first understanding what the cost is.

---

### Task 1: GPU pool handle use-after-free

**Files:**
- Modify: `src/runtime/lgx_gpu_pool.c` (handle creation ~line 935, `lgx_gpu_free` ~lines 975-978, `lgx_gpu_alloc` signature region)
- Create: `tests/phase0/test_gpu_pool_handle_safety.c`
- Modify: `tests/phase0/CMakeLists.txt` (if an explicit target is needed; otherwise the root glob picks it up)

**Interfaces:**
- Consumes: nothing.
- Produces: `lgx_gpu_allocation_t` carries `size_t block_index`; Task 5 and later tasks rely on handles remaining valid across allocator growth.

- [ ] **Step 1: Write the failing regression test**

The defect: `lgx_gpu_alloc` stores `alloc->block = block` — a raw pointer into `allocator->all_blocks[]`. That array is `realloc`'d when `num_blocks >= max_blocks` (initial 1024, `lgx_gpu_pool.c:165`), so every outstanding handle dangles. `lgx_gpu_free` then dereferences it (`used_budget[type] -= alloc->block->size`) and passes it to `buddy_free`, which recomputes an index from freed memory.

Create `tests/phase0/test_gpu_pool_handle_safety.c`:

```c
/*
 * Regression test: GPU pool handles must survive buddy-array growth.
 *
 * Defect (Plan 01, Task 1): lgx_gpu_alloc stored a raw buddy_block_t* in the
 * public handle. buddy_alloc() reallocs all_blocks when it runs out of slots, so
 * every handle issued before that realloc pointed into freed memory. The arena
 * reads well under ASan only if the test forces the realloc; small workloads
 * never reach the 1024-slot threshold, which is why the existing suite missed it.
 *
 * This test MUST be able to fail: it returns non-zero on any inconsistency.
 */
#include <lgx_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int failures = 0;

    /* Initialise the runtime. */
    lgx_runtime_config_t* cfg = lgx_config_create();
    if (!cfg) {
        fprintf(stderr, "FAIL: lgx_config_create returned NULL\n");
        return 1;
    }
    if (lgx_runtime_init(cfg) != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_runtime_init failed\n");
        lgx_config_destroy(cfg);
        return 1;
    }

    /* Hold a set of handles from BEFORE the pool is grown. */
    enum { EARLY = 8 };
    lgx_gpu_allocation_t* early[EARLY];
    for (int i = 0; i < EARLY; i++) {
        early[i] = lgx_gpu_alloc(256, 16, LGX_GPU_HOST_VISIBLE);
        if (!early[i]) {
            /* No Vulkan device available: this test cannot run here. */
            fprintf(stderr, "SKIP: lgx_gpu_alloc returned NULL (no GPU device)\n");
            lgx_runtime_shutdown();
            lgx_config_destroy(cfg);
            return 0;
        }
        /* Write through it so ASan sees a real use. */
        void* p = lgx_gpu_get_mapped_ptr(early[i]);
        if (p) memset(p, 0xAB, 256);
    }

    /* Force the buddy array well past its initial 1024 slots so all_blocks is
     * realloc'd at least once, invalidating every raw pointer held above. */
    enum { GROW = 4000 };
    lgx_gpu_allocation_t* many[GROW];
    int grown = 0;
    for (int i = 0; i < GROW; i++) {
        many[i] = lgx_gpu_alloc(256, 16, LGX_GPU_HOST_VISIBLE);
        if (many[i]) grown++;
    }
    printf("allocated %d/%d growth allocations\n", grown, GROW);

    /* The early handles must still describe the same, live blocks. */
    for (int i = 0; i < EARLY; i++) {
        if (!early[i]) continue;
        VkDeviceSize size = lgx_gpu_get_size(early[i]);
        if (size != 256) {
            fprintf(stderr, "FAIL: early[%d] size changed: got %llu, want 256\n",
                    i, (unsigned long long)size);
            failures++;
        }
        void* p = lgx_gpu_get_mapped_ptr(early[i]);
        if (p) {
            unsigned char probe[256];
            memcpy(probe, p, 256);
            for (int b = 0; b < 256; b++) {
                if (probe[b] != 0xAB) {
                    fprintf(stderr, "FAIL: early[%d] contents corrupted at byte %d\n", i, b);
                    failures++;
                    break;
                }
            }
        }
    }

    /* Free them. Under ASan this is where the use-after-free is detected. */
    for (int i = 0; i < EARLY; i++) {
        if (early[i]) lgx_gpu_free(early[i]);
    }
    for (int i = 0; i < grown; i++) {
        if (many[i]) lgx_gpu_free(many[i]);
    }

    lgx_runtime_shutdown();
    lgx_config_destroy(cfg);

    if (failures) {
        fprintf(stderr, "FAIL: %d handle inconsistency(ies)\n", failures);
        return 1;
    }
    printf("PASS: GPU handles survived buddy growth\n");
    return 0;
}
```

Verify the exact public names before relying on them:

```bash
grep -nE "lgx_gpu_alloc|lgx_gpu_free|lgx_gpu_get_size|lgx_gpu_get_mapped_ptr" include/*.h
grep -rnE "^(lgx_gpu_allocation_t\*|void|VkDeviceSize|void\*) lgx_gpu_" src/runtime/lgx_gpu_pool.c | head -20
```

If `lgx_gpu_get_mapped_ptr` or the `LGX_GPU_HOST_VISIBLE` enum member has a different name, adapt the test to the real API. **Do not add new API to make the test compile.** If the function that returns an allocation's size has a different name, use it.

- [ ] **Step 2: Verify the test can fail (this is the critical step)**

Run it under ASan against the **unfixed** runtime. The build must be the Debug configuration, which enables `-fsanitize=address`:

```bash
cd /home/karl/Projects/LGX/.worktrees/ground-truth
cmake -S . -B /tmp/lgx-t1-asan -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/lgx-t1-asan -j$(nproc) -t test_gpu_pool_handle_safety
/tmp/lgx-t1-asan/tests/phase0/test_gpu_pool_handle_safety; echo "exit=$?"
```

Expected: ASan reports a **heap-use-after-free** in `lgx_gpu_free` (reading `alloc->block`), and/or the contents check fails.

If the test passes on the unfixed code, the test is wrong — the growth did not trigger a `realloc`. Increase `GROW` until it does, and confirm `allocated N/4000` shows a real allocation count. A regression test that cannot fail is worse than none; report this rather than proceeding.

Also confirm the non-ASan Release build does not crash but does fail or corrupt:

```bash
cmake -S . -B /tmp/lgx-t1-rel -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/lgx-t1-rel -j$(nproc) -t test_gpu_pool_handle_safety
/tmp/lgx-t1-rel/tests/phase0/test_gpu_pool_handle_safety; echo "exit=$? (0 = passed, may vary by allocator layout)"
```

- [ ] **Step 3: Fix the handle to store an index, not a pointer**

In `lgx_gpu_pool.c`, find the `lgx_gpu_allocation_t` definition (internal, not in a public header) and add an index field next to `block`:

```c
typedef struct {
    lgx_gpu_memory_type_t memory_type;
    buddy_block_t* block;        /* internal-use only; never dereferenced across a realloc */
    size_t            block_index; /* STABLE: index into all_blocks, survives realloc */
    VkDevice         memory;
    VkDeviceSize     offset;
    VkDeviceSize     size;
    void*            mapped_ptr;
} lgx_gpu_allocation_t;
```

In `lgx_gpu_alloc`, after `buddy_alloc` succeeds, capture the index before any other call can grow the array:

```c
    /* Record the STABLE index, not the pointer. buddy_alloc may realloc
     * all_blocks, which invalidates every raw block pointer. */
    size_t block_index = (size_t)(block - allocator->all_blocks);

    alloc->block = block;          /* convenience only, valid within this lock */
    alloc->block_index = block_index;
```

In `lgx_gpu_free`, recover the pointer from the index **under the same lock**, exactly as the buddy allocator's own internals already do (see the `// CRITICAL: Save block index before any operations (realloc may move memory)` comment near `lgx_gpu_pool.c:229`):

```c
    pthread_mutex_lock(&g_gpu_pool.mutex);

    buddy_allocator_t* allocator = &g_gpu_pool.allocators[type];

    /* Re-derive the pointer from the stable index. The stored pointer may be
     * stale: buddy_alloc reallocs all_blocks when it runs out of slots. */
    if (alloc->block_index >= (size_t)allocator->num_blocks) {
        pthread_mutex_unlock(&g_gpu_pool.mutex);
        fprintf(stderr, "[LGX GPU] corrupt handle: block_index %zu out of range (%d)\n",
                alloc->block_index, allocator->num_blocks);
        free(alloc);
        return;
    }
    buddy_block_t* block = &allocator->all_blocks[alloc->block_index];

    /* Update statistics */
    g_gpu_pool.used_budget[type] -= block->size;
    g_gpu_pool.deallocation_count++;

    buddy_free(allocator, block);

    buddy_update_fragmentation(allocator);

    pthread_mutex_unlock(&g_gpu_pool.mutex);

    free(alloc);
```

Audit every other reader of `alloc->block` in the file and convert each to re-derive from `alloc->block_index` under the lock:

```bash
grep -n "\->block\b" src/runtime/lgx_gpu_pool.c
```

Each site must be inside a region where `allocator->all_blocks` cannot move without the lock, or must re-derive the pointer from the index first.

- [ ] **Step 4: Verify the test now passes under ASan**

```bash
cmake --build /tmp/lgx-t1-asan -j$(nproc) -t test_gpu_pool_handle_safety
/tmp/lgx-t1-asan/tests/phase0/test_gpu_pool_handle_safety; echo "exit=$? (expect 0)"
```

Expected: `PASS: GPU handles survived buddy growth`, no ASan report.

Also confirm the Release build is still clean:

```bash
cmake --build /tmp/lgx-t1-rel -j$(nproc) 2>&1 | grep -c "warning:" || echo "0 warnings"
```

- [ ] **Step 5: Prove the fix is load-bearing**

Temporarily revert the `lgx_gpu_free` change (restore the `alloc->block` reads) while keeping the index field, rebuild under ASan, and confirm the test fails again. Then restore the fix.

```bash
# after reverting:
cmake --build /tmp/lgx-t1-asan -j$(nproc) -t test_gpu_pool_handle_safety
/tmp/lgx-t1-asan/tests/phase0/test_gpu_pool_handle_safety; echo "exit=$? (expect non-zero)"
```

Record both exit codes in the report. A fix whose removal does not break the test was not actually fixing anything.

- [ ] **Step 6: Register the test and run the suite**

```bash
ctest --test-dir /tmp/lgx-t1-rel -R test_gpu_pool_handle_safety --output-on-failure
ctest --test-dir /tmp/lgx-t1-rel 2>&1 | tail -6
```

Confirm no new failures beyond the known allowlist.

- [ ] **Step 7: Commit**

```bash
git add src/runtime/lgx_gpu_pool.c tests/phase0/test_gpu_pool_handle_safety.c
git commit -m "fix(gpu): make pool handles survive buddy-array realloc

lgx_gpu_alloc stored a raw buddy_block_t* in the public handle. buddy_alloc
reallocs all_blocks when it exceeds its slot count (initial 1024), so every
handle issued before that realloc pointed into freed memory; lgx_gpu_free then
read alloc->block->size and passed the stale pointer to buddy_free, which
recomputed an index from freed memory.

The buddy allocator's internals were already index-safe and carefully commented
about this; the public handle boundary was not.

- add block_index to the handle, captured before any growth can move the array
- re-derive the block pointer from the index under the lock in lgx_gpu_free
- bounds-check the index instead of trusting it
- add test_gpu_pool_handle_safety, which holds handles across >1024 allocations
  and fails under ASan without the fix (the existing suite never crossed the
  threshold, which is why this went unnoticed)

No public signature changes."
```

---

### Task 2: Frame arena mid-frame growth invalidates live pointers

**Files:**
- Modify: `src/runtime/lgx_frame_arena.c` (`grow_arena` ~lines 359-391, arena struct definition, `lgx_frame_reset`)
- Create: `tests/phase0/test_frame_arena_growth.c`

**Interfaces:**
- Consumes: nothing.
- Produces: the arena keeps a retired-mapping list that `lgx_frame_reset` releases. No public signature changes.

- [ ] **Step 1: Write the failing regression test**

The defect: `grow_arena` allocates a new base, `memcpy`s the live bytes, then calls `free_huge_page_arena(arena->base, ...)` — unmapping the old region. Every pointer already returned by `lgx_alloc_frame` during that frame now points into an unmapped page. The function returns `true` and the caller proceeds as if nothing happened.

Create `tests/phase0/test_frame_arena_growth.c`:

```c
/*
 * Regression test: growing the frame arena must not invalidate live pointers.
 *
 * Defect (Plan 01, Task 2): grow_arena() memcpy'd the live bytes into a larger
 * mapping and then unmapped the old one immediately. Any pointer already handed
 * out during the current frame became dangling, with no error reported.
 *
 * The test fills the current arena, forces growth, and then reads and writes
 * through pointers obtained BEFORE the growth.
 */
#include <lgx_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int failures = 0;

    lgx_runtime_config_t* cfg = lgx_config_create();
    if (!cfg) {
        fprintf(stderr, "FAIL: lgx_config_create returned NULL\n");
        return 1;
    }
    if (lgx_runtime_init(cfg) != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_runtime_init failed\n");
        lgx_config_destroy(cfg);
        return 1;
    }

    lgx_frame_arena_stats_t before;
    if (lgx_frame_arena_get_stats(&before) != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_frame_arena_get_stats failed\n");
        lgx_runtime_shutdown();
        lgx_config_destroy(cfg);
        return 1;
    }
    printf("arena capacity before: %zu bytes\n", before.capacity_bytes);

    /* Pointers taken early in this frame. */
    enum { EARLY = 16 };
    void* early[EARLY];
    for (int i = 0; i < EARLY; i++) {
        early[i] = lgx_alloc_frame(1024);
        if (!early[i]) {
            fprintf(stderr, "FAIL: lgx_alloc_frame returned NULL at %d\n", i);
            failures++;
            break;
        }
        memset(early[i], 0x5A, 1024);
    }
    if (failures) {
        lgx_runtime_shutdown();
        lgx_config_destroy(cfg);
        return 1;
    }

    /* Allocate past capacity to force grow_arena. On success every allocation
     * either grows the arena or falls back to the persistent heap; either way
     * the early pointers must stay valid. */
    size_t total = 0;
    size_t chunk = 64 * 1024;
    for (int i = 0; i < 4096 && total < (size_t)512 * 1024 * 1024; i++) {
        void* p = lgx_alloc_frame(chunk);
        if (!p) break;
        memset(p, 0x11, chunk);   /* touch it so an unmapped page faults here */
        total += chunk;
    }
    printf("allocated %zu bytes this frame (forces growth)\n", total);

    /* The early pointers must still be readable, writable, and intact.
     * Under ASan, a freed/unmapped region is caught here. */
    for (int i = 0; i < EARLY; i++) {
        unsigned char probe[1024];
        memcpy(probe, early[i], sizeof(probe));
        for (int b = 0; b < 1024; b++) {
            if (probe[b] != 0x5A) {
                fprintf(stderr,
                        "FAIL: early[%d] corrupted at byte %d (0x%02X, want 0x5A)"
                        " -- pointer invalidated by arena growth\n",
                        i, b, probe[b]);
                failures++;
                break;
            }
        }
        /* Write through it too: read-only mappings must not exist here. */
        memset(early[i], 0x3C, 1024);
    }

    lgx_frame_arena_stats_t after;
    if (lgx_frame_arena_get_stats(&after) == LGX_SUCCESS) {
        printf("arena capacity after: %zu bytes (grew: %s)\n",
               after.capacity_bytes,
               after.capacity_bytes > before.capacity_bytes ? "yes" : "no");
    }

    lgx_runtime_shutdown();
    lgx_config_destroy(cfg);

    if (failures) {
        fprintf(stderr, "FAIL: %d live pointer(s) invalidated by arena growth\n", failures);
        return 1;
    }
    printf("PASS: live pointers survived arena growth\n");
    return 0;
}
```

Verify the real names before relying on them:

```bash
grep -nE "lgx_frame_arena_get_stats|lgx_frame_arena_stats_t|capacity_bytes" include/*.h | head
```

Adapt to the actual stats API. Do not invent API.

- [ ] **Step 2: Verify the test fails on the unfixed code under ASan**

```bash
cmake -S . -B /tmp/lgx-t2-asan -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/lgx-t2-asan -j$(nproc) -t test_frame_arena_growth
/tmp/lgx-t2-asan/tests/phase0/test_frame_arena_growth; echo "exit=$? (expect non-zero)"
```

Expected: ASan reports a use-after-free / SEGV on the mapping returned by `allocate_huge_page_arena` when the early pointers are touched after growth.

If the arena does not actually grow (capacity unchanged and the loop hits the fallback path), the test does not exercise the defect. In that case, raise `chunk`/iteration count, or reduce the configured arena size, so `grow_arena` is genuinely reached. Verify by printing `arena capacity after` — it must be larger than before. **Report this if you cannot make the test fail; do not proceed with a test that cannot detect the bug.**

- [ ] **Step 3: Implement deferred reclamation of the old mapping**

Add a small retired-mapping list to the arena state:

```c
/* Mappings retired by grow_arena, released at the next frame boundary.
 * Keeping them alive for the remainder of the frame means every pointer already
 * returned by lgx_alloc_frame stays valid for the whole frame, which is the
 * contract callers rely on. */
#define LGX_MAX_RETIRED_MAPPINGS 8
typedef struct {
    uint8_t* base;
    size_t   size;
} lgx_retired_mapping_t;

static lgx_retired_mapping_t g_retired_mappings[LGX_MAX_RETIRED_MAPPINGS];
static uint32_t              g_retired_mapping_count = 0;
```

In `grow_arena`, replace the immediate free with a deferred release, and make the function's contract explicit:

```c
    /* Do NOT free the old base here. Every pointer already returned by
     * lgx_alloc_frame during this frame points into it, and callers are
     * entitled to use them until the frame ends. Retire the mapping and
     * release it at the next lgx_frame_reset. */
    if (g_retired_mapping_count < LGX_MAX_RETIRED_MAPPINGS) {
        g_retired_mappings[g_retired_mapping_count].base = arena->base;
        g_retired_mappings[g_retired_mapping_count].size  = arena->capacity;
        g_retired_mapping_count++;
    } else {
        /* More growth in one frame than the retire list holds: the oldest
         * mappings are long dead and their pointers already violated the frame
         * contract. Release the oldest to bound memory. */
        free_huge_page_arena(g_retired_mappings[0].base, g_retired_mappings[0].size);
        memmove(&g_retired_mappings[0], &g_retired_mappings[1],
                (LGX_MAX_RETIRED_MAPPINGS - 1) * sizeof(lgx_retired_mapping_t));
        g_retired_mappings[LGX_MAX_RETIRED_MAPPINGS - 1].base = arena->base;
        g_retired_mappings[LGX_MAX_RETIRED_MAPPINGS - 1].size  = arena->capacity;
    }
```

At the top of `lgx_frame_reset`, after the reset itself succeeds, release everything retired this frame:

```c
    /* Release mappings retired by arena growth during the frame that just ended.
     * Every pointer from that frame is now expired by contract. */
    for (uint32_t i = 0; i < g_retired_mapping_count; i++) {
        free_huge_page_arena(g_retired_mappings[i].base, g_retired_mappings[i].size);
    }
    g_retired_mapping_count = 0;
```

Also release any stragglers in `lgx_frame_arena_shutdown` so the process does not leak the retired mappings:

```c
    for (uint32_t i = 0; i < g_retired_mapping_count; i++) {
        free_huge_page_arena(g_retired_mappings[i].base, g_retired_mappings[i].size);
    }
    g_retired_mapping_count = 0;
```

- [ ] **Step 4: Verify the test passes**

```bash
cmake --build /tmp/lgx-t2-asan -j$(nproc) -t test_frame_arena_growth
/tmp/lgx-t2-asan/tests/phase0/test_frame_arena_growth; echo "exit=$? (expect 0)"
```

Expected: `PASS: live pointers survived arena growth`, no ASan report, and `arena capacity after` larger than before.

- [ ] **Step 5: Prove the fix is load-bearing**

Temporarily restore the immediate `free_huge_page_arena(arena->base, ...)` in `grow_arena`, rebuild, confirm the test fails again, then restore. Record both exit codes.

- [ ] **Step 6: Check for leaks and run the suite**

```bash
cmake --build /tmp/lgx-t2-asan -j$(nproc) 2>&1 | grep -c "warning:" || echo "0 warnings"
ctest --test-dir /tmp/lgx-t2-asan -R test_frame_arena --output-on-failure
ctest --test-dir /tmp/lgx-t2-asan 2>&1 | tail -6
```

Check specifically that the retired mappings do not leak: run under ASan and confirm no `LeakSanitizer` report naming `grow_arena`.

- [ ] **Step 7: Commit**

```bash
git add src/runtime/lgx_frame_arena.c tests/phase0/test_frame_arena_growth.c
git commit -m "fix(arena): keep retired mappings alive until the frame ends

grow_arena() memcpy'd the live bytes into a larger mapping and then unmapped
the old base immediately. Every pointer already returned by lgx_alloc_frame
during that frame became dangling, silently, and grow_arena still reported
success.

Callers are entitled to use a frame allocation until the frame ends, so the old
mapping must outlive the growth.

- retire the old mapping instead of freeing it
- release retired mappings at the next lgx_frame_reset and at shutdown
- bound the retire list; on overflow release the oldest
- add test_frame_arena_growth, which holds pointers across a forced growth and
  fails under ASan without the fix

No public signature changes."
```

---

### Task 3: Chaos testing self-deadlock

**Files:**
- Modify: `src/runtime/lgx_chaos_testing.c` (RNG ~lines 63-67, callers ~188, ~219, ~266, ~294, mutex init ~line 44)
- Create: `tests/phase0/test_chaos_locking.c`

**Interfaces:**
- Consumes: nothing.
- Produces: chaos testing becomes enable-able for the first time. No public signature changes.

- [ ] **Step 1: Write the failing regression test**

The defect: `chaos_random()` locks `g_chaos_state.mutex`. Its callers (`lgx_chaos_should_fail_allocation` and three siblings) **already hold** that same mutex when they call it. It is initialised with `pthread_mutex_init(&g_chaos_state.mutex, NULL)` — default attributes, i.e. **non-recursive** — so the first check deadlocks. Chaos testing has never been enable-able; it is dormant only because it defaults to off.

Create `tests/phase0/test_chaos_locking.c`:

```c
/*
 * Regression test: enabling chaos testing must not deadlock.
 *
 * Defect (Plan 01, Task 3): chaos_random() acquires g_chaos_state.mutex, but
 * every caller already holds it. The mutex is non-recursive
 * (pthread_mutex_init(..., NULL)), so the first chaos decision hangs the process.
 *
 * This test enables chaos testing and forces a decision. On the unfixed code it
 * never returns (the harness must run it under a timeout).
 */
#include <lgx_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

int main(void) {
    lgx_runtime_config_t* cfg = lgx_config_create();
    if (!cfg) {
        fprintf(stderr, "FAIL: lgx_config_create returned NULL\n");
        return 1;
    }
    if (lgx_runtime_init(cfg) != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_runtime_init failed\n");
        lgx_config_destroy(cfg);
        return 1;
    }

    lgx_chaos_config_t chaos;
    memset(&chaos, 0, sizeof(chaos));
    chaos.inject_memory_pressure = true;
    chaos.failure_rate            = 0.5;   /* deterministic enough to exercise both arms */
    chaos.inject_random_delays    = false;

    lgx_result_t r = lgx_runtime_enable_chaos_testing(&chaos);
    if (r != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_runtime_enable_chaos_testing returned %d\n", (int)r);
        lgx_runtime_shutdown();
        lgx_config_destroy(cfg);
        return 1;
    }
    fprintf(stderr, "chaos testing enabled; exercising decision path\n");

    /* This is the call that deadlocks on the unfixed code. */
    for (int i = 0; i < 1000; i++) {
        (void)lgx_chaos_should_fail_allocation();
    }
    fprintf(stderr, "decision path returned (no deadlock)\n");

    /* And the other three observers must take the same discipline. */
    for (int i = 0; i < 1000; i++) {
        (void)lgx_chaos_should_delay();
        (void)lgx_chaos_inject_latency();
        (void)lgx_chaos_should_fail_now();
    }

    lgx_chaos_config_t off;
    memset(&off, 0, sizeof(off));
    lgx_runtime_disable_chaos_testing(&off);

    lgx_runtime_shutdown();
    lgx_config_destroy(cfg);

    printf("PASS: chaos decision path completes without deadlock\n");
    return 0;
}
```

**Verify the real API before using it.** The exact names of the chaos observer functions and the enable/disable signatures must be checked:

```bash
grep -nE "chaos" include/lgx_runtime.h | head -30
grep -nE "^(lgx_result_t|bool|void) lgx_(chaos|runtime_(enable|disable)_chaos)" src/runtime/lgx_chaos_testing.c
```

Use the real names. If a function in the sketch does not exist, use the ones that do. **Do not add new public API to make the test compile.**

- [ ] **Step 2: Verify the test hangs on the unfixed code**

```bash
cmake -S . -B /tmp/lgx-t3 -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/lgx-t3 -j$(nproc) -t test_chaos_locking
timeout 15 /tmp/lgx-t3/tests/phase0/test_chaos_locking; echo "exit=$? (124 = TIMED OUT = deadlock reproduced)"
```

Expected: exit **124** (timeout). That is the defect reproduced. If it returns 0, the deadlock is not being reached — investigate before proceeding.

- [ ] **Step 3: Fix the lock discipline**

The minimal, clearest fix: compute the random value from the state **already protected by the caller's lock**, without re-acquiring it. Change `chaos_random` so it does not lock, and rename it to make the contract explicit:

```c
/*
 * Draw a pseudo-random value in [0,1) from the chaos RNG state.
 *
 * The caller MUST already hold g_chaos_state.mutex. This function deliberately
 * does not lock: every observer in this file is called with the mutex held, and
 * the mutex is non-recursive, so acquiring it again would self-deadlock.
 * rand_r() advances the per-thread state that g_chaos_state.random_state points
 * at, so the result stays consistent under the caller's lock.
 */
static double chaos_random_locked(void) {
    int r = rand_r(&g_chaos_state.random_state);
    return (double)r / (double)RAND_MAX;
}
```

Replace all five call sites (`chaos_random()` → `chaos_random_locked()`):

```bash
grep -n "chaos_random()" src/runtime/lgx_chaos_testing.c
```

Audit every remaining `pthread_mutex_lock(&g_chaos_state.mutex)` in the file and confirm each is on a path that does not already hold it. The observer functions (`lgx_chaos_inject_latency`, and any other that does not call the RNG) must not lock either if they are only ever reached from already-locked contexts.

- [ ] **Step 4: Verify the test now completes**

```bash
cmake --build /tmp/lgx-t3 -j$(nproc) -t test_chaos_locking
timeout 15 /tmp/lgx-t3/tests/phase0/test_chaos_locking; echo "exit=$? (expect 0)"
timeout 15 /tmp/lgx-t3/tests/phase0/test_chaos_locking 2>&1 | tail -2
```

Expected: exit 0, `PASS: chaos decision path completes without deadlock`.

- [ ] **Step 5: Prove the fix is load-bearing**

Temporarily restore `pthread_mutex_lock/unlock` inside the RNG helper, rebuild, confirm the test times out again (exit 124), then restore the fix. Record both results.

- [ ] **Step 6: Run the chaos suite and the wider suite**

Now that chaos testing can actually be enabled, run the tests that exercise it and confirm nothing hangs:

```bash
ctest --test-dir /tmp/lgx-t3 -R chaos --output-on-failure --timeout 120
ctest --test-dir /tmp/lgx-t3 2>&1 | tail -6
cmake --build /tmp/lgx-t3 -j$(nproc) 2>&1 | grep -c "warning:" || echo "0 warnings"
```

- [ ] **Step 7: Commit**

```bash
git add src/runtime/lgx_chaos_testing.c tests/phase0/test_chaos_locking.c
git commit -m "fix(chaos): remove the self-deadlock in the chaos decision path

chaos_random() acquired g_chaos_state.mutex, but all four observers already
held that same mutex when calling it. The mutex is initialised with
pthread_mutex_init(..., NULL) -- default attributes, so non-recursive -- making
the first chaos decision a guaranteed hang.

Chaos testing has therefore never been enable-able; it was dormant only because
it defaults to off, and it is advertised as a production-hardening feature.

- make the RNG read the caller-protected state instead of re-locking
  (chaos_random -> chaos_random_locked, documenting the lock contract)
- audit every remaining acquisition in the file for the same mistake
- add test_chaos_locking, which enables chaos testing and forces the decision;
  it times out (exit 124) on the unfixed code and passes after

No public signature changes."
```

---

### Task 4: `lgx_alloc_gpu_shared()` never reaches the GPU pool

**Files:**
- Modify: `src/runtime/lgx_intent_allocator.c` (routing chain ~lines 268-285)
- Create: `tests/unit/test_intent_routing.c`
- Modify: `tests/unit/CMakeLists.txt` (add to `UNIT_TEST_SOURCES`)

**Interfaces:**
- Consumes: nothing.
- Produces: the GPU-hint branch is reachable from the public `lgx_alloc_gpu_shared()` inline.

- [ ] **Step 1: Write the failing regression test**

The defect: `lgx_alloc_gpu_shared()` (`include/lgx_runtime.h`) sets BOTH `lifetime = LGX_LIFETIME_FRAME` and `hint = LGX_HINT_GPU_SHARED`. The router tests `lifetime == LGX_LIFETIME_FRAME` **first** (`lgx_intent_allocator.c:268`), so the `else if (hint == LGX_HINT_GPU_SHARED)` branch at `:274` is unreachable from that public entry point. A caller asking for GPU-visible shared memory receives ordinary CPU frame-arena memory.

Create `tests/unit/test_intent_routing.c`:

```c
/*
 * Regression test: an explicit GPU-sharing hint must route to the GPU pool.
 *
 * Defect (Plan 01, Task 4): lgx_alloc_gpu_shared() sets lifetime=FRAME *and*
 * hint=GPU_SHARED. The router evaluated lifetime first, so the GPU branch was
 * unreachable from the public API and GPU requests silently got frame-arena
 * memory that is recycled after three frames and was never host-visible.
 *
 * The test asserts routing by observing the allocation's lifetime class through
 * the runtime's own stats, not by reading private state.
 */
#include <lgx_runtime.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    int failures = 0;

    lgx_runtime_config_t* cfg = lgx_config_create();
    if (!cfg) {
        fprintf(stderr, "FAIL: lgx_config_create returned NULL\n");
        return 1;
    }
    if (lgx_runtime_init(cfg) != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_runtime_init failed\n");
        lgx_config_destroy(cfg);
        return 1;
    }

    lgx_intent_stats_t s0, s1;
    memset(&s0, 0, sizeof(s0));
    memset(&s1, 0, sizeof(s1));
    if (lgx_intent_get_stats(&s0) != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_intent_get_stats failed\n");
        lgx_runtime_shutdown();
        lgx_config_destroy(cfg);
        return 1;
    }

    /* Request GPU-shared memory through the public inline. */
    enum { N = 8 };
    int allocated = 0;
    for (int i = 0; i < N; i++) {
        void* p = lgx_alloc_gpu_shared(1024);
        if (p) allocated++;
    }
    printf("lgx_alloc_gpu_shared: %d/%d returned non-NULL\n", allocated, N);

    if (lgx_intent_get_stats(&s1) != LGX_SUCCESS) {
        fprintf(stderr, "FAIL: lgx_intent_get_stats (2nd) failed\n");
        lgx_runtime_shutdown();
        lgx_config_destroy(cfg);
        return 1;
    }

    long long gpu_delta = (long long)s1.gpu_allocations - (long long)s0.gpu_allocations;
    long long frame_delta = (long long)s1.frame_allocations - (long long)s0.frame_allocations;
    printf("gpu_allocations delta=%lld, frame_allocations delta=%lld\n", gpu_delta, frame_delta);

    if (allocated == 0) {
        /* No GPU device on this machine: routing cannot be observed. Say so and
         * do not claim a pass that was not demonstrated. */
        fprintf(stderr, "SKIP: no GPU allocations succeeded; routing not observable here\n");
        lgx_runtime_shutdown();
        lgx_config_destroy(cfg);
        return 0;
    }

    if (gpu_delta < allocated) {
        fprintf(stderr,
                "FAIL: %d GPU-shared requests produced %lld GPU-pool allocations\n",
                allocated, gpu_delta);
        failures++;
    }
    if (frame_delta > 0) {
        fprintf(stderr,
                "FAIL: %lld GPU-shared requests were routed to the frame arena\n",
                frame_delta);
        failures++;
    }

    lgx_runtime_shutdown();
    lgx_config_destroy(cfg);

    if (failures) {
        fprintf(stderr, "FAIL: %d routing failure(s)\n", failures);
        return 1;
    }
    printf("PASS: GPU-sharing hint routes to the GPU pool\n");
    return 0;
}
```

Verify the stats struct's real field names before relying on them:

```bash
grep -n -A20 "lgx_intent_stats_t\|} lgx_intent_stats" include/lgx_types.h include/lgx_runtime.h | head -30
grep -rn "gpu_allocations\|frame_allocations" src/runtime/lgx_intent_allocator.c | head
```

Use the real field names. Do not add API.

- [ ] **Step 2: Verify the test fails on the unfixed code**

```bash
cmake -S . -B /tmp/lgx-t4 -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/lgx-t4 -j$(nproc) -t unit_test_intent_routing
/tmp/lgx-t4/tests/unit/unit_test_intent_routing; echo "exit=$? (expect 1)"
```

Expected: `gpu_allocations delta=0`, `frame_allocations delta=8`, exit 1.

If it exits 0, either there is no GPU device (the SKIP path) or the routing already works on this machine — investigate and report rather than proceeding blindly.

- [ ] **Step 3: Fix the routing precedence**

In `lgx_intent_allocator.c`, evaluate an explicit hardware hint **before** the lifetime class, and document why:

```c
    // Route based on intent.
    //
    // Precedence: an explicit placement hint beats the lifetime class. A
    // GPU_SHARED request is frame-lived AND must live in GPU-visible memory;
    // testing lifetime first sent every lgx_alloc_gpu_shared() call to the frame
    // arena, so the GPU branch below was unreachable from the public API.
    // Most common path first: plain frame allocations still take the FRAME arm.

    if (unlikely(intent->hint == LGX_HINT_GPU_SHARED)) {
        /* GPU-shared allocation -> GPU pool, host-visible so the CPU can map it */
        ptr = lgx_gpu_alloc(intent->size, 16, LGX_GPU_HOST_VISIBLE);
        tls_intent_stats.gpu_allocations++;

    } else if (likely(intent->lifetime == LGX_LIFETIME_FRAME)) {
        /* Frame-scoped allocation -> frame arena (most common path) */
        ptr = lgx_frame_alloc(intent->size);
        tls_intent_stats.frame_allocations++;

    } else if (intent->lifetime == LGX_LIFETIME_LEVEL ||
               intent->lifetime == LGX_LIFETIME_SESSION) {
        ...unchanged...
    }
```

Move the GPU arm above the FRAME arm; leave every other arm in its original relative order.

- [ ] **Step 4: Verify the test passes**

```bash
cmake --build /tmp/lgx-t4 -j$(nproc) -t unit_test_intent_routing
/tmp/lgx-t4/tests/unit/unit_test_intent_routing; echo "exit=$? (expect 0)"
```

- [ ] **Step 5: Prove the fix is load-bearing**

Restore the original ordering, rebuild, confirm exit 1, restore the fix. Record both.

- [ ] **Step 6: Commit**

```bash
git add src/runtime/lgx_intent_allocator.c tests/unit/test_intent_routing.c tests/unit/CMakeLists.txt
git commit -m "fix(intent): route GPU-sharing hints to the GPU pool

lgx_alloc_gpu_shared() sets lifetime=FRAME and hint=GPU_SHARED. The router
tested lifetime first, so the GPU branch was unreachable from the public API:
every GPU-shared request silently received frame-arena memory that is recycled
after three frames and was never host-visible.

Evaluate an explicit placement hint before the lifetime class, and document the
precedence. Plain frame allocations still take the most-common path.

Adds unit_test_intent_routing, which asserts the routing through the runtime's
own intent stats and fails before the fix.

No public signature changes."
```

---

### Task 5: Guard the GPU path so non-Vulkan builds link

**Files:**
- Modify: `src/runtime/lgx_intent_allocator.c` (the GPU arm moved in Task 4)
- Create: `tests/unit/test_no_vulkan_link.c` (compile-only guard, see below)

**Interfaces:**
- Consumes: Task 4's reordered router.
- Produces: the `no-vulkan` CI job goes green.

- [ ] **Step 1: Reproduce the link failure**

```bash
cd /home/karl/Projects/LGX/.worktrees/ground-truth
cmake -S . -B /tmp/lgx-t5-novk -DCMAKE_BUILD_TYPE=Release -DVulkan_LIBRARY= -DVulkan_INCLUDE_DIR=
cmake --build /tmp/lgx-t5-novk -j$(nproc) > /tmp/lgx-t5-novk.log 2>&1; echo "build rc=$?"
grep -c "undefined reference to \`lgx_gpu_alloc" /tmp/lgx-t5-novk.log
```

Expected: non-zero `undefined reference` count. If CMake still finds Vulkan on this machine, configure with `-DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=TRUE`:

```bash
cmake -S . -B /tmp/lgx-t5-novk2 -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=TRUE
cmake --build /tmp/lgx-t5-novk2 -j$(nproc) > /tmp/novk2.log 2>&1; echo "rc=$?"
grep -c "undefined reference" /tmp/novk2.log
```

Record which invocation reproduces it.

- [ ] **Step 2: Guard the call**

`lgx_gpu_pool.c` is only compiled when Vulkan is found, so `lgx_gpu_alloc` does not exist otherwise. Wrap the GPU arm in the same `#if HAVE_VULKAN` the file already uses elsewhere (see `:152` and `:195`):

```c
#if HAVE_VULKAN
    if (unlikely(intent->hint == LGX_HINT_GPU_SHARED)) {
        ptr = lgx_gpu_alloc(intent->size, 16, LGX_GPU_HOST_VISIBLE);
        tls_intent_stats.gpu_allocations++;
    } else
#endif
    if (likely(intent->lifetime == LGX_LIFETIME_FRAME)) {
        ptr = lgx_frame_alloc(intent->size);
        tls_intent_stats.frame_allocations++;
    } else if (intent->lifetime == LGX_LIFETIME_LEVEL ||
               intent->lifetime == LGX_LIFETIME_LEVEL ||
               intent->lifetime == LGX_LIFETIME_SESSION) {
        ptr = lgx_heap_alloc(intent->size);
        ...
    }
```

(Keep the arm bodies exactly as they are; only the guard and the chain shape change. Verify the `d/else` chain still compiles as a single statement list — restructure with a helper if the `} else` placement is awkward, but keep the logic identical.)

When `HAVE_VULKAN` is 0, a `GPU_SHARED` intent must fall through to the lifetime arm rather than silently returning NULL. Verify the fall-through compiles and behaves; if the intent cannot be satisfied, return NULL explicitly and log once, rather than allocating from the wrong pool.

- [ ] **Step 3: Verify both configurations build**

```bash
cmake --build /tmp/lgx-t5-novk2 -j$(nproc) > /tmp/novk3.log 2>&1; echo "no-vulkan rc=$?"
grep -c "undefined reference" /tmp/novk3.log || echo "0 undefined refs"
cmake --build /tmp/lgx-t4 -j$(nproc) > /tmp/vk.log 2>&1; echo "vulkan rc=$?"
grep -c "warning:" /tmp/vk.log || echo "0 warnings"
```

Both must be 0, with no warnings.

- [ ] **Step 4: Run the CI no-vulkan job locally, exactly as CI does**

Read `.github/workflows/ci.yml`'s `no-vulkan` job and reproduce its configure and build lines verbatim. The job must go from red to green.

- [ ] **Step 5: Commit**

```bash
git add src/runtime/lgx_intent_allocator.c
git commit -m "build: guard the GPU allocation path when Vulkan is absent

lgx_intent_allocator called lgx_gpu_alloc unconditionally while lgx_gpu_pool.c
is only compiled when Vulkan is found, so any build without Vulkan failed to
link. The build only succeeded here because Vulkan happens to be installed.

The file already guards two other sites with #if HAVE_VULKAN; apply the same
guard, and make a GPU_SHARED intent fall through to the lifetime arm rather than
returning NULL when the pool is unavailable.

This turns the deliberately-red no-vulkan CI job green."
```

---

### Task 6: Net connections time out regardless of traffic

**Files:**
- Modify: `src/net/lgx_net.c` (receive path; `last_recv_ms` written only at `:212`, read at `:242`)
- Modify: `tests/net/test_net_basic.c`

**Interfaces:**
- Consumes: nothing.
- Produces: the receive path refreshes activity.

- [ ] **Step 1: Write the failing test**

`last_recv_ms` is assigned in exactly one place, `lgx_net_connect` (`:212`), and read in exactly one place, the timeout check in `lgx_net_update` (`:242`). It is never refreshed when data arrives, so every connection is marked inactive after `timeout_ms` no matter how much traffic it carries.

Add to `tests/net/test_net_basic.c` a case that opens a socket, sends data to a connected peer, calls `lgx_net_update` past the timeout with traffic in between, and asserts the connection is still active. Use the module's real API:

```bash
grep -nE "^(lgx_net_socket_t\*|lgx_result_t|void|bool) lgx_net_" include/lgx_net.h
grep -n "recv\|recvfrom" src/net/lgx_net.c | head
```

Write the test against those real names.

- [ ] **Step 2: Verify it fails**

```bash
cmake --build /tmp/lgx-t6 -j$(nproc) -t test_net_basic
/tmp/lgx-t6/test_net_basic; echo "exit=$? (expect non-zero)"
```

- [ ] **Step 3: Fix it**

In the receive path, refresh the timestamp for the connection the data arrived on, using the same clock (`time_ms()`) the timeout check uses:

```c
    /* Refresh activity so lgx_net_update's timeout check measures idleness,
     * not total connection age. Previously last_recv_ms was only set at connect
     * time, so every connection timed out after timeout_ms regardless of
     * traffic. */
    sock->connections[conn_index].last_recv_ms = time_ms();
```

Match the existing local-variable naming and locking discipline in that function.

- [ ] **Step 4: Verify it passes, then prove load-bearing**

Run the test (expect 0), then revert the one line and confirm it fails again. Record both.

- [ ] **Step 5: Remove the library-code `printf` if it is still there**

`lgx_net.c` around `:244` writes to stdout from library code:

```c
printf("[LGX NET] Connection %u timed out (%lu ms)\n", ...);
```

Replace it with the project's tagged logging (`lgx_log_tagged(LGX_SUBSYSTEM_NET, LGX_LOG_DEBUG, ...)`) or remove it. A library must not write to a process's stdout uninvited.

```bash
grep -n "printf" src/net/lgx_net.c
```

Audit the other module sources for the same defect and fix any found:

```bash
grep -rn "printf(" src/net/ src/asset/ src/tools/ src/profile/ | grep -v "fprintf(stderr" | head
```

- [ ] **Step 6: Commit**

```bash
git add src/net/lgx_net.c tests/net/test_net_basic.c
git commit -m "fix(net): refresh connection activity on receive

last_recv_ms was assigned only in lgx_net_connect and read only in the timeout
check, so it never advanced. Every connection was marked inactive after
timeout_ms no matter how much traffic it carried -- the module's receive paths
were unusable.

- refresh last_recv_ms in the receive path
- replace the library-code printf on the timeout path with tagged logging
- add a regression case covering traffic between update calls

No public signature changes."
```

---

### Task 7: Profile the frame-arena hot path

**Status:** the reference floors are already merged (`bump_floor_*` / `atomic_floor_*`, commit "perf(bench): measure the arena against a bare bump-pointer floor"). This task consumes them; it does not re-add them.

**Files:**
- Create: `docs/05-performance/arena-hotpath-profile.md`
- Modify: `src/runtime/lgx_frame_arena.c` (only if the profile identifies a defect that Task 1-6 do not cover)
- Modify: `benchmarks/baseline_frame_arena.txt` (record the floor comparison)

**Interfaces:**
- Consumes: the two provenance measurements above, and the `FLOOR_LINE` rows the benchmark now emits.
- Produces: a written account of where the arena's per-allocation time goes, with commands, numbers, and a pass/fail against the acceptance bar. This document is the input to Plan 05's positioning and to Plan 06's decision.

- [ ] **Step 1: Confirm the measurement baseline still reproduces**

```bash
cmake -S . -B /tmp/lgx-t7 -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/lgx-t7 -j$(nproc) -t benchmark_benchmark_allocation_throughput
for i in 1 2 3; do /tmp/lgx-t7/benchmarks/benchmark_benchmark_allocation_throughput 2>/dev/null | grep FRAME_RESET_PATTERN; done
```

Record the three runs. If they differ wildly, stop and report — that is a Plan 00 regression, not a Plan 01 question.

- [ ] **Step 2: Get a profile**

**Already ruled out — do not re-investigate these; the measurements are in the provenance block above:**

- **Per-byte work on allocation or reset.** No `memset`/`memcpy` appears on either hot path; all instances are in init, stats, and dump routines.
- **Debug poisoning in Release.** The `0xDEADBEEF` magic check is inside `#if LGX_DEBUG_BUILD`, which resolves to 0 under `-DNDEBUG`.
- **The atomic frame-index load.** `atomic_floor` measured 341 ns against `bump_floor`'s 305 ns at 16B — free on x86-TSO, confirmed by an isolated acquire-vs-relaxed microbenchmark with zero delta. (Still worth measuring on non-x86; it would cost there.)

The budget to account for is ~23-25 ns per allocation at small sizes, against a floor of ~3-5 ns, with the three observers explaining 1.5-4.1 ns of it.

```bash
perf stat -e cycles,instructions,cache-misses,branch-misses \
  /tmp/lgx-t7/benchmarks/benchmark_benchmark_allocation_throughput 2>&1 | tail -20
```

If `perf` is unavailable or blocked by `perf_event_paranoid`, say so plainly and fall back to a `callgrind`-style approach or to manual instrumentation. Record which method was used; do not present a guess as a profile.

Profile the arena row specifically, and compare against the `bump_floor` row in the same process — the difference between those two symbols is the thing to explain.

```bash
perf record -g -- /tmp/lgx-t7/benchmarks/benchmark_benchmark_allocation_throughput > /dev/null 2>&1
perf report --stdio --sort symbol 2>/dev/null | head -30
```

Identify the top contributors inside the allocation success path.

- [ ] **Step 3: Account for the cost explicitly**

The allocation success path currently executes, per call:

1. atomic load of the frame index
2. size alignment arithmetic
3. bounds check against capacity
4. offset update, allocation counter, peak update
5. `check_high_usage_warning` — a floating-point division plus a compare
6. `track_allocation_histogram` — a logarithmic bucket loop plus an array increment
7. `track_allocation_call_site` — a **linear scan** over up to `MAX_ALLOCATION_CALL_SITES` entries, comparing `const char*` pointers
8. three global statistics updates, including a peak comparison
9. a cache-line prefetch
10. `record_profiler_alloc_event`

For each, record: measured or estimated cost, and whether it is required for correctness or is diagnostic. In particular:

- Item 7 is a linear scan whose length grows with distinct call sites. Measure it with many distinct call sites, not one.
- Item 5 uses floating point on a hot path for a threshold compare; an integer compare on `offset * 100 >= capacity * 80` is equivalent and far cheaper. **Measure before changing.**
- Item 10: determine whether it is compiled in Release at all.

```bash
grep -n -A20 "static void record_profiler_alloc_event" src/runtime/lgx_frame_arena.c
grep -n "MAX_ALLOCATION_CALL_SITES\|HISTOGRAM_BUCKET_COUNT" src/runtime/lgx_frame_arena.c | head
```

- [ ] **Step 4: Decide, per observer, keep / gate / remove**

For each of items 4-10, reach a decision and write it down:

- **Keep unconditionally** if it is required for safety or correctness.
- **Gate behind a compile-time flag** (default off in Release, on in Debug) if it is diagnostic.
- **Remove** if it is neither.

Gating must not silently disable a safety check. If an observer guards memory safety, it stays on in every configuration.

- [ ] **Step 5: Measure the result of any change against the acceptance bar**

Re-run and compare the arena row to `bump_floor` and `malloc`:

```bash
/tmp/lgx-t7/benchmarks/benchmark_benchmark_allocation_throughput 2>/dev/null \
  | grep -E "FLOOR_LINE|FRAME_RESET_PATTERN"
```

The bar: **within 2× of `bump_floor` at ≤256B, and faster than `malloc` at every size.** Report each size as pass or fail against it. A fail at the end of this task is a legitimate and useful outcome — it means the arena is not delivering on its premise, and that finding is worth more than a tuned number. Do not weaken the bar to make it pass.

If you gated or removed anything, re-run the benchmark the same way and record before/after per allocation (divide the per-frame delta by 64). If you changed nothing, record that explicitly — "no change, because each observer is required" is a legitimate outcome and must be stated.

- [ ] **Step 6: Write the profile document**

Create `docs/05-performance/arena-hotpath-profile.md` containing:

- the method and tool used, with versions
- the command to reproduce
- a table of every operation on the hot path with its measured cost and its necessity
- the before/after numbers for any change
- an explicit statement of what remains unexplained, if anything
- the machine (CPU model, core, whether pinned, compiler version)

Every number must come from a run you performed in this task.

- [ ] **Step 7: Commit**

```bash
git add docs/05-performance/arena-hotpath-profile.md src/runtime/lgx_frame_arena.c
git commit -m "perf(arena): account for the frame-arena hot path

The arena measures 1.07-1.92x slower than glibc malloc at 1-4KB under the
Plan 00 benchmark. A bump allocator should cost a few nanoseconds, so this is a
defect to locate, not a property of the technique.

Profiles the allocation success path and accounts for each of its ten
operations. Records the measured cost of the three unconditional observers
(1.5-4.1 ns per allocation combined) and a decision for each: keep, gate
behind a compile-time flag, or remove with the reason.

Documents method, commands, and machine so the numbers can be reproduced or
contradicted."
```

---

### Task 8: Add a workload that is not biased toward glibc

**Files:**
- Modify: `benchmarks/benchmark_allocation_throughput.c` (add workload; keep the existing one)
- Modify: `scripts/verify_benchmark_methodology.sh` (extend the guard)
- Modify: `benchmarks/baseline_frame_arena.txt` (add the new workload's results with provenance)

**Interfaces:**
- Consumes: Task 7's conclusions.
- Produces: a `MIXED_WORKLOAD` line per size from `benchmark_benchmark_allocation_throughput`.

- [ ] **Step 1: Write down what the current benchmark measures, honestly**

The existing tight loop is same-size alloc/free repeated, which is glibc's best case: tcache hit, hot in L1, immediate reuse of the same block. A bump allocator has no way to win a contest with no variance to exploit. Any "parity" number from this workload says more about glibc's fast path than about the arena.

State this in the benchmark's header comment before changing anything. A benchmark whose bias is undocumented is a benchmark that will be quoted out of context.

- [ ] **Step 2: Add a mixed-size workload with many live objects**

Add a second workload alongside the existing one (do not replace it — both are informative):

```c
/*
 * Mixed-size workload with many simultaneously-live objects.
 *
 * The tight loop measures same-size alloc/free with immediate reuse, which is
 * glibc's best case (tcache hit, hot in L1). It cannot show what a frame arena
 * is actually for. This workload allocates a realistic mix of sizes, keeps
 * every object live until the end of the frame, then resets:
 *
 *   - mixed sizes, so size-class routing matters
 *   - all objects live at once, so peak usage and fragmentation behaviour show
 *   - no per-object free, only a frame reset
 *
 * Emits: MIXED_WORKLOAD size=<n> median=<ns>ns p99=<ns>ns peak=<bytes>
 */
#define MIXED_OBJECTS   4096
#define MIXED_FRAMES    200

static void bench_mixed_frame(void* ctx) {
    (void)ctx;

    for (int f = 0; f < MIXED_FRAMES; f++) {
        lgx_frame_reset();                       /* start clean */

        void* live[MIXED_OBJECTS];
        size_t peak = 0;

        for (int i = 0; i < MIXED_OBJECTS; i++) {
            /* Deterministic pseudo-mixed sizes: a power-law-ish spread from
             * 16B to 4KB, no RNG so runs are comparable. */
            size_t sz = 16u << (i % 9);          /* 16B .. 4096B */
            live[i] = lgx_alloc_frame(sz);
            if (live[i]) {
                memset(live[i], 0, sz);          /* touch: models real use */
                peak += sz;
            }
        }

        /* Everything stays live until here -- no frees, by design. */
        (void)live;
    }
}
```

Choose sizes deliberately and document the distribution you picked and why. Note in a comment that `peak` is the quantity of interest for fragmentation claims.

- [ ] **Step 3: Report both allocation outcomes and fragmentation**

Extend the emitted line to carry peak usage and overflow/fallback counts, because those are where the arena's real behaviour shows:

```c
    printf("MIXED_WORKLOAD size=%zu median=%.0fns p99=%.0fns peak_bytes=%zu\n",
           sz, median, p99, peak);
```

```bash
grep -nE "overflow_count|fallback_count|peak_usage" include/lgx_runtime.h include/lgx_types.h | head
```

Report the frame arena's `overflow_count`/`fallback_count` across the mixed workload. **If any allocation silently fell back to the persistent heap, the frame-arena numbers are meaningless for that size** — say so explicitly and size the arena so it does not happen, rather than reporting a fallback-contaminated number as if it were a frame-arena measurement. This is exactly the error Plan 00 caught in the original benchmark.

- [ ] **Step 4: Extend the methodology guard**

```bash
grep -n "MIXED_WORKLOAD" scripts/verify_benchmark_methodology.sh || echo "not yet guarded"
```

Add a check that fails if `MIXED_WORKLOAD` is removed from the benchmark, so the workload cannot silently disappear.

- [ ] **Step 5: Run both workloads and record the comparison**

```bash
cmake --build /tmp/lgx-t7 -j$(nproc) -t benchmark_benchmark_allocation_throughput
./scripts/verify_benchmark_methodology.sh; echo "guard exit=$?"
/tmp/lgx-t7/benchmarks/benchmark_benchmark_allocation_throughput 2>/dev/null | grep -E "FRAME_RESET_PATTERN|MIXED_WORKLOAD"
```

Run at least 3 times. Report:

- the tight-loop result (glibc's best case) — expect the arena to be at or near parity
- the mixed-workload result — this is the one that tests the hypothesis
- the fragmentation/peak numbers
- whether either workload changed the arena's standing

**If the mixed workload shows the arena winning, that is the number Plan 05 may build on. If it shows the arena losing, that is equally important to record.** Do not tune the workload until it produces the answer you expected; pick the workload first, report what it says.

- [ ] **Step 6: Update the baseline file with full provenance**

Add the mixed-workload results to `benchmarks/baseline_frame_arena.txt` with the same rigor as the existing content: command, machine, repeats, and a note that the tight loop favours glibc. Follow Plan 00's format exactly.

- [ ] **Step 7: Update README's performance section with both workloads**

The README currently presents only the tight-loop table. Add the mixed-workload result alongside it, labelled with what each workload does and does not demonstrate. If the mixed workload changes the conclusion, the headline sentence changes with it.

- [ ] **Step 8: Commit**

```bash
git add benchmarks/benchmark_allocation_throughput.c scripts/verify_benchmark_methodology.sh \
        benchmarks/baseline_frame_arena.txt README.md
git commit -m "perf(bench): add a workload that can test what the frame arena is for

The existing benchmark is same-size alloc/free with immediate reuse, which is
glibc's best case: a tcache hit on a hot block. A bump allocator has no
variance to exploit there, so 'parity' from that workload describes glibc's
fast path, not the arena.

Add a mixed-size, many-live-objects workload: sizes spread across classes, every
object live until the frame reset, no per-object free. Report peak usage and
overflow/fallback counts alongside latency, because those are where fragmentation
behaviour is visible.

Both workloads are kept. The methodology guard fails if the mixed workload is
removed, and the baseline records which workload favours which allocator.

No runtime behaviour changes."
```

---

## Verification

After all eight tasks:

```bash
cd /home/karl/Projects/LGX/.worktrees/ground-truth

# 1. Clean Release build, zero warnings
rm -rf /tmp/lgx-final1
cmake -S . -B /tmp/lgx-final1 -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build /tmp/lgx-final1 -j$(nproc) 2>&1 | tee /tmp/final1.log
grep -c "warning:" /tmp/final1.log        # must be 0

# 2. Clean Debug/ASan build
cmake -S . -B /tmp/lgx-final1-asan -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build /tmp/lgx-final1-asan -j$(nproc) > /dev/null

# 3. Both Vulkan configurations link
cmake -S . -B /tmp/lgx-final1-novk -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=TRUE > /dev/null
cmake --build /tmp/lgx-final1-novk -j$(nproc) > /dev/null && echo "no-vulkan links OK"

# 4. Test suite, via the CI runner (honest known-failure policy)
cmake --build /tmp/lgx-final1 -j$(nproc) --target benchmark_benchmark_allocation_throughput
./.github/ci/run-tests.sh /tmp/lgx-final1 release 3

# 5. Gates
./scripts/verify_benchmark_methodology.sh
./scripts/verify_test_integrity.sh 2>&1 | tail -2   # must still exit 1 (debt, not regression)

# 6. Benchmarks emit both workloads
/tmp/lgx-final1/benchmarks/benchmark_benchmark_allocation_throughput 2>/dev/null \
  | grep -E "FRAME_RESET_PATTERN|MIXED_WORKLOAD"
```

Expected: Release and ASan builds clean with zero warnings; the no-Vulkan build links; the CI runner reports zero unlisted failures; the methodology guard passes; the integrity gate still exits 1 (that is Plan 03's debt, and it must not have been silently reduced); both workload line types are present.

## Out of Scope

- Allocator replacement benchmarks (mimalloc/tcmalloc/jemalloc) — that is Plan 06, and it decides whether the malloc-replacement positioning survives at all.
- The 25 assert-only tests and the 12 disabled tests — Plan 03.
- `lgx_integration.h`'s 21 unimplemented declarations — Plan 01 follow-up, after these correctness fixes land.
- Anything touching `build/`, the other CI workflows, or the README's non-performance claims — Plan 04.