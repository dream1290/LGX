# Frame-arena hot path: instruction-level profile (Task D1)

This file records the assembly evidence behind the arena hot path so that
hot-path work can be judged per instruction, not per claim. It owns the
regression evidence for the D2/D4 tasks of
`docs/superpowers/plans/2026-10-18-phase1-completion.md`.

## Commands (reproduce exactly)

```bash
cmake -S . -B /tmp/opencode/lgx-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/opencode/lgx-release -j$(nproc)

objdump -d /tmp/opencode/lgx-release/liblgx_runtime.so \
  | awk '/<lgx_frame_alloc_internal>:/,/^$/' > /tmp/opencode/arena-hotpath.asm
wc -l /tmp/opencode/arena-hotpath.asm
grep -cE '\block\b|\bxadd[q|l]\b|xchg' /tmp/opencode/arena-hotpath.asm   # atomics
grep -cE '\bcall\b' /tmp/opencode/arena-hotpath.asm                      # out-of-line calls
```

Host at capture time (2026-10-19): Linux x86_64, gcc (Ubuntu 13), Release
`-O2 -march=native`, lavapipe Vulkan runtime installed. Raw listing lives
in this repo's git history next to this document (Task D1 commit).

## Totals

| Metric | Value | Where |
|---|---|---|
| Lines of disassembly for `lgx_frame_alloc_internal` | 440 | `/tmp/opencode/arena-hotpath.asm` |
| `call` instructions (whole function) | 19 | `objdump -d`, counted by grep |
| `lock`-prefixed instructions | 0 | grep |
| `xadd` instructions | 0 | grep |
| `xchg` instructions | 1 — `xchg %ax,%ax` at `d62e`, a **NOP padding** after a `ret` chain, not an atomic | asm line 354 |

## The success path: d020–d129 (no calls, no locks)

The allocation success path enters at `d020` (function entry), checks
`g_frame_arena_state` initialized (`d026`, plain `cmpb`+`je`), computes the
16-byte alignment (`d059–d05d` ADD + AND), selects this thread's arena slot
by the `%3` size-class index math (`d06b–d093`, `imul $0x17e8` = 6088-byte
strides over `g_frame_arena_state`), performs the bounds check
(`d09c–d0a3`, `cmp`/`jb`), then on success:

| Offset | Instruction role |
|---|---|
| `d0bb` | store new bump pointer (`mov %rdx,0x20(%rax)`, plain store) |
| `d0bf` | increment arena allocation counter (`addq $0x1,0x30(%rax)`, plain) |
| `d0c4–d0d0` | rolling usage update (plain load/add/cmp/store) |
| `d0d3–d0da` | SIMD `vpaddq`/`vmovdqu` — the C1 counter pair (plain stores) |
| `d0db` | global allocation counter (`addq $0x1`, plain) |
| `d0e3–d0f4` | comparison-select of peak usage metric (cold-store only on new peak) |
| `d117` | `prefetcht0` next block (intentional, kept) |
| `d11b–d129` | epilogue: pops + `ret` |

**Measurement-relevant facts:**
- Zero `call` instructions execute on the success path; all 19 `call`s sit
  in the overflow/edge paths (`d138` onward).
- Zero locked instructions; every counter is a plain store to an already
  dirty cache line for a single thread's arena slot.
- No PLT stubs (`@plt`) on the success path — no library boundary is
  crossed on the hot path.
- The percentage trick (`0xaaaaaaab` multiply-shift at `d03d–d07f`) is the
  compiler's modulo-3 for the size-class index; it is arithmetic, not a call.

## Observer table (feeds D2)

| Observer | On success path? | Evidence (address) | D2 verdict |
|---|---|---|---|
| Per-arena stat counters (C1: per-slot alloc count, global count, usage sums, peak) | **YES** | plain stores `d0bf`, `d0c4–d0db`, SIMD pair `d0d3–d0da` | D2-3: keep — public API (`lgx_frame_get_stats`); D2 benchmark decides if the stores show at all ("measured, within noise" if not) |
| Next-block prefetch | YES | `prefetcht0` at `d117` | keep — intentional; removing it is measure-first, not assumed |
| `lgx_time_now_ns` (overflow-warning rate limiter) | NO | `call d19e`, reached from `d138` growth path only | no action (D1: not on hot path) |
| `lgx_telemetry_record_arena_overflow` | NO | `call d199` (overflow path) | no action |
| `lgx_heap_alloc` fallback | NO | `call d1c7` (arena exhausted) | no action |
| `__fprintf_chk` × 10 + `fwrite` × 4 (cap/warning prints) | NO | `d139`–`d379` region, overflow/growth paths | no action here; D2-1 handles the SAME printing in `lgx_frame_reset`, a different function |
| `malloc@plt` (chain-growth node, B3) | NO | `call d31a` (growth only) | no action |
| `allocate_huge_page_arena` | NO | `call d2af` (growth only) | no action |
| `munmap@plt` (retires a growth chain node, B3) | NO | `call d662`, reached only when a node is fully empty | no action |
| Rolling-average machinery (D2-2 subject) | NOT VISIBLE | `update_rolling_average` is called from `lgx_frame_reset` (source `:741`), not from alloc; the alloc path's usage math at `d0c4–d0f4` is the inlined version | D2-2 untouched by alloc evidence; D2-2 acts on reset |

Note `lgx_frame_reset` is a separate function and is NOT disassembled here;
D2-1 (warning block) and D2-2 (rolling loop) are judged on the reset
segment and the benchmark pair (both live in the reset path the benchmark
measures as FRAME_RESET).

## Verdict for D2-3 (recorded per stop rule)

The D1 evidence shows the C1 counter stores are plain stores to dirty
lines in code entirely within one cache-resident struct. The decision to
keep them is recorded here and is re-justified by the D2 benchmark deltas
(see the benchmark section of this file, written per D2 step).

## Benchmark section (D2: per-change record)

Methodology for every row: `taskset -c 2` (verified the binary pins itself to
CPU 2 via `BENCH_AFFINITY cpu=2`), 3 runs per state, median-of-3 of the
`FRAME_RESET_PATTERN` medians. The binary is
`/tmp/opencode/lgx-release/benchmarks/benchmark_benchmark_allocation_throughput`
(CMake names it `benchmark_benchmark_<source>`).

**Baseline (pre-D2, stderr still unmuted code present + mute workaround
in harness):**

```text
taskset -c 2 ./benchmark_benchmark_allocation_throughput   # 3x, 2026-10-19
FRAME_RESET_PATTERN size=16B  median=770ns (768..772)
FRAME_RESET_PATTERN size=64B  median=741ns (736..741)
FRAME_RESET_PATTERN size=256B median=810ns (801..814)
FRAME_RESET_PATTERN size=1KB  median=1307ns
FRAME_RESET_PATTERN size=4KB  median=3889ns
FLOOR_LINE bump_floor_16B median=82ns   (81..154, first run cold)
FLOOR_LINE bump_floor_64B median=85ns
FLOOR_LINE bump_floor_256B median=153ns
```
Run spread 0.5–2%; the baseline's absolute numbers remained invocation-
dependent because the runtime printed to the caller's stderr (that is the
defect D2-1 fixes).

**D2-1 (gate reset-frequency warnings behind `LGX_DEBUG_BUILD`; remove the
harness stderr-mute workaround):** kept.

```text
taskset -c 2 ./benchmark_benchmark_allocation_throughput   # 3x, post-D2-1
FRAME_RESET_PATTERN size=16B  median=493ns (491..493)
FRAME_RESET_PATTERN size=64B  median=476ns (475..476)
FRAME_RESET_PATTERN size=256B median=531ns (529..539)
FRAME_RESET_PATTERN size=1KB  median=1014ns
FRAME_RESET_PATTERN size=4KB  median=3669ns
```
Delta far outside run spread at every size; the invocation-dependence is
gone (stderr content is now a caller property, the timing path is quiet).
`verify_benchmark_methodology.sh` updated: it now FAILS if any stderr-mute
call exists in the harness (the runtime owns diagnostics gating; a mute
would hide future debug output and re-introduce invocation dependence).

**D2-2 (O(1) rolling-average insert instead of the 60-iteration loop):
kept.** Semantics preserved: eviction of the outgoing sample, insertion of
the incoming one, same division-by-window once per reset; zeroed on arena
init. `test_frame_arena_polish` green after the change.

```text
taskset -c 2 ./benchmark_benchmark_allocation_throughput   # 3x, post-D2-2
FRAME_RESET_PATTERN size=16B  median=468ns (459..469)
FRAME_RESET_PATTERN size=64B  median=453ns (452..458)
FRAME_RESET_PATTERN size=256B median=522ns (522..527)
FRAME_RESET_PATTERN size=1KB  median=1016ns (1004..1017)
FRAME_RESET_PATTERN size=4KB  median=3643ns (3622..3646)
```

**D2-3 (per-arena counters on the alloc success path): kept, measured.**
Throwaway build with the counter stores neutralized on the alloc success
path measured 432/441ns (16B), 426/440ns (64B), 487/497ns (256B) — a
consistent ~±5–6% drop at 16B (≈30ns), above run spread. Decision: KEEP
(option a) — the counters are public API (`lgx_frame_get_stats`,
`lgx_frame_arena_print_report`, health monitor consumption), and D2-3
forbids deleting asserted behaviour without changing tests first. The
honest note: eliminating them would buy ≈30ns at 16B; that difference is
smaller than the machine's full-run variance at 4KB and does not change
any ratio conclusion below. Revisit only with a config-flag design.

**D2-4 (`record_profiler_reset_event`): no action, evidenced.** The D2
disassembly of `lgx_frame_reset` (same Release tree) shows the helper
fully inlined: the only thing left of it on the reset path are the three
flag tests (`cmpb`s near `c1bd`/`c1e8`), and profiler calls
(`lgx_time_now_ns` + `write_chrome_trace_event` at `c2c0`/`c2e7`) are
reached only when a profiler is enabled. No out-of-line call exists.

**Stop-rule verdict (D2):** after D2-1/D2-2,
`lgx_frame_16B median 468ns` vs `bump_floor_16B median 82ns` = **>2×
ratio (5.7×)**. Per the Plan 01 stop rule the arena is slower than a
plain bump pointer by reasons the observer removals do not fully explain
(the measured counter cost is ~30ns of the ~386ns gap; the remaining
~350ns sits in the per-thread arena indexing, alignment math, bounds
check and fallback machinery recorded in the D1 listing). Recorded
honestly here and in D4's claims table; no further iteration without new
evidence. The "200× faster than malloc" claim stays retracted.
