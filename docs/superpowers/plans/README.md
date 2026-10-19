# LGX Remediation Plan Index

**Created:** 2026-10-10 · **Revised:** 2026-10-19 (Plan 01 superseded and executed; see below)
**Source:** `docs/AUDIT_REPORT.md`
**Method:** Every plan below assumes Plan 00 has been merged. Plan 00 restores the ability to build, compile, measure, and gate. No other plan is trustworthy without it.

---

## Revision note — 2026-10-18

Plan 01's measurement tasks were executed directly on `main` (`ceb7855`, `3abeb60`, `40a4721`, `b6c5ab7`, `bcd8b52`). That left `main` **not building** (header regression in `b6c5ab7`), swept 22 junk files into git (`bcd8b52`), and removed frame-arena statistics that tests still assert (`3abeb60`). A full re-investigation confirmed six Plan 01 defects, found four additional GPU buddy-allocator memory-safety defects, a config initialisation bug, and three small defects in modules.

**`2026-10-18-phase1-completion.md` supersedes Plan 01.** It restores the baseline (A0–A4, C2), executes the correctness tasks with reproducing tests (B1–B8), and completes the arena measurement work under the disassembly-first discipline (D1–D4). Rev 2 incorporates the repository review response: A0 fixes `main` via three reverts on a branch (the build breaker is `b6c5ab7`, not `bcd8b52`), branch/PR workflow replaces direct-to-main commits, AGENTS.md working rules, CI hygiene guards (gitleaks, tracked-binary check), and the no-numbers-in-commit-messages rule for measurements.

## Revision note — 2026-10-19 (execution record)

`2026-10-18-phase1-completion.md` was executed on branch `fix/phase1-safety` (A0 through D4). Correctness tasks 1–6, the restored-stats damage (`3abeb60`), the GPU buddy-allocator defects, and the three small module defects (net timeout subsystem tagging, leak-detector init order, hardware-adapter status, intent-allocator GPU routing) are re-homed there and now have reproducing tests. The measurement tasks (arena hot-path profile, observer gating, claims table + ratio gate) were likewise re-homed and executed. Plan 01 is fully superseded; its outline below is historical.

---

## Revision note — why the order changed

Plan 00 produced a measurement that reordered the roadmap:

> Under the project's own (now reproducible) benchmark, the frame arena is **1.07×–1.92× slower than glibc malloc at 1–4KB**, at parity at 16–256B. The general `lgx_alloc` path is materially slower than glibc malloc.

Two consequences, both of which invalidate parts of the original plan:

1. **The malloc-replacement and Wine-heap positioning has no evidence behind it.** "Materially slower than glibc" means it will not beat mimalloc, tcmalloc, or jemalloc — the actual bar. Those two pillars of the original strategy are unsupported until benchmarked against a real competitor. Hence **Plan 06**.
2. **"The arena is slower than a bump pointer" is a defect to locate, not a property of the technique.** A bump allocator should cost a few nanoseconds. Task 7 of Plan 01 profiles the hot path; the initial measurement (three unconditional observers costing 1.5–4.1 ns per allocation) is in Plan 01's provenance table and does **not** explain the full gap.

Plan 05 therefore cannot begin until Plan 01 and Plan 06 establish what is honestly claimable. Shipping a single-header arena on an unmeasured value proposition is how you build a repository nobody stars.

---

## Why this is decomposed

The audit found defects across seven independent subsystems. A single combined plan would be unexecutable and would have no independently reviewable deliverable. Each plan below produces working, testable software on its own and can be reviewed — and rejected — without affecting its neighbours.

Plans are ordered by dependency, not by severity. Severity ordering is preserved *within* each plan.

| Plan | Scope | Depends on | Blocks |
|---|---|---|---|
| **00 — Ground Truth** | Build, C++ header, benchmark methodology, test-integrity contract, CI gate | — | everything |
| **01 — Correctness & Arena Perf** | **superseded — executed as `2026-10-18-phase1-completion.md` (2026-10-19)** | 00 | 05, 06 |
| **01b — Phase 1 Completion (2026-10-18)** | Baseline restore, 14 verified defects with reproducing tests, disassembly-first arena perf, claims table + ratio gate · **executed 2026-10-19 on `fix/phase1-safety`** | 00 | 02, 03, 05, 06 |
| **02 — Modules** | Threading flake, fiber bugs, graphics extension pointers, leak detector, health monitor | 00, 01 | 05 |
| **03 — Test Recovery** | The 25 assert-only tests and 12 disabled tests | 00 | 05 |
| **04 — Repository Truth** | Remaining doc reconciliation, the 8 unrepaired workflows, git history | 00 | 05 |
| **06 — Allocator Competitiveness** | `lgx_alloc` vs mimalloc/tcmalloc on a standard suite — decides whether the replacement path exists | 01 | 05 |
| **05 — The Wedge** | `lgx_arena.h` — **gated on 01/02/03/04/06** | 00–04, 06 | — |

---

## Plan 00 — Ground Truth ✅ WRITTEN

`docs/superpowers/plans/2026-10-10-ground-truth.md`

| # | Task | Fixes |
|---|---|---|
| 1 | Clean build succeeds | B2 — `numa` never linked; build break masked by a committed binary |
| 2 | Header consumable from C++ | B1 — unbalanced `extern "C"` blocks; header never compiled as C++ |
| 3 | Benchmark measures the real pattern | B3 — frame arena benchmark never called `lgx_frame_reset()` |
| 4 | Test integrity contract | Makes the 37 incapable tests visible and blocks new ones |
| 5 | CI gate that runs | CI referenced `examples/`, `lgx_arena_example`, `make asan` — none exist |

**Outcome after Plan 00:** a clean checkout builds with zero warnings; `lgx_runtime.h` compiles under C11 and C++17; the benchmark measures what it claims; CI fails when reality regresses. Every subsequent plan inherits this.

**Explicitly out of scope:** allocator algorithms, memory-safety fixes, incapable tests, git history, documentation reconciliation.

---

## Plan 01 — Correctness & Arena Performance 🗄️ SUPERSEDED (executed 2026-10-19 via 2026-10-18-phase1-completion.md)

`docs/superpowers/plans/2026-10-11-correctness-and-arena-performance.md`

Eight tasks. Tasks 1-6 are correctness; each lands with a regression test that fails before the fix. Tasks 7-8 are measurement.

| # | Task | Fixes / determines |
|---|------|--------------------|
| 1 | GPU pool handle use-after-free | Handles store a raw pointer into an array `realloc` moves → store a stable index |
| 2 | Frame arena mid-frame growth | Old mapping unmapped while live pointers point into it → retire until frame end |
| 3 | Chaos testing self-deadlock | Non-recursive mutex locked twice → chaos testing has never been enable-able |
| 4 | GPU-sharing hint never routed | `lifetime` checked before `hint`, so the public GPU API returned frame-arena memory |
| 5 | Unguarded `lgx_gpu_alloc` | Non-Vulkan builds fail to link → makes the red `no-vulkan` CI job green |
| 6 | Net connections always time out | `last_recv_ms` only set at connect, never refreshed on receive |
| 7 | **Arena hot-path profile** | Locates the real cost of the allocation success path; decides keep/gate/remove per observer |
| 8 | **Unbiased workload** | Adds mixed-size, many-live-objects workload; the existing tight loop is glibc's best case |

**Plan 01 supersedes the earlier Plan 01 sketch** (correctness only) by folding in the arena investigation. Tasks 1-6 are the same defects, now with the plan's original detail restored plus the GPU-link guard that was previously only a "known failing" CI note.

---

## Plan 06 — Allocator Competitiveness (NOT YET WRITTEN)

The gate that decides whether a whole positioning survives. Scope:

- Benchmark `lgx_alloc` against **mimalloc, tcmalloc, and jemalloc** on a standard allocator suite (same harnesses Plan 00 established: CPU-pinned, median-of-N, stderr-muted, provenance recorded).
- Report multi-threaded scaling, not just single-thread. An allocator that loses single-threaded can still win under contention, and games are multi-threaded.
- Decide explicitly, in writing: does the malloc-replacement path exist?
  - **If `lgx_alloc` is competitive** → the original positioning survives and the arena becomes the on-ramp rather than the whole pitch.
  - **If it is not** → the project drops the replacement framing entirely and positions only on what Plan 01 measured. That is a positioning change, not a code change, and it should be made deliberately before a single-header library ships.

The honest framing to test against: glibc malloc at ~20 ns for small hot allocations is a strong baseline, and a general-purpose allocator is a large engineering surface to beat it on. The expectation from Plan 00's data is that this will not be won. Establish it rather than assume it.

---

## Plan 05 — The Wedge (NOT YET WRITTEN — gated)

`lgx_arena.h` as a single-header library.

**This plan must not start until 01, 02, 03, 04, and 06 have landed.** Its differentiation must be derived from what those plans measured, not chosen during implementation. Single-header arena allocators are a crowded space and none win on a raw-latency claim.

Settle before writing this plan:

1. **What is measurable and true?** Candidate ground: fragmentation behaviour under a realistic frame, safety checking at reset (use-after-reset detection already exists behind `LGX_DEBUG_BUILD` — can it be made cheap enough to ship on?), integration with the rest of LGX (intents, profiling hooks, the persistent heap for overflow). A benchmark must demonstrate it, per Plan 01 Task 8's methodology.
2. **API shape.** Handle-based (`lgx_arena_create` / `lgx_frame_alloc(arena, …)`), matching the already-written tests and avoiding collision with the runtime's persistent `lgx_alloc`. Do not ship the thread-local global API from the staged spec.
3. **Licence.** The spec says Apache 2.0; the launch copy says MIT and links `github.com/yourusername/lgx-arena`. Converge on one before publishing anything.
4. **Honest performance claim.** Whatever Plan 01 Tasks 7-8 establish. The current "200× faster than malloc" is retracted in the README and must not return in any artifact.
5. **Repository.** Keep in this repo until external demand justifies a second one.

---

## Plan 02 — Modules (NOT YET WRITTEN)

| Priority | Defect | Location |
|---|---|---|
| P1 | `test_threading_basic` fails ~1 in 60 runs — intermittent concurrency defect | `src/threading/lgx_threading.c` |
| P1 | Fiber scheduler stores a pointer to a stack-local `ucontext`; `lgx_fiber_wait_job` has no wake path | `lgx_threading.c:1533, 1625, 1652` |
| P2 | Graphics may pass NULL extension pointers to `vkCreateInstance` | `src/graphics/lgx_graphics.c:294–298` |
| P2 | Tools ring-buffer percentile math corrupts after wrap | `src/tools/lgx_tools.c:182` |
| P2 | Health monitor reads uninitialised `huge_pages_available`; hardcoded `memory_usage_mb` | `lgx_health_monitor.c:161, 187` |
| P3 | Asset async loading is a synchronous stub | `src/asset/lgx_asset.c:245` |

The threading flake is the highest-value item: it is the only defect that reproduces on demand, and it sits in the job system every other module depends on.

---

## Plan 03 — Test Recovery (NOT YET WRITTEN)

Plan 00 makes these visible. This plan makes them real.

| Group | Count | Action |
|---|---|---|
| Incapable tests | 37 | Add real assertions and non-zero exit paths. Start with `test_frame_arena.c` (363 lines, zero assertions) and the four performance tests. |
| Disabled tests | 12 | Migrate to the current API or delete. Causes on record: "compilation errors", "non-existent `lgx_version_info_t` API", "API mismatches". |
| Integration coverage | 1 of 6 files active | Restore coverage of the runtime as a whole. |

**Definition of done:** `scripts/verify_test_integrity.sh` exits 0, and `scripts/count_tests.sh` reports `incapable=0`.

---

## Plan 04 — Repository Truth (NOT YET WRITTEN)

| Item | Action |
|---|---|
| `build/` in git | Untrack and, if history rewrite is acceptable, purge. This stale artifact masked the build break. |
| Deleted `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md` | Restore from history or rewrite. |
| `LGX_ARENA_SINGLE_HEADER_SPEC.md` | Currently staged-then-deleted. Decide: recover or retire. |
| Three conflicting test counts (59/64/76) | Reconcile to `scripts/count_tests.sh` output. |
| "Production-Ready" banners | Downgrade until Plans 01–03 complete and CI is green. |
| Stale case study | `docs/LAUNCH_BLOG_POST.md` describes a C++ SuperTuxKart integration that could not have compiled against the shipped header. Correct or withdraw. |
| CAD expansion analysis | Decide whether it stays. It is an unapproved proposal, not a plan. |

---

## Cross-cutting rules for every plan

These apply to Plans 01–06 and are not repeated in each document:

1. **Write the failing test first.** For every defect fix, add a test that reproduces the bug and fails before the change. The current suite caught none of the audited defects; a fix without a reproducing test is not accepted.
2. **Every new test must be able to fail.** `scripts/verify_test_integrity.sh` is the gate (Plan 00 Task 4).
3. **No claim without a reproducing command.** Any number in any document must name the command that produces it.
4. **No new warnings.** The build is warning-free under `-Werror`; that property must survive every change.
5. **Commit per task**, with a message that states what defect is fixed and how it was verified.
6. **Do not weaken a gate to make it pass.** If a test cannot be made to pass, that is information — report it, do not delete the assertion.
7. **Sanitizers on for memory work.** Memory-safety fixes are verified under the Debug ASan/UBSan build before being called done.

---

## Recommended order

```
Plan 00  Ground Truth      ✅ DONE — branch `ground-truth`, merged INTO 01b execution
   ↓
Plan 01  Correctness & Arena Perf   ✅ DONE (2026-10-19) — superseded by
   ↓                                   docs/superpowers/plans/2026-10-18-phase1-completion.md,
                                       executed on `fix/phase1-safety`
Plan 06  Allocator Competitiveness ← lgx_alloc vs mimalloc/tcmalloc/jemalloc
   ↓                                   decides whether the replacement positioning survives
Plan 02  Modules           ← threading flake first
   ↓
Plan 03  Test Recovery     ← make the suite able to fail
   ↓
Plan 04  Repository Truth  ← remaining docs, the 8 unrepaired workflows, history
   ↓
Plan 05  The Wedge         ← only now, and only with a claim 01+06 actually measured
```

Plans 02 and 06 are independent of each other once 01 lands and may run in
parallel. Plan 03 benefits from 01 and 02 landing first, because the regression
tests written there will exercise the fixed code.

**Why 06 sits before 05 and not after:** shipping a single-header arena that
leads with a performance claim the project's own benchmark already refuted is
the failure mode this whole plan sequence exists to prevent. Establishing what
is honestly claimable is upstream of building the artifact.
