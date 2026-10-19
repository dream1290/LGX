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

Filled per D2 step, each with the exact commands (both before and after),
the raw per-run prints, run-to-run spread, and the keep/revert verdict.
