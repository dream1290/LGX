# LGX — What Next

This document answers "what do we do next?" based on the verified state of the codebase. It assumes you have read `docs/PLATFORM_VIEW.md` and `docs/AUDIT_REPORT.md`.

## The Short Answer

**Finish Plan 01 before anything else.** The arena, threading, graphics, audio, net, and asset modules exist as headers and stubs. The runtime core has 3 known memory-safety bugs. Until those are fixed and tested, the platform has no foundation to build on.

## Immediate Actions (This Week)

| Priority | Task | Why | Time |
|----------|------|-----|------|
| 1 | Fix GPU pool use-after-free | Critical memory-safety bug — handles dangle after buddy array `realloc` | 2–4 hours |
| 2 | Fix frame-arena mid-frame growth | `grow_arena` unmaps old base while live pointers exist | 4–6 hours |
| 3 | Fix chaos-testing self-deadlock | Chaos testing is currently unusable — it deadlocks | 1–2 hours |
| 4 | Fix intent router GPU routing | `lgx_alloc_gpu_shared()` never reaches GPU pool | 1 hour |
| 5 | Fix non-Vulkan link | `lgx_gpu_alloc` called without `#if HAVE_VULKAN` guard | 30 minutes |
| 6 | Fix net receive timeout | `last_recv_ms` never refreshed; every connection times out | 1 hour |
| 7 | Write regression tests for 1–6 | Each fix must have a test that fails first | 1–2 days |
| 8 | Push to `origin/main` | 24 commits ahead; `main` and `origin/main` diverge | 5 minutes |

## Next Week (After Critical Fixes)

| Task | Why | Time |
|------|-----|------|
| Run fragmentation matrix on Debian 12, Fedora 40, Arch, Ubuntu 24.04 | Determines if distro differences matter for this codebase | 1–2 days |
| Profile arena hot path | Find the ~20 ns/unalloc between bump floor (~3 ns) and current (~24 ns) | 1–2 days |
| Add mixed-size, many-live-objects benchmark workload | Current benchmark is glibc's best case (same-size alloc/free tight loop); doesn't test what arena is for | 4 hours |
| Decide: library vs platform | Based on matrix + profiling results | 1 day discussion |

## After That (Month 2)

Based on Phase 1 + 2 results, one of two paths:

### Path A: Library (Most Likely Based on Current Data)
- Focus on memory subsystem excellence
- Document threading/graphics/etc. as "not yet implemented" — do not ship half-built modules
- Market as a fast, stable memory allocator for Linux games
- Target: games that need predictable frame-time memory

### Path B: Platform (Only If Data Supports It)
- If fragmentation matrix shows major distro differences AND arena performance is competitive
- Build threading, graphics, input as real modules
- Target: game engines that want a stable Linux ABI

## What NOT to Do Next

| Don't | Why |
|-------|-----|
| Build graphics module | Vulkan wrapper exists as stub; no validated need yet |
| Build input module | SDL3 already exists and is maintained by Valve |
| Build audio module | PipeWire/PipeWire is the Linux audio standard |
| Add networking | ENet and GameNetworkingSockets already exist |
| Claim "200× faster than malloc" | Refuted by measurement; see README retraction |
| Claim "production-ready" | 3 known memory-safety bugs, 26 tests that can't fail |

## The One Command to Run Today

If you only do one thing:

```bash
# Fix the GPU pool use-after-free — it's the most dangerous bug
# (handles dangle after buddy array realloc)
git checkout -b fix/gpu-pool-use-after-free
# Fix src/runtime/lgx_gpu_pool.c: store block index, not pointer
# Add regression test in tests/phase0/test_gpu_pool_handle_safety.c
# Verify with ASan: cmake -B build -DCMAKE_BUILD_TYPE=Debug && ctest
```

Then push `main` to GitHub so the CI fixes and README rewrite are actually available to anyone cloning the repo.

---

**Bottom line:** LGX is a memory allocator with a platform-shaped hole around it. Fix the memory core, measure it honestly, then decide whether to build a platform or ship a library. Do not build modules on top of a foundation that has known memory-safety bugs.