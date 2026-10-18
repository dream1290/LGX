# Phase 1 Completion — Build Restore, Memory-Safety Fixes, Arena Performance

**Created:** 2026-10-18
**Supersedes:** `2026-10-11-correctness-and-arena-performance.md` (Plan 01).
Plan 01 is partially executed: its measurement tasks 7-8 produced commits `ceb7855`, `3abeb60`, `40a4721`, `b6c5ab7`, `bcd8b52`, which left `main` broken and the tree full of junk. This plan restores a trustworthy baseline first, then executes Plan 01's correctness tasks (with four additional defects found in a full re-investigation), then completes the arena performance work under measurement discipline.

**For the executing engineer:** every task below is self-contained. Read each file before editing. Line numbers are anchors from commit `bcd8b52` (main, 2026-10-18); if a line has moved, locate the exact code block shown and edit that. Follow every commit message exactly — they are part of the deliverable. Do not combine tasks into one commit.

**Workflow:** start with Task A0. From A0 onward, work happens on **branches with pull requests** — never push `main` directly once the repository ruleset exists. Suggested branches: `fix/revert-arena-misstep` (A0), `fix/phase1-safety` (B1–B8), `bench/arena-measurement` (D1–D4). The repository owner merges, pushes, changes settings, tags and releases; the agent has no credentials for those. If a push or merge is ever rejected, that is the system working — report it, do not route around it.

**Baseline state (verified 2026-10-18):**
- `main == origin/main == bcd8b52`. HEAD does not build: 12 TUs + the C++17 header gate fail with `implicit-function-declaration`, `int-conversion`, `conflicting types` in `include/lgx_runtime.h`.
- `git status`: only ` M benchmark_allocation_throughput.csv`.
- Local branch `ground-truth` is checked out in `.worktrees/ground-truth` (stale; cleaned up in E1).
- Four prior perf commits are on main; `3abeb60` removed frame-arena statistics that tests still assert, so the test suite is red for that reason too.

---

## Task index

| Task | What | Defect fixed | New test |
|---|---|---|---|
| A0 | Fix `main` with three reverts on a branch (+ `.gitignore`) | Build break + junk + stats | existing suite |
| ~~A1~~ | ~~Revert `lgx_alloc_frame` to intent-based API~~ | superseded by A0 (`git revert b6c5ab7`) | — |
| ~~A2~~ | ~~Untrack the 22 junk files~~ | superseded by A0 (`git revert bcd8b52`) | — |
| ~~C1~~ | ~~Restore frame-arena statistics~~ | superseded by A0 (`git revert 3abeb60`) | — |
| C2 | `lgx_config_create` uses `calloc` | Uninitialised arena size read | `test_config_init` (new) |
| A3 | Record honest baseline (build, tests, benchmark) | No trustworthy baseline exists | provenance file |
| A4 | Add repo-root `AGENTS.md` with working rules | Agent behaviour not constrained | hooks/CI in B6 |
| B1 | GPU buddy allocator: index handles, split realloc order, double-insert, alignment/size bounds | 4 memory-safety defects | extend `test_gpu_buddy_allocator.c` |
| B2 | GPU pool init order and failure cleanup | `get_memory` always NULL; leaks on init failure | extend `test_gpu_pool.c` |
| B3 | Frame arena chained growth (no mid-frame munmap) | Use-after-free on overflow | `test_frame_arena_growth.c` (new) |
| B4 | Chaos testing self-deadlock | First chaos decision deadlocks | `test_chaos_deadlock.c` (new, explicit target) |
| B5 | GPU-shared hint routed before frame lifetime | `lgx_alloc_gpu_shared` never reaches GPU | extend `test_intent_allocator.c` |
| B6 | Non-Vulkan build links; CI job becomes a real gate | `undefined reference to lgx_gpu_alloc` | no-vulkan CI job |
| B7 | Net `last_recv_ms` refreshed on receive; `printf` → `lgx_log_tagged` | Connections always time out | extend `tests/net/test_net_basic.c` |
| B8 | Leak-detector init order, hardware status fields, memory-manager tracking pointer | Uninit reads; untrack never matches | 2 new unit tests |
| D1 | Disassembly evidence for the arena hot path | Measurement discipline | `objdump` log |
| D2 | Measured removal of remaining hot-path observers | Arena still slow | benchmark deltas |
| D3 | Mixed-size workload benchmark | Tight loop is glibc's best case | new benchmark scenario |
| D4 | README claims table + ratio gate + snapshot | Claims not backed by commands | CI gate |
| E1 | Close-out: index, full verification, cleanup | — | all gates |

**Cross-cutting rules (from `docs/superpowers/plans/README.md`, unchanged):** every fix lands with a test that fails before it; no new warnings under `-Werror`; one commit per task; memory-safety fixes verified under ASan/UBSan before "done"; never weaken a gate to make it pass; no claim without the command that produces it.

---

## Task A0 — Fix `main` first: three reverts on a branch

**This task replaces A1, A2 and C1.** The reverts restore the build, remove the junk, and restore the statistics in three reviewable commits with no hand-editing. A1/A2/C1 sections are kept below for reference only — they exist for the case where a selective hand-edit is preferred over a revert; if you follow A0, skip them.

**Facts verified before writing this task (2026-10-18):**
- The build breaker is **`b6c5ab7`**, not `bcd8b52`. `git show --name-status bcd8b52` lists 22 files, all additions: no header, no `.gitignore`.
- `bcd8b52`'s message makes two false claims: that it "fixes the .gitignore" (it does not touch the file) and that it measured "~12ns/alloc" (the number came from a stale binary). `bcd8b52`'s stated numbers are **withdrawn**; the revert commit says so, the same way `c7147d4` retracted the README's 200× claim.
- `opencode.json` in `bcd8b52` contains **no secrets** — content is `{"$schema": "https://opencode.ai/config.json", "plugin": ["superpowers@git+https://github.com/obra/superpowers.git"]}`. Nothing to rotate.
- `git revert bcd8b52` alone leaves `main` red: the build still fails (header) and `test_frame_arena`/`test_frame_arena_polish` still fail (statistics).

**Steps:**

```bash
git switch -c fix/revert-arena-misstep

# Discard the pre-existing local modification so the revert is clean
git checkout -- benchmark_allocation_throughput.csv

git revert bcd8b52     # junk out; amend the generated message (below)
git revert b6c5ab7     # build restored: intent-based header returns
git revert 3abeb60     # frame-arena statistics return; stats tests green
```

**Revert commit messages.** `git revert` generates a message; use it as the first line/body, then append the explanation. Exact texts:

1. bcd8b52 revert — append:

```
bcd8b52 committed scratch files, six assert-only tests, two ELF binaries
and a benchmark CSV via git add -A. Its message claimed two things that
are false: that it fixed .gitignore (the commit does not touch the file)
and that it measured ~12ns/alloc (measured from a stale binary). The
numbers stated in bcd8b52 are withdrawn. The .gitignore coverage it
claimed is added in the next commit of this branch.
```

2. b6c5ab7 revert — append:

```
b6c5ab7 pointed the public inline lgx_alloc_frame at
lgx_frame_alloc_internal, which is only declared in the internal header.
Every translation unit including lgx_runtime.h failed to compile
(-Werror=implicit-function-declaration, -Wint-conversion). This revert
restores the verified pre-b6c5ab7 intent-based bodies for both the C and
C++ variants. It also returns lgx_intent_allocator.c:270 to the public
lgx_frame_alloc() call; both forms compile.
```

3. 3abeb60 revert — append:

```
3abeb60 removed arena->allocations++, the per-arena peak update and four
global statistic increments from the allocation success path, claiming
cache-line contention. Its own message records no measured change; the
counters are plain increments, not atomics. test_frame_arena and
test_frame_arena_polish assert on them and were red without them. This
restores the code exactly as it was before 3abeb60.
```

**Then add the missing `.gitignore` coverage as a fourth commit on the same branch** (a revert alone deletes the files from HEAD but does not stop the same `git add -A` accident recurring). Append the block from Task A2 verbatim:

```
chore(gitignore): ignore the scratch paths reclaimed in the revert

The revert removed these from tracking but not from the filesystem; a
future git add -A would re-add them without explicit ignore rules.
```

**Push and open the PR** (the owner pushes if the agent has no credentials):

```bash
git push -u origin fix/revert-arena-misstep
```

**Expected CI result for this PR:** `build-and-test` and `asan-ubsan` green. The `no-vulkan` job stays red **by design** until Task B6 — the ruleset must require only `build-and-test` at this stage. Do not "fix" no-vulkan in this PR.

**Verification (local, before pushing):**
1. Fresh clone into a temp dir (not just a fresh build dir), checkout the branch, build, test:
   ```bash
   git clone . /tmp/opencode/lgx-a0 && git -C /tmp/opencode/lgx-a0 switch fix/revert-arena-misstep
   cmake -S /tmp/opencode/lgx-a0 -B /tmp/opencode/lgx-a0-build -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build /tmp/opencode/lgx-a0-build -j$(nproc)
   ctest --test-dir /tmp/opencode/lgx-a0-build --output-on-failure
   ```
   Build: zero warnings. Tests: frame-arena tests green; known-red until later tasks only (chaos has no test yet, GPU tests may fail if this host exercises B1/B2 defects).
2. `./scripts/verify_test_integrity.sh` exits 0 at the 98/68/30 census; `./scripts/count_tests.sh` matches its baseline.

**Commit messages (summary of the branch):** the three revert messages above plus the `.gitignore` commit. No performance numbers in any of them.

---

## Task A1 — Restore the build: revert `lgx_alloc_frame` to the intent-based API

> **Superseded by A0** (`git revert b6c5ab7`). Keep for the hand-edit route only.

**Defect:** `b6c5ab7` changed the two `lgx_alloc_frame` inline functions in `include/lgx_runtime.h` to call `lgx_frame_alloc_internal`, which is only declared in the internal header `include/lgx/lgx_runtime_internal.h`. Every translation unit that includes the public header now fails to compile.

**Files:** `include/lgx_runtime.h`.

**Current code (broken), `include/lgx_runtime.h:53-58`:**

```c
#ifdef __cplusplus
extern "C++" {

static inline void* lgx_alloc_frame(size_t size) {
    return lgx_frame_alloc_internal(size, "unknown", 0);
}
```

**Current code (broken), `include/lgx_runtime.h:93-97`:**

```c
#else
// C version with designated initializers
static inline void* lgx_alloc_frame(size_t size) {
    return lgx_frame_alloc_internal(size, "unknown", 0);
}
```

**Exact original code to restore (C++ branch) — verified `git show b6c5ab7^:include/lgx_runtime.h`:**

```c
static inline void* lgx_alloc_frame(size_t size) {
    lgx_allocation_intent_base_t intent;
    intent.struct_size = sizeof(lgx_allocation_intent_base_t);
    intent.size = size;
    intent.access_pattern = LGX_ACCESS_SEQUENTIAL;
    intent.lifetime = LGX_LIFETIME_FRAME;
    intent.hint = LGX_HINT_CRITICAL_PATH;
    intent.validation_policy = LGX_INTENT_TRUST;
    return lgx_alloc_with_intent(&intent);
}
```

**Exact original code to restore (C branch):**

```c
static inline void* lgx_alloc_frame(size_t size) {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = size,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    return lgx_alloc_with_intent(&intent);
}
```

**Do not change** `src/runtime/lgx_intent_allocator.c:270` (`ptr = lgx_frame_alloc_internal(intent->size, "unknown", 0);`). That file includes the internal header (`:43`) and compiles; the direct call there is deliberate (it skips re-validating an intent that was just validated).

**Verification:**
1. `cmake -S . -B /tmp/opencode/lgx-build -DCMAKE_BUILD_TYPE=RelWithDebInfo`
2. `cmake --build /tmp/opencode/lgx-build -j$(nproc) 2>&1 | tee /tmp/opencode/build-a1.log` — zero warnings, zero errors.
3. The C++17 header gate (whatever target compiles the header as C++) must pass: if unsure which target, `cmake --build /tmp/opencode/lgx-build -j$(nproc)` builds it; grep the log for `error`.

**Commit message:**
```
fix(header): restore intent-based lgx_alloc_frame

b6c5ab7 pointed the public inline at lgx_frame_alloc_internal, which is
only declared in the internal header. Every TU including lgx_runtime.h
failed to compile (-Werror=implicit-function-declaration). Reverted both
the C and C++ variants to the verified pre-b6c5ab7 intent-based bodies.
The direct internal call in lgx_intent_allocator.c:270 is kept: that TU
includes the internal header and must not re-validate a fresh intent.

Verified: clean cmake configure + build, zero warnings.
```

---

## Task A2 — Untrack the junk committed in `bcd8b52`

> **Superseded by A0** (`git revert bcd8b52` plus the `.gitignore` commit). The `.gitignore` block below is used verbatim in A0's fourth commit. Keep for the hand-edit route only.

**Defect:** `bcd8b52` ("chore: fix .gitignore...") committed with `git add -A` and swept in scratch files, editor config, skipped tests, profiler binaries and a benchmark CSV. The junk `tests/test_*.c` files are counted by `scripts/verify_test_integrity.sh` and push the integrity census to 102 tests / 34 incapable, which the ratchet rejects.

**Files:** repository root, `tests/`, `tools/lifetime-profiler/`, `.gitignore`.

**Expected files (verify against reality first):**

```bash
git show --name-only bcd8b52
```

The list that must be untracked (all paths relative to repo root):
`f_str.c`, `g_dead_tern.c`, `h_cmp_only.c`, `i_cast.c`, `j_dead_shape.c`, `k_exit_var.c`, `l_helper_ret.c`, `opencode.json`, `tests/Makefile`, `tests/test_alignment.c`, `tests/test_basic.c`, `tests/test_framework.h`, `tests/test_overflow.c`, `tests/test_statistics.c`, `tools/lifetime-profiler/` contents (binaries + scratch), `benchmark_allocation_throughput.csv`.

**Steps:**
1. `git rm -r --cached` each path (or `git rm -r --cached` the whole verified list in one command). Do **not** delete files that are tracked from before `bcd8b52`; only the ones that commit introduced. Cross-check with:
   ```bash
   git diff --name-status bcd8b52^ bcd8b52
   ```
2. Append to `.gitignore` (root), under a comment:
   ```
   # Scratch files accidentally committed in bcd8b52 (untracked in A2)
   /f_str.c
   /g_dead_tern.c
   /h_cmp_only.c
   /i_cast.c
   /j_dead_shape.c
   /k_exit_var.c
   /l_helper_ret.c
   /opencode.json
   /benchmark_allocation_throughput.csv
   /tests/Makefile
   /tests/test_alignment.c
   /tests/test_basic.c
   /tests/test_framework.h
   /tests/test_overflow.c
   /tests/test_statistics.c
   /tools/lifetime-profiler/
   ```
3. `git status` must show the files as deleted-from-index (staged) and not as modified working-tree files. The scratch files may remain on disk; they must not be tracked.

**Verification:**
1. `./scripts/verify_test_integrity.sh` — back to 98 total / 68 capable / 30 incapable (exact counts from the script's output; the point is the ratchet passes).
2. `./scripts/count_tests.sh` — matches its protected baseline expectations.

**Commit message:**
```
chore(repo): untrack scratch files committed in bcd8b52

bcd8b52 used git add -A and committed scratch sources, opencode.json,
six assert-only tests under tests/, two profiler binaries and the
benchmark CSV. The extra tests inflated the integrity census to
102/34, tripping the ratchet. Untracked the verified bcd8b52 file list
and added explicit .gitignore entries.

Verified: verify_test_integrity.sh passes at the Plan 00 baseline.
```

---

## Task C1 — Restore frame-arena statistics removed by `3abeb60`

> **Superseded by A0** (`git revert 3abeb60`). Keep for the hand-edit route only.

**Defect:** `3abeb60` deleted, from the allocation success path: `arena->allocations++`, the per-arena peak update, `stats.total_allocations++`, `stats.total_bytes_allocated += size`, `stats.allocations_since_reset++`, and `stats.peak_usage_bytes`. Tests `tests/phase0/test_frame_arena.c` (`total_allocations == 3`) and `tests/phase0/test_frame_arena_polish.c` (peak assertions, `total_allocations == 100`, `total_bytes_allocated == 100*1024`) fail. The commit was described as a perf win but its own message says the measurement was unchanged — the counters are plain increments, not atomics. This restores them; D2 measures whether they actually cost anything, with disassembly evidence.

**Files:** `src/runtime/lgx_frame_arena.c`, function `lgx_frame_alloc_internal`, region around `:613-623`.

**Current code (`bcd8b52`, after the `3abeb60` deletions):**

```c
allocation_succeeded:
    // Update offset
    arena->offset = new_offset;
    
    void* ptr = arena->base + old_offset;
    if (new_offset + CACHE_LINE_SIZE < arena->capacity) {
        prefetch(arena->base + new_offset + CACHE_LINE_SIZE);
    }
    
    return ptr;
```

(Note the stray `return ptr;` at column 0 — the diff tools mangled the indentation. Fix it to four spaces.)

**Exact replacement:**

```c
allocation_succeeded:
    // Update offset
    arena->offset = new_offset;
    arena->allocations++;
    
    // Update peak usage
    if (new_offset > arena->peak_usage) {
        arena->peak_usage = new_offset;
    }
    
    // Update global statistics
    g_frame_arena_state.stats.total_allocations++;
    g_frame_arena_state.stats.total_bytes_allocated += size;
    g_frame_arena_state.stats.allocations_since_reset++;
    
    if (arena->peak_usage > g_frame_arena_state.stats.peak_usage_bytes) {
        g_frame_arena_state.stats.peak_usage_bytes = arena->peak_usage;
    }
    
    void* ptr = arena->base + old_offset;
    if (new_offset + CACHE_LINE_SIZE < arena->capacity) {
        prefetch(arena->base + new_offset + CACHE_LINE_SIZE);
    }
    
    return ptr;
```

Notes:
- Do **not** re-add `record_profiler_alloc_event` (removed by `40a4721`; that removal is measured and intentional). Do not re-add the high-usage warning or the histogram/call-site tracking (removed by `ceb7855`; the comments in that commit explain why).
- B3 later changes `new_offset` semantics; keep this task's code exactly as above and let B3 adjust it, so each commit stays reviewable.

**Verification:**
1. Rebuild; `ctest -R 'test_frame_arena' --output-on-failure` — both `test_frame_arena` and `test_frame_arena_polish` must pass.
2. `ctest --output-on-failure` — whole suite (expect remaining failures only in tests covered by later tasks).

**Commit message:**
```
fix(arena): restore per-arena and global allocation statistics

3abeb60 removed arena->allocations++, the per-arena peak update, and the
four global statistic increments from the success path, claiming cache
contention. Its own measurement showed no change: these are plain
increments, not atomics. test_frame_arena and test_frame_arena_polish
assert on all of them and are red without them.

Verified: ctest -R test_frame_arena passes; D2 (later) measures whether
any of these actually cost clock cycles before removing them again.
```

---

## Task C2 — Zero-initialise the runtime config

**Defect:** `lgx_config_create` (`src/runtime/lgx_runtime_core.c:230-241`) allocates with `malloc` and leaves most fields uninitialised. `lgx_runtime_init` later reads `config->frame_arena_size` and `config->frame_arena_max_size` (`:612-617`) behind `> 0` guards — garbage nonzero values silently change arena sizing.

**Files:** `src/runtime/lgx_runtime_core.c`, `tests/unit/` (new test), `tests/unit/CMakeLists.txt`.

**Change:** in `lgx_config_create`, replace `malloc(sizeof(lgx_runtime_config_t))` with `calloc(1, sizeof(lgx_runtime_config_t))`. Do not otherwise restructure the function.

**New test — `tests/unit/test_config_init.c`:**

```c
#include "lgx_runtime.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);

    /* calloc: every field that lgx_runtime_init reads unguarded or
       behind a "> 0" check must start at zero. */
    assert(config->frame_arena_size == 0);
    assert(config->frame_arena_max_size == 0);

    lgx_config_destroy(config);
    printf("test_config_init: OK\n");
    return 0;
}
```

Register in `tests/unit/CMakeLists.txt` following the existing pattern in that file (read it first; it has a per-test `add_executable` + `add_test` shape, likely with sanitizer flags inherited from the directory).

**Honest note:** pre-fix this test can pass by luck — fresh heap often hands back zeroed pages. It is a regression guard, not the proof; the proof is the `calloc` call itself, which cannot return uninitialised memory. If `lsan.supp`/CI policy requires, state that in the commit.

**Verification:** rebuild; `ctest -R test_config_init --output-on-failure`; full `ctest` still shows only the known-red tests.

**Commit message:**
```
fix(config): zero-initialise lgx_runtime_config_t

lgx_config_create allocated with malloc and never initialised
frame_arena_size / frame_arena_max_size, which lgx_runtime_init reads
behind "> 0" guards. Garbage bytes silently resized the frame arena.

Verified: test_config_init added; full ctest unchanged otherwise.
```

---

## Task A3 — Record the honest baseline

**Purpose:** after A0 (reverts) and C2 the repository is describable in one page of numbers. Everything in D2/D4 compares against this record. No source changes in this task.

**Steps:**
1. Fresh configure/build of the verification tree:
   ```bash
   cmake -S . -B /tmp/opencode/lgx-build -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build /tmp/opencode/lgx-build -j$(nproc)
   ```
   and a Release tree for benchmarks:
   ```bash
   cmake -S . -B /tmp/opencode/lgx-release -DCMAKE_BUILD_TYPE=Release
   cmake --build /tmp/opencode/lgx-release -j$(nproc)
   ```
2. Full suite: `ctest --test-dir /tmp/opencode/lgx-build --output-on-failure 2>&1 | tail -40`. Record exact pass/fail counts. Known-red groups at this point: chaos (no test yet — B4), GPU (B1/B2 may still fail if this machine's driver exposes the defects), no-vulkan job (B6). Everything else must be green.
3. Gates: `./scripts/verify_test_integrity.sh`, `./scripts/count_tests.sh`, `./scripts/verify_benchmark_methodology.sh`, `./scripts/check_gate_scripts_tracked.sh` — record each exit code.
4. Benchmark, pinned and quiet:
   ```bash
   taskset -c 2 /tmp/opencode/lgx-release/benchmark_allocation_throughput 2>&1 | tee /tmp/opencode/bench-a3.txt
   ```
   Run it from `/tmp/opencode` so the CSV lands there, not in the repo. Record `bump_floor_16B`, `atomic_floor_16B`, `lgx_frame_16B` (and 64/256B) medians.
5. Append a dated section to `benchmarks/baseline_frame_arena.txt`:
   ```
   == 2026-10-18 · main@<sha> · post A0/C2 ==
   host: <CPU model>, governor <governor>
   commands: <the four commands above>
   ctest: <passed>/<total>
   gates: <exit codes>
   bench medians (ns per frame): bump_floor_16B=..., atomic_floor_16B=..., lgx_frame_16B=...
   ```
   Numbers only from the commands in this file. No numbers typed from memory.

**Commit message:**
```
docs(bench): record post-restore baseline (2026-10-18)

Baseline after A1 (build), A2 (repo hygiene), C1 (stats), C2 (config).
Contains ctest counts, gate exit codes, and pinned benchmark medians,
all with their producing commands. D2/D4 compare against this.
```

---

## Task A4 — Add repo-root `AGENTS.md`

**Why:** the agent works as a contributor with scoped access, not an admin. The rules it must follow live in the repo, so they are reviewable and versioned. This file did not exist before this plan (verified).

**File:** `AGENTS.md` (repo root), new. Exact content:

```markdown
# Working rules for agents in this repository

## Authority
- Work on branches and open pull requests. Never push to `main`.
- The human owner performs: merges, repository setting changes, secrets,
  tags and releases. Do not attempt them.

## Evidence
1. Never write a performance number in a commit message, README, or repo
   description unless it links to benchmark output committed in the same
   change. Numbers live in `docs/05-performance/` and
   `benchmarks/`; commit messages reference those files, they do not
   restate the numbers.
2. Before saying "done": clone the repository fresh into a temp directory
   and build and test from that clone. A build tree that already exists
   proves nothing about what a new contributor gets.
3. Run `git status` and `git diff --stat` before every commit. Never
   commit binaries, logs, editor/agent config, probe output, or generated
   CSVs.
4. A commit message states what changed and how it was verified. It does
   not state what was achieved ("faster", "robust", "complete") without
   the reproducing command.

## Writing
5. User-facing docs (README, descriptions, changelogs): no emojis, no
   checkmark tables, no "robust", "comprehensive", "seamless",
   "guaranteed". Internal plan files under `docs/superpowers/plans/` may
   use status markers.
6. If a claim in a doc cannot be verified from this repository, delete it
   or label it unverified.

## Gates
7. Never weaken or bypass a gate to make something pass. A rejected push,
   failing ratchet, or red check is information; report it.
```

**Verification:** `git status` shows only `AGENTS.md` added; file contains exactly the rules above; no other file changed.

**Commit message:**

```
docs: add AGENTS.md working rules

Agent rules moved into the repo so they are versioned and reviewable:
branches + PRs only, evidence rules for numbers (docs, not commit
messages), fresh-clone verification before "done", commit hygiene, doc
language rules, and a no-gate-weakening rule.
```

---

## Task B1 — GPU buddy allocator memory safety

Four defects in `src/runtime/lgx_gpu_pool.c`. All four are fixed in this task; the regression test exercises all four. Do not split.

**Defect 1 — use-after-free via raw block pointers in handles.** `struct lgx_gpu_allocation` (`:877-884`) stores `buddy_block_t* block`. `buddy_split` reallocs `allocator->all_blocks` (`:248`). Every outstanding handle's pointer is then dangling; `lgx_gpu_free` derefs it (`:974`, `:978`).

**Defect 2 — free-list double insertion after rebuild.** In `buddy_split`, when `did_realloc` is true: the block is unlinked at `:234-241`, then `buddy_rebuild_free_lists` re-adds all free blocks including the parent (`:278`), then `:285-295` inserts `block` and `buddy` **again**. The second insertion into a list that already contains them creates cycles and duplicate entries.

**Defect 3 — removal before realloc failure.** The block is unlinked before the capacity check at `:245`. If `realloc` fails (`:250-252`), the block stays unlinked and is orphaned in no free list while still marked free.

**Defect 4 — out-of-bounds free-list index / undersized block.** In `buddy_alloc` (`:431-446`): `while (level_to_size(level) < alignment) level++;` can push `level` to `NUM_BUDDY_LEVELS` (19) for alignments > 64MB, and then `allocator->free_lists[level]` (`:308`, `:452`) reads out of bounds. Separately, `size_to_level` clamps any size ≥ 64MB to level 18, so `lgx_gpu_alloc(128MB, ...)` returns a 64MB block — the caller writes past the allocation.

### B1a — Move capacity growth before unlinking, delete the rebuild double-insert

**Current code, `src/runtime/lgx_gpu_pool.c:228-298`** (read the file; the exact text is):

```c
    size_t block_index = block - allocator->all_blocks;
    
    // Remove from current free list (using index to be safe)
    block = &allocator->all_blocks[block_index];
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        allocator->free_lists[block->level] = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    // Create buddy block
    bool did_realloc = false;
    if (allocator->num_blocks >= allocator->max_blocks) {
        // Expand block array
        size_t new_max = allocator->max_blocks * 2;
        buddy_block_t* new_blocks = (buddy_block_t*)realloc(allocator->all_blocks, 
                                                            new_max * sizeof(buddy_block_t));
        if (!new_blocks) {
            return NULL;
        }
        allocator->all_blocks = new_blocks;
        allocator->max_blocks = new_max;
        did_realloc = true;
    }
    
    // Restore block pointer after potential realloc
    block = &allocator->all_blocks[block_index];
    buddy_block_t* buddy = &allocator->all_blocks[allocator->num_blocks++];
    
    // Split into two blocks at lower level
    uint8_t new_level = block->level - 1;
    VkDeviceSize new_size = level_to_size(new_level);
    
    block->level = new_level;
    block->size = new_size;
    
    buddy->offset = block->offset + new_size;
    buddy->size = new_size;
    buddy->level = new_level;
    buddy->is_free = true;
    buddy->next = NULL;
    buddy->prev = NULL;
    
    // If we did realloc, rebuild all free lists to fix pointers
    if (did_realloc) {
        buddy_rebuild_free_lists(allocator);
        
        // Restore pointers after rebuild
        block = &allocator->all_blocks[block_index];
        buddy = &allocator->all_blocks[allocator->num_blocks - 1];
    }
    
    // Add both blocks to lower level free list
    block->next = buddy;
    block->prev = NULL;
    buddy->next = allocator->free_lists[new_level];
    buddy->prev = block;
    
    if (allocator->free_lists[new_level]) {
        allocator->free_lists[new_level]->prev = buddy;
    }
    
    allocator->free_lists[new_level] = block;
    
    return block;
}
```

**Exact replacement of the whole function body (from `size_t block_index = ...` to `return block;`):**

```c
    size_t block_index = block - allocator->all_blocks;

    // Ensure block array capacity BEFORE unlinking: realloc invalidates
    // every pointer including free-list links, so grow first, rebuild the
    // lists from block state, then proceed with normal unlink/split/insert.
    if (allocator->num_blocks >= allocator->max_blocks) {
        size_t new_max = allocator->max_blocks * 2;
        buddy_block_t* new_blocks = (buddy_block_t*)realloc(allocator->all_blocks,
                                                            new_max * sizeof(buddy_block_t));
        if (!new_blocks) {
            return NULL;  // Block is still linked and marked free; no corruption
        }
        allocator->all_blocks = new_blocks;
        allocator->max_blocks = new_max;
        buddy_rebuild_free_lists(allocator);
        block = &allocator->all_blocks[block_index];
    }

    // Remove from current free list (using index to be safe)
    block = &allocator->all_blocks[block_index];
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        allocator->free_lists[block->level] = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }

    // Create buddy block
    buddy_block_t* buddy = &allocator->all_blocks[allocator->num_blocks++];

    // Split into two blocks at lower level
    uint8_t new_level = block->level - 1;
    VkDeviceSize new_size = level_to_size(new_level);

    block->level = new_level;
    block->size = new_size;

    buddy->offset = block->offset + new_size;
    buddy->size = new_size;
    buddy->level = new_level;
    buddy->is_free = true;
    buddy->next = NULL;
    buddy->prev = NULL;

    // Add both blocks to lower level free list (exactly once)
    block->next = buddy;
    block->prev = NULL;
    buddy->next = allocator->free_lists[new_level];
    buddy->prev = block;

    if (allocator->free_lists[new_level]) {
        allocator->free_lists[new_level]->prev = buddy;
    }

    allocator->free_lists[new_level] = block;

    return block;
}
```

### B1b — Bounds checks in `buddy_alloc`

**Current code, `src/runtime/lgx_gpu_pool.c:431-446`:**

```c
static buddy_block_t* buddy_alloc(buddy_allocator_t* allocator, VkDeviceSize size, VkDeviceSize alignment) {
    // Task 3.5.2.2: Prefetch allocator metadata (likely to be accessed)
    // Read prefetch with high temporal locality (will be accessed multiple times)
    __builtin_prefetch(&allocator->free_lists[0], 0, 3);
    
    // Adjust size for alignment
    if (size < alignment) {
        size = alignment;
    }
    
    uint8_t level = size_to_level(size);
    
    // Adjust level for alignment requirements
    while (level_to_size(level) < alignment) {
        level++;
    }
```

**Insert immediately after the first `__builtin_prefetch`:**

```c
    // Reject requests larger than the largest representable block.
    // size_to_level() clamps silently; without this check a request larger
    // than MAX_BLOCK_SIZE would get a smaller block back.
    if (size > MAX_BLOCK_SIZE || alignment > MAX_BLOCK_SIZE) {
        return NULL;
    }
```

Leave the rest of `buddy_alloc` unchanged; with the guard in place the `while` loop cannot push `level` past `NUM_BUDDY_LEVELS - 1`.

### B1c — Index-based handles

**Current code, `src/runtime/lgx_gpu_pool.c:874-884`:**

```c
/**
 * GPU allocation handle
 */
struct lgx_gpu_allocation {
    lgx_gpu_memory_type_t memory_type;
    buddy_block_t* block;
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
    void* mapped_ptr;
};
```

**Replacement:**

```c
/**
 * GPU allocation handle
 *
 * The block is referenced by index, not pointer: allocator->all_blocks is
 * realloc'd by buddy_split, which invalidates any stored pointer.
 */
struct lgx_gpu_allocation {
    lgx_gpu_memory_type_t memory_type;
    size_t block_index;
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
    void* mapped_ptr;
};
```

**In `lgx_gpu_alloc`, current `:934-938`:**

```c
    alloc->memory_type = type;
    alloc->block = block;
    alloc->memory = allocator->memory;
    alloc->offset = block->offset;
    alloc->size = block->size;
```

**Replacement:**

```c
    alloc->memory_type = type;
    alloc->block_index = (size_t)(block - allocator->all_blocks);
    alloc->memory = allocator->memory;
    alloc->offset = block->offset;
    alloc->size = block->size;
    alloc->mapped_ptr = NULL;
```

**In `lgx_gpu_free`, current `:969-985`:**

```c
    pthread_mutex_lock(&g_gpu_pool.mutex);
    
    buddy_allocator_t* allocator = &g_gpu_pool.allocators[type];
    
    // Update statistics
    g_gpu_pool.used_budget[type] -= alloc->block->size;
    g_gpu_pool.deallocation_count++;
    
    // Free buddy block
    buddy_free(allocator, alloc->block);
    
    // Update fragmentation
    buddy_update_fragmentation(allocator);
    
    pthread_mutex_unlock(&g_gpu_pool.mutex);
    
    free(alloc);
```

**Replacement:**

```c
    pthread_mutex_lock(&g_gpu_pool.mutex);
    
    buddy_allocator_t* allocator = &g_gpu_pool.allocators[type];
    
    // Update statistics (alloc->size is fixed at allocation time)
    g_gpu_pool.used_budget[type] -= alloc->size;
    g_gpu_pool.deallocation_count++;
    
    // Free buddy block (index survives realloc; pointer does not)
    if (alloc->block_index < allocator->num_blocks) {
        buddy_block_t* block = &allocator->all_blocks[alloc->block_index];
        buddy_free(allocator, block);
        
        // Update fragmentation
        buddy_update_fragmentation(allocator);
    }
    
    pthread_mutex_unlock(&g_gpu_pool.mutex);
    
    free(alloc);
```

### B1d — Test accessors (the handle struct is private to `lgx_gpu_pool.c`)

Tests cannot dereference `lgx_gpu_allocation_t` — the struct is file-local. Add two accessors next to `lgx_gpu_get_memory` (`:988-993`):

**In `src/runtime/lgx_gpu_pool.c`, after `lgx_gpu_get_memory`:**

```c
/**
 * Get the byte offset of a GPU allocation within its VkDeviceMemory
 */
VkDeviceSize lgx_gpu_get_offset(const lgx_gpu_allocation_t* alloc) {
    return alloc ? alloc->offset : 0;
}

/**
 * Get the CPU-mapped pointer of a GPU allocation (NULL if not host-visible)
 */
void* lgx_gpu_get_mapped_ptr(lgx_gpu_allocation_t* alloc) {
    return alloc ? alloc->mapped_ptr : NULL;
}
```

**In `include/lgx/lgx_runtime_internal.h`, next to the other `lgx_gpu_*` declarations (`:279-294` region):**

```c
VkDeviceSize lgx_gpu_get_offset(const lgx_gpu_allocation_t* alloc);
void* lgx_gpu_get_mapped_ptr(lgx_gpu_allocation_t* alloc);
```

Note the `const` mismatch to fix in the implementation as written (`lgx_gpu_get_mapped_ptr` takes non-const), and match the declaration style of the surrounding GPU prototypes (some are inside a `#if HAVE_VULKAN` block — put these inside it too if that is where `lgx_gpu_get_memory` is declared).

### B1e — Regression test

**File:** `tests/phase0/test_gpu_buddy_allocator.c` (extend; read it first — it already has a `create_test_vulkan_instance`-style helper around `:32-50` and skips with exit 0 when Vulkan is unavailable; follow that convention exactly).

**Add two static tests and call them from `main` before shutdown:**

```c
/* Forces at least one all_blocks realloc (initial capacity is 1024 blocks)
   and frees every handle afterwards. Pre-fix this either corrupts the free
   lists (double insert) or dereferences freed memory (stale block pointer). */
static void test_handle_stability_across_realloc(void) {
    printf("\nTest: handle stability across all_blocks realloc\n");

    enum { COUNT = 2000 };
    lgx_gpu_allocation_t* handles[COUNT];
    int allocated = 0;

    for (int i = 0; i < COUNT; i++) {
        handles[i] = lgx_gpu_alloc(4096, 256, LGX_GPU_HOST_VISIBLE);
        if (!handles[i]) break;
        allocated++;
    }
    if (allocated < 1100) {
        printf("  SKIP: only %d allocations possible (budget too small)\n", allocated);
        for (int i = 0; i < allocated; i++) lgx_gpu_free(handles[i]);
        return;
    }

    /* All returned offsets must be distinct: duplicates mean the free
       lists handed out the same block twice. lgx_gpu_get_offset is the
       B1d accessor; the handle struct itself is private. */
    for (int i = 0; i < allocated; i++) {
        for (int j = i + 1; j < allocated; j++) {
            if (lgx_gpu_get_offset(handles[i]) == lgx_gpu_get_offset(handles[j])) {
                printf("  FAIL: duplicate block offset %llu\n",
                       (unsigned long long)lgx_gpu_get_offset(handles[i]));
                for (int k = 0; k < allocated; k++) lgx_gpu_free(handles[k]);
                exit(1);
            }
        }
    }

    /* Mapped pointers must be writable (host-visible). */
    void* mapped = lgx_gpu_get_mapped_ptr(handles[0]);
    if (mapped) {
        memset(mapped, 0xAB, 4096);
    }

    for (int i = 0; i < allocated; i++) lgx_gpu_free(handles[i]);

    /* Pool must still allocate after the churn. */
    lgx_gpu_allocation_t* after = lgx_gpu_alloc(4096, 256, LGX_GPU_HOST_VISIBLE);
    if (!after) { printf("  FAIL: alloc after churn returned NULL\n"); exit(1); }
    lgx_gpu_free(after);

    printf("  PASS: %d allocations, no duplicates, all freed\n", allocated);
}
```

The O(n²) duplicate scan is 2M comparisons of an 8-byte field — acceptable in a test. If the harness style in the file forbids `exit`, use the file's existing FAIL/PASS macros and counters instead; message text is what matters.

```c
/* Requests that exceed what the buddy tree can represent must be rejected,
   not silently served by a smaller block. */
static void test_size_and_alignment_bounds(void) {
    printf("\nTest: size and alignment bounds\n");

    /* 128 MB > MAX_BLOCK_SIZE (64 MB): must return NULL, not a 64 MB block. */
    lgx_gpu_allocation_t* too_big = lgx_gpu_alloc(128ull * 1024 * 1024, 256,
                                                  LGX_GPU_DEVICE_LOCAL);
    if (too_big) {
        printf("  FAIL: oversized request was served\n");
        lgx_gpu_free(too_big);
        exit(1);
    }

    /* Alignment > MAX_BLOCK_SIZE must return NULL, not index free_lists[19]. */
    lgx_gpu_allocation_t* bad_align = lgx_gpu_alloc(1024, 128ull * 1024 * 1024,
                                                    LGX_GPU_HOST_VISIBLE);
    if (bad_align) {
        printf("  FAIL: oversized alignment was served\n");
        lgx_gpu_free(bad_align);
        exit(1);
    }

    printf("  PASS: out-of-range requests rejected\n");
}
```

**Verification (order matters):**
1. ASan/UBSan build of the test (the CI `asan-ubsan` configuration, or locally `cmake -S . -B /tmp/opencode/lgx-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"`). Run the test **before** applying B1a-d (stash or worktree off `bcd8b52` + A1): it must fail — ASan heap-use-after-free in `lgx_gpu_free`, or the duplicate-offset check, or the bounds checks. Record the pre-fix failure output in the commit message. This machine has a real Vulkan device (lavapipe), so the test runs, not skips. CI has no device: the skip path exits 0, which is the project's existing convention.
2. Apply B1a-d, rebuild ASan: test passes, no ASan reports, no leaks (LSan output clean; the `lsan.supp` suppression file is passed by ctest).

**Commit message:**
```
fix(gpu): buddy allocator memory safety (4 defects)

1. Handles stored buddy_block_t*, invalidated by all_blocks realloc in
   buddy_split: lgx_gpu_free dereferenced freed memory. Handles now store
   a block index; free looks the block up under the pool mutex.
2. buddy_split grew all_blocks after unlinking the block, then
   rebuilt the free lists and inserted block+buddy a second time:
   cyclic/duplicate lists. Growth now happens before unlinking, with a
   single rebuild, and blocks are inserted exactly once.
3. A failed realloc orphaned the already-unlinked block. Check first,
   unlink after.
4. buddy_alloc indexed free_lists[19] for alignment > 64MB and served
   too-small blocks for size > 64MB (silent size_to_level clamp).
   Both now return NULL.

Verified: requires ASan; pre-fix the new tests fail under
-fsanitize=address,undefined with heap-use-after-free / duplicate block
offsets. Post-fix: clean run. CI skips without a device, per existing
convention.
```

---

## Task B2 — GPU pool init order and failure cleanup

**Defects:** in `lgx_gpu_pool_init` (`src/runtime/lgx_gpu_pool.c:695-797`):
1. The buddy allocator is initialised *after* the Vulkan allocation (`:754`) and mapping (`:764`) write `allocator->memory` / `allocator->mapped_ptr`. `buddy_init` memsets the allocator struct (`:162` region), wiping both. Result: `lgx_gpu_get_memory()` always returns `VK_NULL_HANDLE`; `lgx_gpu_alloc` never sets `mapped_ptr` (`:941-944` sees NULL). `lgx_gpu_pool_shutdown` then skips `vkFreeMemory` (`:820`) and leaks the Vulkan allocation.
2. On in-loop failure (`:757`, `:777`), `lgx_gpu_pool_shutdown()` is a no-op because `initialized` is still false (`:803`), and the code then calls `pthread_mutex_destroy` a second time after shutdown would have done it — currently masked only because shutdown returned early. Any already-allocated Vulkan memory leaks.

**Current code, `src/runtime/lgx_gpu_pool.c:738-783`:**

```c
    // Allocate Vulkan memory and initialize buddy allocators
    for (int i = 0; i < LGX_GPU_MEMORY_TYPE_COUNT; i++) {
        if (!g_gpu_pool.memory_type_available[i]) {
            continue;
        }
        
        buddy_allocator_t* allocator = &g_gpu_pool.allocators[i];
        VkDeviceSize alloc_size = g_gpu_pool.total_budget[i];
        
        // Allocate Vulkan memory
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = alloc_size,
            .memoryTypeIndex = g_gpu_pool.memory_type_indices[i]
        };
        
        VkResult vk_result = vkAllocateMemory(g_gpu_pool.device, &alloc_info, NULL, &allocator->memory);
        if (vk_result != VK_SUCCESS) {
            fprintf(stderr, "[LGX ERROR] Failed to allocate Vulkan memory for type %d: %d\n", i, vk_result);
            lgx_gpu_pool_shutdown();
            pthread_mutex_destroy(&g_gpu_pool.mutex);
            return LGX_ERROR_OUT_OF_MEMORY;
        }
        
        // Map memory if host-visible
        if (i == LGX_GPU_HOST_VISIBLE || i == LGX_GPU_HOST_CACHED) {
            vk_result = vkMapMemory(g_gpu_pool.device, allocator->memory, 0, alloc_size, 0, &allocator->mapped_ptr);
            if (vk_result != VK_SUCCESS) {
                fprintf(stderr, "[LGX WARNING] Failed to map host-visible memory for type %d: %d\n", i, vk_result);
                allocator->mapped_ptr = NULL;
            }
        } else {
            allocator->mapped_ptr = NULL;
        }
        
        // Initialize buddy allocator
        result = buddy_init(allocator, alloc_size);
        if (result != LGX_SUCCESS) {
            fprintf(stderr, "[LGX ERROR] Failed to initialize buddy allocator for type %d\n", i);
            lgx_gpu_pool_shutdown();
            pthread_mutex_destroy(&g_gpu_pool.mutex);
            return result;
        }
    }
    
    g_gpu_pool.initialized = true;
    
    printf("[LGX INFO] GPU Memory Pool initialized\n");
```

**Exact replacement:**

```c
    // From here on, all failures are cleaned up by lgx_gpu_pool_shutdown(),
    // which frees partial state, destroys the mutex, and clears this flag.
    g_gpu_pool.initialized = true;
    
    // Allocate Vulkan memory and initialize buddy allocators
    for (int i = 0; i < LGX_GPU_MEMORY_TYPE_COUNT; i++) {
        if (!g_gpu_pool.memory_type_available[i]) {
            continue;
        }
        
        buddy_allocator_t* allocator = &g_gpu_pool.allocators[i];
        VkDeviceSize alloc_size = g_gpu_pool.total_budget[i];
        
        // Initialize buddy allocator FIRST: buddy_init() memsets the
        // allocator struct, which would wipe memory/mapped_ptr if the
        // Vulkan calls below ran first.
        result = buddy_init(allocator, alloc_size);
        if (result != LGX_SUCCESS) {
            fprintf(stderr, "[LGX ERROR] Failed to initialize buddy allocator for type %d\n", i);
            lgx_gpu_pool_shutdown();
            return result;
        }
        allocator->memory = VK_NULL_HANDLE;
        allocator->mapped_ptr = NULL;
        
        // Allocate Vulkan memory
        VkMemoryAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = alloc_size,
            .memoryTypeIndex = g_gpu_pool.memory_type_indices[i]
        };
        
        VkResult vk_result = vkAllocateMemory(g_gpu_pool.device, &alloc_info, NULL, &allocator->memory);
        if (vk_result != VK_SUCCESS) {
            fprintf(stderr, "[LGX ERROR] Failed to allocate Vulkan memory for type %d: %d\n", i, vk_result);
            allocator->memory = VK_NULL_HANDLE;
            lgx_gpu_pool_shutdown();
            return LGX_ERROR_OUT_OF_MEMORY;
        }
        
        // Map memory if host-visible
        if (i == LGX_GPU_HOST_VISIBLE || i == LGX_GPU_HOST_CACHED) {
            vk_result = vkMapMemory(g_gpu_pool.device, allocator->memory, 0, alloc_size, 0, &allocator->mapped_ptr);
            if (vk_result != VK_SUCCESS) {
                fprintf(stderr, "[LGX WARNING] Failed to map host-visible memory for type %d: %d\n", i, vk_result);
                allocator->mapped_ptr = NULL;
            }
        } else {
            allocator->mapped_ptr = NULL;
        }
    }
    
    printf("[LGX INFO] GPU Memory Pool initialized\n");
```

Notes:
- The earlier failure paths (`detect_memory_types` `:717`, `query_memory_budget` `:724`) stay as-is: they run before `initialized = true`, allocate no Vulkan memory, and correctly destroy the mutex manually.
- `lgx_gpu_pool_shutdown` (`:802-835`) is safe on partially-initialised state: it guards `memory_type_available[i]`, checks `mapped_ptr`/`memory != VK_NULL_HANDLE`, and `buddy_shutdown` tolerates zeroed allocators (verify by reading `buddy_shutdown` before editing).
- Do **not** add a manual `pthread_mutex_destroy` to the in-loop failure paths; shutdown owns it now.

**Test — extend `tests/phase0/test_gpu_pool.c`:**

After the existing `lgx_gpu_pool_init(...)` succeeds and before shutdown:

```c
    /* The pool must hand back usable handles: a real VkDeviceMemory and a
       mapped pointer for host-visible memory. Pre-fix, buddy_init memsets
       the allocator after vkAllocateMemory/vkMapMemory, so get_memory
       returns VK_NULL_HANDLE and mapped_ptr is always NULL. */
    lgx_gpu_allocation_t* h = lgx_gpu_alloc(4096, 256, LGX_GPU_HOST_VISIBLE);
    TEST_ASSERT(h != NULL, "host-visible allocation succeeds");
    if (h) {
        TEST_ASSERT(lgx_gpu_get_memory(h) != VK_NULL_HANDLE,
                    "handle carries a real VkDeviceMemory");
        void* mapped = lgx_gpu_get_mapped_ptr(h);   /* B1d accessor */
        TEST_ASSERT(mapped != NULL, "host-visible allocation is mapped");
        if (mapped) {
            memset(mapped, 0x5A, 4096);
            TEST_ASSERT(((uint8_t*)mapped)[0] == 0x5A, "mapped memory is writable");
        }
        lgx_gpu_free(h);
    }
```

Adapt macro names to the file's existing harness (read it first). Place this block after `lgx_gpu_pool_init` succeeds and before `lgx_gpu_pool_shutdown`.

**Verification:**
1. Pre-fix run under the normal build: `test_gpu_pool` fails the `VK_NULL_HANDLE` assertion.
2. Apply fix, rerun: passes.
3. ASan + LSan on `test_gpu_pool` init/shutdown cycle: no leaks (catches the vkAllocateMemory leak path indirectly via budget accounting only on this host; note in commit that the failure-branch cleanup is verified by inspection + successful normal path).
4. Full `ctest -R test_gpu` on this machine.

**Commit message:**
```
fix(gpu): initialise buddy allocator before Vulkan memory; own cleanup

buddy_init() memsets the allocator struct, wiping memory/mapped_ptr set
by vkAllocateMemory/vkMapMemory one statement earlier. Consequences:
lgx_gpu_get_memory() always returned VK_NULL_HANDLE, host-visible
allocations never got a mapped pointer, and shutdown therefore never
freed the Vulkan memory.

Init order is now buddy_init -> vkAllocateMemory -> vkMapMemory, with
explicit VK_NULL_HANDLE after the memset. In-loop failure paths now set
initialized=true before the loop and delegate to lgx_gpu_pool_shutdown()
instead of calling it while it is a no-op and then double-destroying the
mutex.

Verified: test_gpu_pool asserts a real VkDeviceMemory and mapped writes;
pre-fix the memory-handle assertion fails.
```

---

## Task B4 — Chaos testing self-deadlock

**Defect:** `chaos_random()` (`src/runtime/lgx_chaos_testing.c:63-68`) locks `g_chaos_state.mutex`, but every caller already holds that non-recursive mutex (the decision functions that call it run between lock/unlock in the same file). Enabling chaos testing succeeds (it never calls the RNG); the first injected decision deadlocks forever.

**Current code, `src/runtime/lgx_chaos_testing.c:60-68`:**

```c
/**
 * Thread-safe random number generator
 */
static double chaos_random(void) {
    pthread_mutex_lock(&g_chaos_state.mutex);
    int r = rand_r(&g_chaos_state.random_state);
    pthread_mutex_unlock(&g_chaos_state.mutex);
    return (double)r / (double)RAND_MAX;
}
```

**Exact replacement:**

```c
/**
 * Random number generator.
 *
 * Caller MUST hold g_chaos_state.mutex: every call site is already inside
 * the lock (it needs the config it just read under the same lock), and the
 * mutex is not recursive -- locking here deadlocks on the first decision.
 */
static double chaos_random(void) {
    int r = rand_r(&g_chaos_state.random_state);
    return (double)r / (double)RAND_MAX;
}
```

**Before committing, verify every call site holds the lock:**

```bash
grep -n "chaos_random()" src/runtime/lgx_chaos_testing.c
```

For each line number, confirm it is lexically between a `pthread_mutex_lock(&g_chaos_state.mutex)` and the matching unlock. If any site does not hold the lock, add the lock there instead of removing it from the helper — do not guess; the grep is cheap.

**Also update `scripts/run_chaos_tests.sh`** (it invokes five binaries that are not built or registered): replace its body with a ctest invocation of the new test, and keep exit-code passthrough:

```bash
#!/usr/bin/env bash
# Chaos testing regression gate. The deadlock this script exists for is
# covered by test_chaos_deadlock (explicit target, 20s timeout): pre-fix
# the test hangs and ctest kills it with a non-zero result.
set -euo pipefail
BUILD_DIR="${1:-build}"
ctest --test-dir "$BUILD_DIR" -R '^test_chaos_deadlock$' --output-on-failure
```

**New test — `tests/phase0/test_chaos_deadlock.c`:**

```c
/*
 * Pre-fix: lgx_runtime_enable_chaos_testing succeeds, then the first
 * decision call (anything that reads a rate and calls chaos_random)
 * deadlocks on the non-recursive mutex. ctest's 20s timeout turns the
 * hang into a failure.
 */
#include "lgx_runtime.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    lgx_config_destroy(config);

    lgx_chaos_config_t chaos = {
        .struct_size = sizeof(lgx_chaos_config_t),
        .failure_rate = 1.0f,
        .latency_spike_rate = 1.0f,
        .max_latency_ms = 0,
        .random_seed = 42,
    };
    assert(lgx_runtime_enable_chaos_testing(&chaos) == LGX_SUCCESS);

    /* First decision: deadlocks pre-fix. failure_rate=1.0 makes the
       outcome deterministic. */
    bool fail = lgx_chaos_should_fail_allocation();
    assert(fail == true);

    /* Second decision exercises the latency path's RNG call too. */
    (void)lgx_chaos_should_inject_latency();

    assert(lgx_runtime_disable_chaos_testing() == LGX_SUCCESS);
    lgx_runtime_shutdown();

    printf("test_chaos_deadlock: OK\n");
    return 0;
}
```

Adjust function names/types to the actual declarations in `lgx_runtime.h` (read the chaos section; `lgx_chaos_config_t` field names and the `should_*` names must match the header, not this sketch).

**Register as an explicit target in `tests/phase0/CMakeLists.txt`** so it is excluded from the generic glob (the glob uses a fixed 300s timeout; a deadlock would eat 5 minutes and report as a generic timeout). Add the test name to `PHASE0_TESTS_EXCLUDED` in the root `CMakeLists.txt` (currently only `test_csf2_numa_awareness`, `:332`), and in `tests/phase0/CMakeLists.txt` add, following the NUMA pattern at the top of that file:

```cmake
add_executable(test_chaos_deadlock test_chaos_deadlock.c)
target_link_libraries(test_chaos_deadlock lgx_runtime m)
add_test(NAME test_chaos_deadlock COMMAND test_chaos_deadlock)
set_tests_properties(test_chaos_deadlock PROPERTIES
    LABELS "phase0"
    TIMEOUT 20
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)
```

The short timeout is the point: a deadlock fails the suite in 20 seconds.

**Verification (order matters):**
1. On a scratch worktree at A1+A2 (no B4 fix), build and run `ctest -R test_chaos_deadlock` — it must fail by timeout. Record ctest's "Timeout" output.
2. Apply B4, rerun: passes in well under a second.

**Commit message:**
```
fix(chaos): draw randomness under the caller's lock, not twice

chaos_random() locked the non-recursive g_chaos_state.mutex that every
caller already holds; the first chaos decision deadlocked. The helper is
now lock-free by contract (documented on the function), and call sites
were verified to hold the lock. scripts/run_chaos_tests.sh now runs the
one test that gates this instead of five binaries that were never built.

Verified: test_chaos_deadlock (20s ctest timeout) fails by timeout
pre-fix; passes sub-second post-fix.
```

---

## Task B5 — GPU-shared hint must be routed before the frame fast path

**Defect:** `lgx_alloc_with_intent` (`src/runtime/lgx_intent_allocator.c:264-299`) checks `lifetime == LGX_LIFETIME_FRAME` first. `lgx_alloc_gpu_shared()` (public header) uses `lifetime = LGX_LIFETIME_FRAME` **and** `hint = LGX_HINT_GPU_SHARED`, so it is silently served from the frame arena; the GPU branch at `:275` is unreachable for the public API.

**Current code, `src/runtime/lgx_intent_allocator.c:262-281`:**

```c
    void* ptr = NULL;
    
    // Route based on intent with branch hints (Task 12.1.3)
    // Priority: lifetime > performance hint > access pattern
    // Most common path first (FRAME = 80% of allocations)
    
    if (likely(intent->lifetime == LGX_LIFETIME_FRAME)) {
        // Frame-scoped allocation → Frame arena (MOST COMMON PATH)
        ptr = lgx_frame_alloc_internal(intent->size, "unknown", 0);
        
        // Thread-local statistics (no mutex) (Task 12.1.2)
        tls_intent_stats.frame_allocations++;
        
    } else if (unlikely(intent->hint == LGX_HINT_GPU_SHARED)) {
        // GPU-shared allocation → GPU pool (15% of allocations)
        // Default to host-visible memory for CPU-GPU sharing
        ptr = lgx_gpu_alloc(intent->size, 16, LGX_GPU_HOST_VISIBLE);
        
        tls_intent_stats.gpu_allocations++;
        
    } else if (intent->lifetime == LGX_LIFETIME_LEVEL || 
```

**Exact replacement of the routing head (keep the trailing branches and everything after `:299` unchanged):**

```c
    void* ptr = NULL;
    
    // Route based on intent with branch hints (Task 12.1.3)
    // Priority: explicit performance hint > lifetime > access pattern.
    // The GPU hint must be tested before the frame-lifetime fast path:
    // lgx_alloc_gpu_shared() sets lifetime=FRAME AND hint=GPU_SHARED, so
    // a lifetime-first check swallows the whole public GPU API.
    // Bare lgx_alloc_frame() (hint=CRITICAL_PATH) still takes the fast path.
    
#if HAVE_VULKAN
    if (unlikely(intent->hint == LGX_HINT_GPU_SHARED)) {
        // GPU-shared allocation → GPU pool
        // Default to host-visible memory for CPU-GPU sharing
        ptr = lgx_gpu_alloc(intent->size, 16, LGX_GPU_HOST_VISIBLE);
        
        tls_intent_stats.gpu_allocations++;
        
    } else
#endif
    if (likely(intent->lifetime == LGX_LIFETIME_FRAME)) {
        // Frame-scoped allocation → Frame arena (MOST COMMON PATH)
        ptr = lgx_frame_alloc_internal(intent->size, "unknown", 0);
        
        // Thread-local statistics (no mutex) (Task 12.1.2)
        tls_intent_stats.frame_allocations++;
        
    } else if (intent->lifetime == LGX_LIFETIME_LEVEL || 
```

The `#if HAVE_VULKAN` around the hint branch is deliberate — it is B6's link fix (the only unguarded `lgx_gpu_alloc` reference in the file). With `HAVE_VULKAN=0` a GPU-shared intent falls through to the frame/heap branches, matching the documented fallback in `tests/phase0/test_intent_allocator.c:124-134`.

**Test — extend `tests/phase0/test_intent_allocator.c`** (new function + one `TEST(...)` line in `main`):

```c
/**
 * Test: GPU hint precedence over frame lifetime
 *
 * lgx_alloc_gpu_shared() = lifetime FRAME + hint GPU_SHARED. The routing
 * must key on the hint. Counters prove the branch taken; no device needed,
 * because the GPU counter increments regardless of allocation success.
 */
void test_gpu_hint_precedence() {
    lgx_intent_aggregate_stats();
    lgx_intent_stats_t before;
    ASSERT(lgx_intent_get_stats(&before) == LGX_SUCCESS, "baseline stats read");

    void* ptr = lgx_alloc_gpu_shared(512);

    lgx_intent_aggregate_stats();
    lgx_intent_stats_t after;
    ASSERT(lgx_intent_get_stats(&after) == LGX_SUCCESS, "post stats read");

#if HAVE_VULKAN
    ASSERT(after.gpu_allocations == before.gpu_allocations + 1,
           "GPU hint routed to the GPU pool");
    ASSERT(after.frame_allocations == before.frame_allocations,
           "GPU hint did not consume frame arena");
    if (ptr) {
        lgx_gpu_free((lgx_gpu_allocation_t*)ptr);
    }
#else
    /* No Vulkan: must fall back to the persistent heap, never NULL-crash
       and never the frame arena. */
    ASSERT(ptr != NULL, "GPU hint falls back to heap without Vulkan");
    ASSERT(after.persistent_allocations == before.persistent_allocations + 1,
           "fallback counted as persistent");
    lgx_heap_free(ptr);
#endif
}
```

Check the exact stats field names in `include/lgx_runtime.h:290-298` before writing; adjust to match.

**Verification:**
1. Pre-fix (scratch worktree at A1+A2): `ctest -R test_intent_allocator` — the new test fails: `gpu_allocations` unchanged, `frame_allocations` incremented.
2. Post-fix: passes. On this machine (lavapipe) `HAVE_VULKAN=1` path runs; the no-vulkan path is exercised by B6's CI job.
3. Full `ctest -R test_intent` green.

**Commit message:**
```
fix(intent): route GPU_SHARED hint before the frame-lifetime fast path

lgx_alloc_gpu_shared() sets lifetime=FRAME + hint=GPU_SHARED, but the
router tested lifetime first, so the public GPU allocation API was
served from the frame arena and the GPU branch was dead code.

Hint precedence is now explicit, and the GPU branch is compiled under
#if HAVE_VULKAN (B6: removes the last unguarded lgx_gpu_alloc reference
in this file). Without Vulkan, GPU-shared intents fall back to the
persistent heap, as the existing no-vulkan test path already expects.

Verified: new test_gpu_hint_precedence fails pre-fix (frame counter moves,
GPU counter does not), passes post-fix.
```

---

## Task B6 — Non-Vulkan build links; the no-vulkan CI job becomes a real gate

**Defect:** with `HAVE_VULKAN=0`, `src/runtime/lgx_intent_allocator.c:278` references `lgx_gpu_alloc`, which is not compiled, and the four GPU phase0 test sources include `<vulkan/vulkan.h>` unguarded; `tests/unit/test_init_shutdown.c:195` references `lgx_gpu_pool_is_initialized` unconditionally. The no-vulkan CI job does not even run ctest: it builds, is allowed to fail (`continue-on-error: true`), and a report step exits 0 with instructions to delete the workaround "when Plan 01 guards line 278".

B5 already guards `:278` (same commit family). This task finishes the job.

**Part 1 — exclude GPU phase0 test sources when Vulkan is absent.**

In the root `CMakeLists.txt`, immediately after the `foreach` loop that globs `tests/phase0/*.c` (`:334-354`) — or, cleaner, inside the loop next to the existing Vulkan link block at `:343-346` — skip registration when the file is a GPU test and Vulkan is missing. Exact change inside the loop:

```cmake
    # GPU tests include <vulkan/vulkan.h> unguarded; without Vulkan they
    # cannot compile (B6). test_gpu_perf_simple does not include Vulkan and
    # stays. Names match the glob's existing "gpu" link rule below.
    if(NOT Vulkan_FOUND AND test_name MATCHES "^test_gpu_(buddy_allocator|capability|performance|pool)$")
        continue()
    endif()
```

Place it before `add_executable`. Note `test_gpu_detection` and `test_gpu_perf_simple` are *not* in the skip list: verify by grepping `#include <vulkan` in `tests/phase0/test_gpu_*.c` and adjust the list to exactly the files that include the Vulkan header.

**Part 2 — guard the unit test reference.**

`tests/unit/test_init_shutdown.c:193-196`:

```c
    // Check GPU pool if available
    if (lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
        TEST_ASSERT(lgx_gpu_pool_is_initialized(), "GPU pool initialized");
    }
```

Wrap the body:

```c
#if defined(HAVE_VULKAN) && HAVE_VULKAN
    // Check GPU pool if available
    if (lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
        TEST_ASSERT(lgx_gpu_pool_is_initialized(), "GPU pool initialized");
    }
#endif
```

`HAVE_VULKAN` is defined for tests via the directory-scope `add_definitions` in the root `CMakeLists.txt` (verify: `grep -n HAVE_VULKAN CMakeLists.txt`, it is defined both 0 and 1 states; if the unit tests don't inherit it, add the guard using the CMake variable at configure time instead: `target_compile_definitions(test_init_shutdown PRIVATE HAVE_VULKAN=$<BOOL:${Vulkan_FOUND}>)`).

Do the same sweep for any other unguarded `lgx_gpu_` references in `tests/unit/` and `tests/integration/`:

```bash
grep -rn "lgx_gpu_" tests/unit tests/integration | grep -v HAVE_VULKAN
```

Fix each with the same guard or a skip.

**Part 3 — make the no-vulkan CI job a real gate.**

Read `.github/workflows/ci.yml`, the `no-vulkan` job (currently contains `continue-on-error: true`, a build that stops before tests, and a report step that exits 0). Replace it with a working job:

```yaml
  no-vulkan:
    name: No Vulkan (build + tests)
    runs-on: ubuntu-22.04
    timeout-minutes: 45
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake pkg-config libnuma-dev

      - name: Configure without Vulkan
        run: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

      - name: Assert Vulkan was NOT found
        run: |
          grep -q "HAVE_VULKAN=0" build/CMakeFiles/lgx_runtime.dir/flags.make

      - name: Build
        run: cmake --build build -j$(nproc)

      - name: Test
        run: ctest --test-dir build --output-on-failure -E 'test_gpu'
```

Notes:
- The `-E 'test_gpu'` excludes GPU tests; with Part 1 they should not be registered at all when Vulkan is absent, so this is belt-and-braces — keep it only if `ctest -N` shows them absent; otherwise drop it.
- Remove `continue-on-error: true` and the "report" step entirely in the same commit. Update the header comment block at the top of the file (`:23` "deliberately RED") to reflect reality.
- `libvulkan-dev` is deliberately absent here; do not add it.

**Part 4 — Machine guards in CI (not pre-commit).** Local hooks are skippable; the repo already demonstrates that CI gates work (`scripts/count_tests.sh` is blocking in `build-and-test`). Add two steps to the `build-and-test` job in `.github/workflows/ci.yml`, after Checkout:

```yaml
      - name: Secret scan (gitleaks)
        uses: gitleaks/gitleaks-action@v2
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}

      # Mechanical replacement for the rules in AGENTS.md: how the junk in
      # bcd8b52 got in was `git add -A`; these checks make that class of
      # commit fail CI regardless of who (or what) makes it.
      - name: Reject tracked binaries and oversized files
        run: |
          fail=0
          while IFS= read -r f; do
            [ -f "$f" ] || continue
            if [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' \n')" = "7f454c46" ]; then
              echo "ELF binary tracked: $f"; fail=1
            fi
            size=$(stat -c%s "$f")
            if [ "$size" -gt 5242880 ]; then
              echo "File larger than 5 MB tracked: $f ($size bytes)"; fail=1
            fi
          done < <(git ls-files)
          exit $fail
```

Notes: `gitleaks-action` needs no license key on public repositories; on a private repo the owner must set `GITLEAKS_LICENSE` (human-only action — add it to E1's handoff list). The size limit (5 MB) and the ELF check are deliberately over-broad — this repo has no legitimate tracked binaries or large blobs; if it ever does, whitelist explicitly in the step rather than raising the limits.

**Verification (local, without touching system packages):**
1. `cmake -S . -B /tmp/opencode/lgx-novulkan -DCMAKE_BUILD_TYPE=Release` with Vulkan headers hidden. The straightforward way on this machine: `cmake -S . -B /tmp/opencode/lgx-novulkan -DCMAKE_BUILD_TYPE=Release -DVulkan_FOUND=FALSE` — check whether the project's `find_package` respects a pre-set `Vulkan_FOUND`; if not, use a wrapper toolchain that sets `CMAKE_DISABLE_FIND_PACKAGE_Vulkan=ON`:
   `cmake -S . -B /tmp/opencode/lgx-novulkan -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=ON`
2. `grep HAVE_VULKAN /tmp/opencode/lgx-novulkan/CMakeFiles/lgx_runtime.dir/flags.make` → `-DHAVE_VULKAN=0`.
3. Build: zero errors. Then `ctest --test-dir /tmp/opencode/lgx-novulkan --output-on-failure`: all registered tests pass (GPU ones absent).
4. Re-run the normal Vulkan build + full ctest: unchanged.

**Commit message:**
```
ci(no-vulkan): link without Vulkan and run the tests as a gate

The no-vulkan job built without ctest and with continue-on-error, so
nobody learned anything from it. Now that the intent router's GPU branch
is guarded (#if HAVE_VULKAN), the build links; the four phase0 GPU test
sources (which include <vulkan/vulkan.h> unguarded) are excluded when
Vulkan is not found, and test_init_shutdown.c guards its GPU-pool
reference the same way.

Verified locally with -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=ON: build
succeeds, flags.make shows HAVE_VULKAN=0, registered tests pass.

Also adds machine guards to CI (not pre-commit hooks, which are
skippable): a gitleaks secret scan and a tracked-ELF/oversized-file
check over git ls-files. bcd8b52 got in via git add -A; this makes that
class of commit fail CI.
```

---

## Task B3 — Frame arena chained growth (no mid-frame munmap)

**Defect:** `grow_arena` (`src/runtime/lgx_frame_arena.c:293-327`) allocates a new mapping, memcpys, then `munmap`s the old base (`:320`). Every pointer the caller received earlier this frame now points into unmapped memory — writing to any of them is a segfault/UAF. The memcpy is useless because pointers cannot be rewritten.

**Fix:** never move or unmap memory mid-frame. Grow by allocating a *new chunk* and retiring the old one (keep it mapped) until the next reset of that arena. `arena->base/capacity/offset` describe the current chunk; a singly-linked `retired` list holds full chunks.

**Files:** `src/runtime/lgx_frame_arena.c` only.

### B3a — Add the chunk type and fields

After the arena struct definition (region `:116-160`, read it first — add fields just after `bool overflow_occurred;` at `:125`):

```c
    // Chained growth (Task B3): full chunks retired within this frame.
    // Never unmapped until reset — callers hold raw pointers into them.
    struct arena_chunk* retired_chunks;
    size_t retired_bytes;       // bytes used in retired chunks this frame
```

And before the struct (near `:116`):

```c
// A retired chunk of a frame arena's chained growth.
typedef struct arena_chunk {
    uint8_t* base;
    size_t capacity;
    struct arena_chunk* next;
} arena_chunk_t;
```

### B3b — Helper to free retired chunks; replacement for `grow_arena`

Add after `free_huge_page_arena` (`:277-281`):

```c
/**
 * Free all retired chunks of an arena (called at reset and shutdown).
 */
static void free_retired_chunks(lgx_frame_arena_t* arena) {
    arena_chunk_t* node = arena->retired_chunks;
    while (node) {
        arena_chunk_t* next = node->next;
        free_huge_page_arena(node->base, node->capacity);
        free(node);
        node = next;
    }
    arena->retired_chunks = NULL;
    arena->retired_bytes = 0;
}
```

Replace the body of `grow_arena` (`:293-327`) — the `memcpy` and `munmap` are deleted, not moved:

```c
static bool grow_arena(lgx_frame_arena_t* arena, size_t new_capacity) {
    if (!arena || new_capacity <= arena->capacity) {
        return false;
    }
    
    // Clamp to max capacity
    if (new_capacity > arena->max_capacity) {
        new_capacity = arena->max_capacity;
    }
    
    // If we're already at max, can't grow
    if (arena->capacity >= arena->max_capacity) {
        return false;
    }
    
    // Allocate the new chunk
    uint8_t* new_base = allocate_huge_page_arena(new_capacity);
    if (!new_base) {
        return false;
    }
    
    // Retire the current chunk WITHOUT unmapping or copying: callers hold
    // raw pointers into it and they must stay valid until this arena's
    // next reset. The retired node remembers the chunk; the malloc failure
    // path must not leak the freshly allocated mapping.
    if (arena->base) {
        arena_chunk_t* node = (arena_chunk_t*)malloc(sizeof(arena_chunk_t));
        if (!node) {
            free_huge_page_arena(new_base, new_capacity);
            return false;
        }
        node->base = arena->base;
        node->capacity = arena->capacity;
        node->next = arena->retired_chunks;
        arena->retired_chunks = node;
        arena->retired_bytes += arena->offset;  // bytes actually used
    }
    
    // Switch to the new chunk
    arena->base = new_base;
    arena->capacity = new_capacity;
    arena->offset = 0;
    
    return true;
}
```

### B3c — Alloc path contract change

**Current retry code, `src/runtime/lgx_frame_arena.c:556-561`:**

```c
                // Retry allocation with grown arena
                new_offset = old_offset + size;
                if (new_offset <= arena->capacity) {
                    goto allocation_succeeded;  // Jump to success path
                }
```

**Replacement:**

```c
                // Retry in the new chunk: grow_arena() retires the old
                // chunk and resets arena->offset, so this allocation starts
                // at offset 0 of the fresh chunk.
                old_offset = 0;
                new_offset = size;
                if (new_offset <= arena->capacity) {
                    goto allocation_succeeded;  // Jump to success path
                }
```

Everything else in the alloc path is unchanged: `old_offset`/`new_offset` are still locals, `ptr = arena->base + old_offset` picks the right chunk.

### B3d — Peak/usage accounting and lifecycle

1. **Peak accounting (C1's block, `:615-631`).** `new_offset` is now per-chunk. Change the peak update to count everything used this frame:

```c
    arena->allocations++;
    
    // Update peak usage (retired chunks + current chunk)
    size_t total_used = arena->retired_bytes + new_offset;
    if (total_used > arena->peak_usage) {
        arena->peak_usage = total_used;
    }
```

2. **Rolling average (`update_rolling_average`, `:335-346`).** It stores `arena->offset` into the history window; change that one line to `arena->retired_bytes + arena->offset`. (Read the function; the history write is its only arena read.)

3. **Reset (`lgx_frame_reset`, `:686-776`).** After `update_rolling_average(arena);` (`:741`) and before `arena->offset = 0;` (`:744`), insert:

```c
    // Free retired chunks: their pointers are invalid from here on,
    // which is the same contract as arena->offset = 0.
    free_retired_chunks(arena);
```

4. **Shutdown (`lgx_frame_arena_shutdown`, `:474-486`).** Inside the per-arena loop (read it; it frees `arena->base` at `:482-485`), add `free_retired_chunks(arena);` immediately before the `if (arena->base)` block.

5. **Containment debug API (used by `lgx_frame_pointer_is_arena`, `:851-857`).** Extend the check so pointers into retired chunks still report as arena pointers until reset. After the current-chunk check, loop the retired list:

```c
        for (arena_chunk_t* node = arena->retired_chunks; node; node = node->next) {
            if (ptr_addr >= node->base && ptr_addr < (node->base + node->capacity)) {
                return true;
            }
        }
```

   Adapt to the function's actual return conventions (read the whole function; it returns bool in the `:851` region and something else nearby — keep every existing path intact).

6. **Usage reporting (`:922-931` and the JSON emitter `:1112-1118`).** Where the code prints `arena->offset` as current usage, print `arena->retired_bytes + arena->offset`. Capacity lines stay per-chunk (note it: `"capacity (current chunk)"` in the debug string if the format allows). This is diagnostics; do not restructure.

### B3e — Regression test: `tests/phase0/test_frame_arena_growth.c` (new)

Free-standing phase0 test (registered automatically by the glob; no exclusion needed).

```c
/*
 * Pre-fix: growing the arena mid-frame munmaps the old mapping, so the
 * first write to a pointer obtained before the growth segfaults (ASan:
 * heap-use-after-free). Post-fix the old chunk stays mapped until reset.
 */
#include "lgx_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    /* Match the harness used by test_frame_arena_polish.c for setting a
       small initial size and a larger max — copy its setup block verbatim
       (read that file first; the field names live on the config struct). */
    config->frame_arena_size = 256 * 1024;       /* 256 KB initial */
    config->frame_arena_max_size = 8 * 1024 * 1024; /* 8 MB max */
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    lgx_config_destroy(config);

    assert(lgx_frame_reset() == LGX_SUCCESS);  /* advance to a fresh frame */

    void* before = lgx_alloc_frame(200 * 1024);
    assert(before != NULL);
    memset(before, 0x11, 200 * 1024);

    /* Forces growth: 200 KB + 200 KB > 256 KB initial capacity. */
    void* after = lgx_alloc_frame(200 * 1024);
    assert(after != NULL);
    memset(after, 0x22, 200 * 1024);

    /* Pre-fix: this write hits munmapped memory. Post-fix: contents intact. */
    assert(((unsigned char*)before)[0] == 0x11);
    assert(((unsigned char*)before)[199 * 1024 + 511] == 0x11);
    memset(before, 0x33, 1024);
    assert(((unsigned char*)before)[0] == 0x33);

    /* More growth than one doubling: force a second chunk. */
    void* third = lgx_alloc_frame(1000 * 1024);
    if (third) {
        memset(third, 0x44, 1000 * 1024);
        assert(((unsigned char*)third)[0] == 0x44);
    }

    /* Statistics must count everything used this frame. */
    frame_arena_stats_t stats;
    assert(lgx_frame_get_stats(&stats) == LGX_SUCCESS);
    assert(stats.peak_usage_bytes >= 400 * 1024);

    /* Reset frees retired chunks; the arena must be reusable. */
    assert(lgx_frame_reset() == LGX_SUCCESS);
    void* next = lgx_alloc_frame(64);
    assert(next != NULL);
    memset(next, 0x55, 64);

    lgx_runtime_shutdown();
    printf("test_frame_arena_growth: OK\n");
    return 0;
}
```

Name/field notes: `frame_arena_stats_t` and `lgx_frame_get_stats` are public (`lgx_runtime.h:35` region); verify the config field spellings against `test_frame_arena_polish.c` before writing and mirror them exactly. If the runtime requires two warmup resets before allocations land in a growable arena (triple buffering: `frame_index % FRAME_ARENA_COUNT`), add another `lgx_frame_reset()` up front — read `lgx_frame_alloc_internal`'s arena-selection line (`atomic_load(&g_frame_arena_state.current_frame)`) and mirror the polish test's reset count.

**Verification (order matters):**
1. Pre-fix (scratch worktree at A1+A2+C1): build, run under ASan — segfault or heap-use-after-free at the `before` write. Record output.
2. Apply B3, rerun: passes, LSan clean (retired chunks freed at reset/shutdown — this is what free_retired_chunks is for).
3. Full `ctest -R test_frame` green (including polish, whose peak assertions must still hold).

**Commit message:**
```
fix(arena): chain growth chunks instead of munmap'ing live memory

grow_arena() copied the arena and unmapped the old mapping mid-frame,
invalidating every pointer the frame had already handed out; the memcpy
could not help since callers hold raw pointers. Growth now allocates a
new chunk and retires the old one (kept mapped) until that arena's next
reset. Peak/rolling-average/usage accounting now counts retired bytes
plus current offset; the containment debug check sees retired chunks.

Verified: test_frame_arena_growth fails pre-fix under ASan
(heap-use-after-free on the pre-growth pointer), passes post-fix with
LSan clean; frame-arena polish tests unchanged.
```

---

## Task B7 — Net: refresh `last_recv_ms` on receive; replace `printf` with subsystem logging

**Defect:** `connection_t.last_recv_ms` is written only at `lgx_net_connect` (`src/net/lgx_net.c:212`) and read by `lgx_net_update`'s timeout check (`:242`). Data reception never refreshes it, so any connection that receives traffic but sends nothing between `timeout_ms` windows is declared dead. There is no test covering this (the only `lgx_net_update` call in tests is the NULL-param check at `tests/net/test_net_basic.c:67`). Additionally the module logs through 8 raw `printf` calls, ignoring the runtime's subsystem logging; there is no `LGX_SUBSYSTEM_NET` enum value.

### B7a — Add the subsystem enum

`include/lgx_types.h:134-142` (read the enum first): insert `LGX_SUBSYSTEM_NET` immediately before `LGX_SUBSYSTEM_COUNT` and give it an explicit comment that it is appended (never reorder existing values — ABI). Add the matching case to `lgx_subsystem_to_string` in `src/runtime/lgx_platform_services.c:401-413` (`case LGX_SUBSYSTEM_NET: return "NET";`). `test_structured_logging.c:140` iterates `i < LGX_SUBSYSTEM_COUNT`, so the new value is exercised automatically.

### B7b — Refresh on receive

**Current code, `src/net/lgx_net.c:176-190`:**

```c
    ssize_t received = recvfrom(sock->fd, buf, max_len, 0,
                                (struct sockaddr*)&sa, &sa_len);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;  /* No data */
        return -1;
    }

    if (addr) {
        addr->ip = sa.sin_addr.s_addr;
        addr->port = ntohs(sa.sin_port);
    }

    return (int)received;
}
```

**Replacement:**

```c
    ssize_t received = recvfrom(sock->fd, buf, max_len, 0,
                                (struct sockaddr*)&sa, &sa_len);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;  /* No data */
        return -1;
    }

    if (received > 0) {
        /* Refresh the matching connection's timeout clock. Without this,
           a peer that only sends (never replies to anything we send) is
           timed out by lgx_net_update() despite the traffic. */
        uint32_t src_ip = sa.sin_addr.s_addr;
        uint16_t src_port = ntohs(sa.sin_port);
        uint64_t now = time_ms();
        for (uint32_t i = 0; i < sock->max_connections; i++) {
            if (sock->connections[i].active &&
                sock->connections[i].addr.ip == src_ip &&
                sock->connections[i].addr.port == src_port) {
                sock->connections[i].last_recv_ms = now;
                break;
            }
        }
    }

    if (addr) {
        addr->ip = sa.sin_addr.s_addr;
        addr->port = ntohs(sa.sin_port);
    }

    return (int)received;
}
```

Note the comparison: `connection.addr.ip` is stored as a network-order `sin_addr.s_addr` copied through `gethostbyname` at `:204`; confirm against `lgx_net_connect` before finalising (if `addr.ip` is stored host-order there, compare with `ntohl` consistently — the test below proves the direction).

### B7c — Replace the 8 `printf` calls

Find them: `grep -n 'printf' src/net/lgx_net.c`. Replace each with `lgx_log_tagged(LGX_SUBSYSTEM_NET, <level>, ...)` where level is `LGX_LOG_INFO` for established/disconnected, `LGX_LOG_WARN` for timeouts. Keep the format strings and arguments as they are. `lgx_runtime.h` is already included by the file (`:13` region); `lgx_log_tagged` is public (`lgx_runtime.h:260` region). `tests/net/test_net_basic.c:279` calls `lgx_runtime_init(NULL)` first, so logging is live in tests.

### B7d — Regression test

Append to `tests/net/test_net_basic.c` (harness: `TEST`/`PASS`/`FAIL` macros, `tests_passed == tests_run` exit code; keep the style):

```c
static void test_recv_refreshes_timeout(void) {
    TEST("Recv refreshes the connection timeout clock");

    lgx_net_config_t cfg = {
        .struct_size = sizeof(lgx_net_config_t),
        .max_connections = 8,
        .timeout_ms = 200,          /* short, for a fast test */
        .tick_rate = 30,
    };
    lgx_net_socket_t* receiver = lgx_net_socket_create(&cfg);
    lgx_net_socket_t* sender = lgx_net_socket_create(NULL);
    if (!receiver || !sender) { FAIL("socket create"); return; }

    if (lgx_net_socket_bind(receiver, 19878) != LGX_SUCCESS ||
        lgx_net_socket_bind(sender, 19877) != LGX_SUCCESS) {
        FAIL("bind"); goto cleanup;
    }

    /* Register the sender as a connection of the receiver. */
    int conn = lgx_net_connect(receiver, "127.0.0.1", 19877);
    if (conn < 0) { FAIL("connect"); goto cleanup; }

    /* Let the connection's clock age past the timeout WITHOUT traffic. */
    usleep(300 * 1000);

    /* Sender -> receiver traffic (source = 127.0.0.1:19877). */
    lgx_net_addr_t dest = { .ip = htonl(INADDR_LOOPBACK), .port = 19878 };
    if (lgx_net_socket_send(sender, &dest, "ping", 4) != 4) {
        FAIL("send"); goto cleanup;
    }
    usleep(20 * 1000);

    char buf[64];
    lgx_net_addr_t from = {0};
    if (lgx_net_socket_recv(receiver, &from, buf, sizeof(buf)) != 4) {
        FAIL("recv"); goto cleanup;
    }

    /* Pre-fix: elapsed-since-connect is ~320ms > 200ms -> connection is
       reaped. Post-fix: recv refreshed the clock -> still active. */
    lgx_net_update(receiver);
    if (lgx_net_get_connection_count(receiver) != 1) {
        FAIL("connection timed out despite receiving data");
        goto cleanup;
    }

    /* And the timeout still fires when traffic stops. */
    usleep(300 * 1000);
    lgx_net_update(receiver);
    if (lgx_net_get_connection_count(receiver) != 0) {
        FAIL("connection did not time out after traffic stopped");
        goto cleanup;
    }

    PASS();
cleanup:
    lgx_net_socket_destroy(sender);
    lgx_net_socket_destroy(receiver);
}
```

Add `test_recv_refreshes_timeout();` in `main` after `test_loopback_send_recv();`. Timing margins: 300ms ages vs 200ms timeout (100ms margin), 20ms recv delay; total test time ~0.7s. If flaky on loaded CI, raise timeout to 500ms and the sleeps to 700ms rather than loosening the assertion.

**Verification (order matters):**
1. Pre-fix: `ctest -R test_net_basic` (or run the binary) — new test fails at "timed out despite receiving data".
2. Apply B7: passes; second phase still proves the timeout works.
3. `ctest -R 'test_net|test_structured_logging'` green (the enum change must not disturb the logging tests).

**Commit message:**
```
fix(net): refresh connection timeout on receive; log via subsystem API

last_recv_ms was written only by lgx_net_connect, so any peer that sends
traffic but never triggers our send path was reaped by lgx_net_update
after one timeout window. Incoming datagrams now refresh the matching
connection (source address + port match), and the eight raw printf calls
route through lgx_log_tagged with the new LGX_SUBSYSTEM_NET value.

Verified: test_recv_refreshes_timeout fails pre-fix ("timed out despite
receiving data"); post-fix it also proves the timeout still fires when
traffic stops. test_structured_logging unaffected.
```

---

## Task B8 — Three small confirmed defects

Each is a separate commit. All three were verified by reading the code, not inferred.

### B8a — Leak detector wipes its own mutex

`src/runtime/lgx_leak_detector.c:60-73`: `pthread_mutex_init` at `:65`, then `memset(&g_leak_detector, 0, ...)` at `:69` zeroes the mutex. On glibc a zeroed default mutex happens to work, so this is latent — but it is UB and a trap for any later attribute change.

**Fix:** reorder — memset first, then init the mutex:

```c
lgx_result_t lgx_leak_detector_init(void) {
    if (g_leak_detector.initialized) {
        return LGX_ERROR_ALREADY_INITIALIZED;
    }
    
    memset(&g_leak_detector, 0, sizeof(lgx_leak_detector_t));
    
    if (pthread_mutex_init(&g_leak_detector.mutex, NULL) != 0) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    g_leak_detector.initialized = true;
    
    return LGX_SUCCESS;
}
```

**Test:** new `tests/unit/test_leak_detector_cycle.c` — `init -> add a sample -> shutdown -> init -> shutdown`, asserting success at each step (read `lgx_runtime.h`/internal header for the exact sample API, e.g. `lgx_leak_detector_add_sample`). Register in `tests/unit/CMakeLists.txt`. **Honest note in the commit:** the cycle passes pre-fix on glibc (zeroed mutex == default initializer); the test is a regression guard, the fix is justified by the standard, not by a red test on this platform.

```
fix(leak-detector): initialise the mutex after zeroing the state

lgx_leak_detector_init() ran pthread_mutex_init() and then memset() the
whole struct, wiping the mutex it had just initialised. Works by luck on
glibc (zeroed == PTHREAD_MUTEX_INITIALIZER) and breaks the moment the
mutex gains attributes. Reordered; added an init/add/shutdown cycle test
(regression guard -- cannot fail on glibc pre-fix).
```

### B8b — Hardware status returns uninitialised capability fields

`lgx_hardware_adapter_get_status` (`src/runtime/lgx_hardware_adapter.c:120-140`) fills tier/reason strings but never `huge_pages_available` / `numa_topology_detected` / `gpu_acceleration_available` (the `lgx_hardware_status_t` fields). The health monitor (`src/runtime/lgx_health_monitor.c:161-163`) copies those fields out of a by-value struct, so it propagates uninitialised stack memory.

**Fix — replacement of the whole function:**

```c
lgx_hardware_status_t lgx_hardware_adapter_get_status(lgx_hardware_adapter_t* adapter) {
    lgx_hardware_status_t status = {0};
    status.struct_size = sizeof(lgx_hardware_status_t);
    
    if (!adapter) {
        status.achieved_tier = LGX_HW_TIER_DEGRADED;
        status.missing_capabilities = 0xFFFFFFFF;
        status.degradation_reason = "Hardware adapter not initialized";
        status.performance_impact_estimate = "Unknown";
        status.remediation_steps = "Initialize hardware adapter";
        return status;
    }
    
    status.achieved_tier = adapter->tier;
    status.missing_capabilities = adapter->missing_capabilities;
    status.degradation_reason = adapter->degradation_reason;
    status.performance_impact_estimate = adapter->performance_impact;
    status.remediation_steps = adapter->remediation_steps;
    
    /* Capability flags: previously left uninitialised, which the health
       monitor then copied into its own status struct. */
    status.huge_pages_available = adapter->huge_pages_available;
    status.numa_topology_detected = adapter->numa_available;
    status.gpu_acceleration_available = adapter->gpu_available;
    
    return status;
}
```

Verify the destination field names against `lgx_hardware_status_t` in `include/lgx_types.h` (`:185-195` region) before editing; adjust the sources (`adapter->huge_pages_available`, etc.) to what the adapter struct actually holds (`:19-36`).

**Test:** extend `tests/unit/test_health_monitor.c` (it exists) with a consistency check: init adapter, get status, assert each capability flag equals the corresponding `lgx_hardware_adapter_has_*` getter. Pre-fix this reads uninitialised stack — deterministic-fail only when the underlying capability is true, so note in the commit: "on this host, huge pages = false so that flag alone can't discriminate; the gpu flag does". Run under `valgrind --track-origins=yes` once locally for the uninit-read evidence if valgrind is available.

```
fix(hardware): initialise capability flags in get_status

lgx_hardware_adapter_get_status() left huge_pages_available,
numa_topology_detected and gpu_acceleration_available uninitialised;
the health monitor copied them straight out of the by-value struct.
Status now starts zeroed and the three flags mirror the adapter's own
detection results.

Verified: consistency assertions in test_health_monitor (flag ==
has_* getter); strong on hosts with the capability present.
```

### B8c — Memory-manager tracking compares the wrong pointers

`src/runtime/lgx_memory_manager.c`: `track_allocation` (`:1795-1819`) copies an `allocation_header_t` into `manager->tracked_allocations[...]`. Both `lgx_memory_manager_get_usage_stats` (`:1910-1911`) and `untrack_allocation` (`:1960-1961`) search with `(void*)&manager->tracked_allocations[i] == ptr` — comparing the address of the array slot with the user pointer, which can never match. Consequences: usage stats always return the "unknown" defaults, and untrack never removes entries, so `tracked_allocations` grows until realloc every call and leaks entries forever on the active allocation path.

**Fix:**
1. Add a `void* user_ptr;` field to `allocation_header_t` (`:137-144` region — read it).
2. In `track_allocation` (`:1818-1819`), set `entry->user_ptr = ptr;` (the function's `ptr` parameter is the user pointer; confirm by reading the call site before editing).
3. In both search loops, compare `manager->tracked_allocations[i].user_ptr == ptr`.
4. The untrack "move last entry over this slot" copy (`:1963-1966`) already copies the new field structurally.

Also change the unbounded growth behaviour: `track_allocation`'s capacity growth (`:1804-1810`) is fine once untrack works, but add a defensive hard cap (e.g. reuse `tracked_capacity` doubling only when `tracked_count` grows; do not add a new constant) — minimal change is the pointer fix; report growth behaviour in the commit only if measured.

**Test:** new `tests/unit/test_memory_manager_usage_stats.c`:

```c
#include "lgx_runtime.h"
#include "lgx/lgx_runtime_internal.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    lgx_hardware_adapter_t* hw = NULL;
    assert(lgx_hardware_adapter_init(&hw) == LGX_SUCCESS);

    lgx_memory_manager_t* manager = NULL;
    assert(lgx_memory_manager_init(&manager, NULL, hw) == LGX_SUCCESS);

    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 64,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_SESSION,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };

    void* ptr = lgx_memory_manager_alloc(manager, 64, &intent);
    assert(ptr != NULL);

    lgx_allocation_usage_t usage = { .struct_size = sizeof(lgx_allocation_usage_t) };
    assert(lgx_memory_manager_get_usage_stats(manager, ptr, &usage) == LGX_SUCCESS);

    /* Pre-fix: the search never matches, so these are the "not found"
       defaults (access_count 0, pattern UNKNOWN, confidence 0.0). */
    assert(usage.access_count > 0);
    assert(usage.observed_pattern == intent.access_pattern);
    assert(usage.pattern_confidence > 0.0f);

    lgx_memory_manager_free(manager, ptr);

    lgx_memory_manager_shutdown(manager);
    printf("test_memory_manager_usage_stats: OK\n");
    return 0;
}
```

Read `lgx_memory_manager_init`'s expectations first (`:75` region of the internal header): if it rejects a NULL config, pass a `lgx_config_create()`-made config instead. Register the test in `tests/unit/CMakeLists.txt`.

**Verification:** pre-fix run fails on `access_count > 0`; post-fix passes. ASan build of the unit test: clean.

```
fix(memory): track the user pointer, not the tracking-slot address

get_usage_stats and untrack_allocation searched tracked_allocations for
"(void*)&tracked_allocations[i] == ptr" -- the address of the copy in
the manager's own array, compared against the user's pointer. It can
never match: usage stats always returned the unknown defaults and
untrack never removed an entry, leaking tracking slots. Entries now
carry the user pointer.

Verified: test_memory_manager_usage_stats fails pre-fix (access_count 0,
pattern UNKNOWN), passes post-fix.
```

---

## Task D1 — Disassembly evidence for the arena hot path

**Purpose:** Plan 01 measured "~23ns/alloc" and then removed observers without ever proving, at the instruction level, what the hot path actually does. Before touching anything else, produce the instruction-level evidence once, and keep it in the repo.

**Steps:**
1. Build Release with the current tree (post-A/C/B): `cmake -S . -B /tmp/opencode/lgx-release -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/opencode/lgx-release -j$(nproc)`.
2. Disassemble and extract the success path:
   ```bash
   objdump -d /tmp/opencode/lgx-release/liblgx_runtime.so \
     | awk '/<lgx_frame_alloc_internal>:/,/^$/' > /tmp/opencode/arena-hotpath.asm
   wc -l /tmp/opencode/arena-hotpath.asm
   grep -cE '\block\b|\bxadd[q|l]\b|xchg' /tmp/opencode/arena-hotpath.asm   # atomics
   grep -cE '\bcall\b' /tmp/opencode/arena-hotpath.asm                       # out-of-line calls
   grep -nE 'call|lock' /tmp/opencode/arena-hotpath.asm
   ```
3. Classify every `call`: `lgx_time_now_ns` (if in this path — it is in `lgx_frame_reset`, not alloc), telemetry, `fprintf`, `memcpy`, `__asan_*` (debug only), PLT stubs. The alloc success path that matters is between the function entry and a `ret` where `ptr` is returned; note which of the calls are on that path vs. the overflow/slow paths.
4. Write `docs/05-performance/arena-hotpath-profile.md` (the file referenced by the `3abeb60` comments — if it does not exist, create it) with:
   - the exact commands above,
   - the annotated instruction counts (atomics / calls / total, success path only),
   - a table: observer → on success path? → evidence (`call` at offset, `lock` opcode) → verdict (kept/removed in D2).

**Commit message:**
```
docs(perf): disassembly of the frame-arena success path

Instruction-level evidence before further hot-path changes: call sites
and locked instructions in lgx_frame_alloc_internal, classified by
whether they sit on the success path. D2 removes observers one at a
time, each backed by a row in this file.
```

---

## Task D2 — Measured removal of the remaining hot-path observers

**One commit per change. One measurement per commit. Order is fixed by expected cost, cheapest first. Stop rule at the end.**

Working method for every step (this is the Plan 00 methodology — do not improvise):
1. Before the change: `taskset -c 2 /tmp/opencode/lgx-release/benchmark_allocation_throughput` at least 3 times; take the median of the `lgx_frame_16B/64B/256B` FRAME_RESET medians, plus `bump_floor_16B`.
2. Make the change; rebuild; run the benchmark the same number of times.
3. Record the delta in `docs/05-performance/arena-hotpath-profile.md` (benchmark section) **with both commands**. The doc is committed in the same change as the code; the commit message references the doc and never restates the numbers (AGENTS.md rule 1).
4. If the delta is within run-to-run noise (spread of the 3 runs), revert the change and record that too. A change that cannot be shown to help does not ship.
5. Commit only if kept.

**D2-1 — `lgx_frame_reset` warning block (stderr + clock read).**
`lgx_frame_reset` (`src/runtime/lgx_frame_arena.c:686-712`) calls `lgx_time_now_ns()` every reset and prints up to two `fprintf` lines when the reset gap is < 8ms — every frame in a 120+ FPS benchmark, every frame of a 60Hz game in the `< 33ms` happy case is fine but the benchmark hits the fast path. The benchmark harness currently *mutes stderr to work around this* — that is the defect's signature. Fix: keep the diagnostic, move it off the default path:
- Wrap the frequency/infrequency warnings in `#if LGX_DEBUG_BUILD` (the file already uses that macro for magic numbers), or gate on an env var checked once (`getenv("LGX_ARENA_RESET_WARNINGS")`) — prefer the compile-time gate, it costs nothing and matches the file's existing idiom.
- Keep `last_reset_time_ns` bookkeeping (timestamps are also stats) but do not call `lgx_time_now_ns()` a second time — the value is already fetched at `:695` and stored at `:767`.
- Expected: single-digit ns per reset at most; possibly below noise. If noise, revert per step rule.
- Also remove the stderr-muting workaround from `benchmarks/benchmark_allocation_throughput.c` in the **same commit** (it is only valid while this warning is off the default path) and note it: the workaround's comment says "workaround, not a cure".

**D2-2 — Rolling-average loop.**
`update_rolling_average` (`:335-346`) runs a 60-iteration loop every reset (`:741`), called from the reset path the benchmark measures. Replace the loop with an O(1) running sum: add `size_t usage_sum;` to the struct, update `usage_sum += new_value - usage_history[history_index]` on each insert, keep `rolling_average = usage_sum / ROLLING_AVERAGE_WINDOW` (or maintain the division — it is once per reset). Keep the window semantics identical; `test_frame_arena_polish` must stay green.

**D2-3 — Per-arena statistics (C1's counters).**
Only touch this if D1 showed the counters' stores are significant (they are not loads; stores to lines already dirty). If the delta is measurable: the honest options are (a) keep them (they are public API via `lgx_frame_get_stats`), (b) move them behind a config flag. Do **not** repeat `3abeb60`'s mistake of deleting asserted behaviour without changing the tests first. If kept, record "measured, within noise" in the profile doc.

**D2-4 — `record_profiler_reset_event` (only if D1 shows calls).**
Read `:1327-1350`: with profilers compiled out, the function body is three `bool` checks; GCC will inline it. If D1 shows an actual out-of-line call in the reset path, mark the function `inline` (or make the call sites static-force-inline). If already inlined, record "no action, evidenced".

**Stop rule (from Plan 01, unchanged):** after D2, if `lgx_frame_16B` is more than 2× the `bump_floor_16B` median, the arena is slower than a plain bump pointer for reasons the observer removals do not explain; record that verdict honestly in the profile doc and in D4's claims table, and do not iterate further without new evidence. The "200× faster than malloc" claim stays retracted; a claim may only be made from D3's mixed workload on this hardware.

**Commit messages:** one per kept change, each naming the observer and pointing at the profile doc — **no numbers in the message** (AGENTS.md rule 1; the doc is part of the same commit). Example:
```
perf(arena): gate reset-frequency warnings behind LGX_DEBUG_BUILD

lgx_frame_reset printed up to two stderr lines per frame for reset gaps
< 8ms -- i.e. every benchmark frame -- and the benchmark harness carried
a stderr-muting workaround because of it. The diagnostic is now
compile-time gated like the other debug-only behaviour in this file; the
workaround is removed from the harness.

Before/after medians and the exact commands are recorded in
docs/05-performance/arena-hotpath-profile.md (committed here).
```

---

## Task D3 — Mixed-size, many-live-objects workload

**Purpose:** the current benchmark allocates one size in a tight reset loop — glibc malloc's best case, and not what a game frame looks like. Plan 01 Task 8's requirement, still unbuilt.

**File:** `benchmarks/benchmark_allocation_throughput.c` (extend; do not create a second harness — the CSV/provenance plumbing and the muting removal from D2-1 live here).

**Design (keep it comparable to the existing rows):**
- New scenario `MIXED_LINE`, printed with the same `FLOOR_LINE`-style machine-readable format so `scripts/verify_benchmark_methodology.sh` can parse it.
- Per "frame": allocate N=64 objects with a documented size distribution — e.g. 70% in 16..256B (zipf-ish or fixed list `{16,32,48,64,96,128,256}`), 20% in 1..8KB, 10% 64KB..1MB — from the frame arena, writing one byte per cache line of each object (live, touched memory — the existing loop already touches memory, keep that property).
- Same for the malloc comparator in the same run (allocate via malloc, touch, hold, free in reverse order), so the ratio is computed from one binary, same machine, same run.
- Hold all 64 objects live until the reset (that is the point), then `lgx_frame_reset()`.
- Repeats: median-of-5 like the existing scenario; output row name `MIXED_LINE`.

**Verification:** run pinned 3×; medians stable within the spread documented in the profile doc; `verify_benchmark_methodology.sh` still exits 0 (fix its parser if the new row name needs registering — extend, never loosen).

**Commit message:**
```
bench(arena): add mixed-size many-live-objects workload

The single-size tight loop is glibc malloc's best case; game frames
allocate mixed sizes and keep objects live. MIXED_LINE allocates 64
mixed-size objects per frame (70/20/10 split of small/medium/large),
touches every cache line, holds them until reset, and runs the same
pattern through malloc in the same process for a same-run ratio.
Median-of-5, same provenance plumbing as the existing rows.
```

---

## Task D4 — Claims table, ratio gate, snapshot

**Purpose:** make the repo unable to claim anything the harness does not print, and make a real regression fail CI.

**D4-1 — Snapshot.** Copy the final benchmark stdout (verbatim, dated, with host/governor) into `docs/05-performance/arena-hotpath-profile.md` under "Final measurement (2026-10-18)". Every number in README must be traceable to this block or to `benchmarks/baseline_frame_arena.txt`.

**D4-2 — Ratio gate.** New `scripts/verify_arena_ratios.sh` (follow the structure of `verify_benchmark_methodology.sh`: fail loudly, no absolute ns thresholds):

```bash
#!/usr/bin/env bash
# Gate: the frame arena must not regress against the recorded baseline.
# Absolute times are machine-dependent; this compares medians from a
# fresh run against the snapshot committed in
# docs/05-performance/arena-hotpath-profile.md within a wide band, and
# checks the structural property that is the actual claim:
#   latency grows no worse than linearly across the size ladder.
set -euo pipefail
# 1. run the benchmark pinned, capture to a temp file
# 2. parse FRAME_RESET medians for 16/64/256B + MIXED_LINE + bump_floor_16B
# 3. assert: lgx_frame_256B_median <= 1.5 * lgx_frame_16B_median   (no cliff)
#    assert: every parsed median is within [0.5x, 2.0x] of the snapshot
# 4. exit non-zero with the two numbers on failure
```

The exact thresholds live in the script header with a sentence each on why; do not ship a threshold that passes with the pre-fix numbers unless the band is genuinely that wide — the purpose is catching regressions, not certifying performance. Register the script in the CI `build-and-test` job (Release build step) as a new step, and add it to `scripts/check_gate_scripts_tracked.sh`'s expectations if that script enumerates gates.

**D4-3 — README claims table.** In `README.md`, replace performance prose with a table:

| Claim | Status | Evidence (command) | Commit |
|---|---|---|---|
| "200× faster than malloc" | **retracted** | refuted by own benchmark (Plan 00) | `c7147d4` |
| Frame arena vs bump floor (16B) | ratio from D2 snapshot | `taskset -c 2 ./benchmark_allocation_throughput` | D2 commit |
| Frame arena vs glibc malloc (1B–1MB) | ratio from A3/D3 snapshot | same | A3/D3 commits |
| GPU pool: VkDeviceMemory returned correctly | fixed | `ctest -R test_gpu_pool` | B2 commit |
| Non-Vulkan build | builds + tests | `no-vulkan` CI job | B6 commit |

Every row's command must run today and print the cited number. No row may say "production-ready".

**Commit message:**
```
docs(perf): claims table backed by commands; ratio gate in CI

README performance prose replaced with claim -> status -> evidence
command -> commit. New verify_arena_ratios.sh runs the benchmark pinned
and fails on a >2x deviation from the committed snapshot or a size-ladder
cliff; wired into the build-and-test job. Snapshot quoted verbatim in
arena-hotpath-profile.md.
```

---

## Task E1 — Close-out

**Steps, in order:**
1. **Docs index.** Update `docs/superpowers/plans/README.md`:
   - Revision note: Plan 01 status changed — measurement tasks 7-8 executed on main (`ceb7855`); correctness tasks 1-6 and the restored-stats/`bcd8b52` damage are re-homed to `2026-10-18-phase1-completion.md`, which supersedes it.
   - Table row for Plan 01: mark "superseded by 2026-10-18-phase1-completion.md".
   - Recommended-order diagram: `Plan 01 ✅ DONE (2026-10-18) -> Plan 06 ...` with the new file named.
2. **Full verification from a fresh clone** (AGENTS.md rule 2 — an existing build tree proves nothing):
   ```bash
   git clone . /tmp/opencode/lgx-final && git -C /tmp/opencode/lgx-final switch <the merged main or phase branch>
   cmake -S /tmp/opencode/lgx-final -B /tmp/opencode/lgx-final-build -DCMAKE_BUILD_TYPE=RelWithDebInfo
   cmake --build /tmp/opencode/lgx-final-build -j$(nproc)
   ctest --test-dir /tmp/opencode/lgx-final-build --output-on-failure
   # gates run from the clone:
   (cd /tmp/opencode/lgx-final && ./scripts/verify_test_integrity.sh && ./scripts/count_tests.sh \
      && ./scripts/verify_benchmark_methodology.sh && ./scripts/check_gate_scripts_tracked.sh)
   ```
   All green, counts recorded in the close-out commit. ASan build + `ctest` from the same clone as well (the `asan-ubsan` job's local equivalent).
3. **Working-tree state:** `git status` clean except the files this plan staged; `benchmark_allocation_throughput.csv` no longer tracked (A2) and any new one deleted from the repo root.
4. **Stale branch/worktree cleanup** (the `ground-truth` branch is fully merged; its plan was absorbed):
   ```bash
   git worktree remove --force .worktrees/ground-truth && git worktree prune
   git branch -D ground-truth
   ```
   Only after `git log main..ground-truth` shows nothing unmerged; if it shows commits, stop and report instead of deleting.
5. **Report to the repository owner:** one summary — commits made, test count before/after, gate results, benchmark medians (A3 vs final), and the honest negatives (whatever the 2× bump-floor stop rule concluded; the leak-detector test's limitation). Do not push; the owner pushes and merges.
6. **Human-only handoff list** (deliver as text, not by executing — the agent has no admin rights and must not hold them):
   - Fine-grained token scoped to this repository with contents / pull-requests / issues permissions only; log in via `gh auth login` in the owner's own terminal; never paste the token into chat or an agent.
   - Branch ruleset on `main`: require a pull request, require the `build-and-test` check (`no-vulkan` becomes required only after B6 lands), block force pushes.
   - README/docs description alignment (owner runs), e.g.:
     `gh repo edit dream1290/LGX --description "Experimental C11 memory-allocator and threading primitives for Linux games, with a C API. Pre-1.0: interfaces may change." --remove-topic abi,abi-linux,stable,stable-abi,fedora,arch,ubuntu,lifecycle,linux-abi --add-topic c,linux,game-development,memory-allocator,arena-allocator`
   - Mark tag/release `v1.0.1` as a pre-release (the project's own audit says 0.x).
   - `GITLEAKS_LICENSE` only if the repository is private.
   - Facts already verified on 2026-10-18: `opencode.json` in `bcd8b52` contains no secrets; no rotation needed. `bcd8b52`'s stated numbers are withdrawn by A0's revert message.

**Commit message:**
```
docs: close out Phase 1 completion (2026-10-18)

Plan index updated: 2026-10-18-phase1-completion.md supersedes Plan 01.
Full verification recorded: ctest <n>/<n>, all four gate scripts exit 0,
ASan suite green, final benchmark medians in arena-hotpath-profile.md.
```

---

## Execution order

```
A0 -> C2 -> A3 -> A4              # branch fix/revert-arena-misstep, then main is green
B1 -> B2 -> B3 -> B4 -> B5 -> B6  # branch fix/phase1-safety (B5 before B6)
B7 -> B8                          # same branch, separate commits
D1 -> D2 -> D3 -> D4              # branch bench/arena-measurement
E1                                # close-out + handoff
```

A0 is its own PR (`fix/revert-arena-misstep`); merge it before branching for C2/A3/A4. A1/A2/C1 are skipped when A0 is executed as written. C2 is small and can ride in the first B branch if you prefer fewer PRs, but keep the commits separate. B5 and B6 share one code region — B5 first, B6 only touches CI/CMake/test guards. D2 must not start before A3 exists (measure against a recorded baseline, and only on a tree built from a fresh clone); D4 must not start before D3's workload runs. Branch rules: no direct pushes to `main` after A0's PR; the owner merges.

**Definition of done for the phase:**
1. `main` builds from a clean clone with zero warnings, with and without Vulkan.
2. Every B-task's regression test exists, fails on the pre-fix commit, and passes now (the B8a test excepted, documented).
3. `ctest` green in RelWithDebInfo and under ASan/UBSan.
4. All four gate scripts exit 0; CI's three jobs are all genuine gates (no `continue-on-error`, no "known failing").
5. README contains no performance number that a command in this repo cannot reproduce today.
6. `AGENTS.md` exists at the root and the rules survived execution of this plan (numbers only in docs, fresh-clone verification, no gate weakening).
7. CI rejects tracked ELF binaries and oversized files, and scans for secrets (B6 Part 4).
8. The human-only handoff list (E1) is delivered: token scope, branch ruleset, description/topics edit, v1.0.1 marked pre-release, and any gitleaks license if the repo is private.

