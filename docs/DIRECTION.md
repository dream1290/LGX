# LGX — The Direction

*Where the project is going, the decisions that need making, and the paths from here to the stated goal.*

---

## 1. The Destination

The vision is settled and repeated across every strategy document:

> **LGX becomes the Standard ABI for Linux Gaming** — what DirectX is for Windows. An open-source, modular, stable-ABI platform that lets a developer target one API and run on every Linux distro.

The agreed road there is three phases:

| Phase | Window | Goal |
|---|---|---|
| 1 — Core Platform | 2026 | Nine modules built and production-ready (memory, threading, graphics, input, audio, profiling, net, asset, tooling) |
| 2 — Ecosystem | 2027 | Engine integrations (Godot, Unity), visual tooling, distro packages |
| 3 — Industry Standard | 2028 | "Powered by LGX" on storefronts, distros shipping by default, AAA adoption |

---

## 2. The Current Fork: How to Get the First 1000 Developers

The newest strategic document (`LGX_ARENA_SINGLE_HEADER_SPEC.md`, marked **CRITICAL — Strategic Pivot**) reframes the path. A complete platform is a hard sell; a single header file is not.

**The wedge strategy:**

```
Developer journey:
1. Discovers lgx_arena.h (5 minutes, zero config)
2. Integrates, sees 200× speedup (1 hour)
3. Shares with community (viral)
4. Wants more features (threading, GPU)
5. Discovers the LGX platform
6. Adopts the full platform

Tagline: "200× faster allocation for game loops. One header. Zero config."
Target:  100 stars in week 1, 1000 by week 12, then Valve/Steam Deck case study
```

The upsell path is the entire point: *prove value with the arena, then scale to the platform.*

---

## 3. The Immediate Path (Next 1–2 Weeks)

The repository cannot launch as it stands. Concretely, the sequence is:

### Step 1 — Repo hygiene (hours, not days)
- **Decide what the repo is**: recover or delete `LGX_ARENA_SINGLE_HEADER_SPEC.md` (currently staged-then-deleted)
- **Restore or replace** `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md` (deleted in working tree)
- **Untrack `build/`** and remove build artifacts from git; restore a proper `.gitignore` (build dirs, packages, security audits, scripts)
- **Fix CI to match reality** — either implement the arena it checks for, or revert the checks; restore lost jobs (formatting, static analysis) or consciously drop them
- Reconcile the version story (docs say 59/59, 64/64, 76/76 in different places)

### Step 2 — Finish the wedge: `lgx_arena.h` (the spec's 2-week plan)
1. **Extract** the frame arena from `src/runtime/lgx_frame_arena.c` into a single header — pure C99/POSIX, no LGX dependencies, auto-init
2. **Resolve the API collision first**: the spec's `lgx_alloc(size)` (auto-freed frame memory) would clash with the runtime's `lgx_alloc` (persistent malloc-backed). The handle-based API (`lgx_arena_create` / `lgx_frame_alloc(arena, …)`) used in the already-written root tests is safer; or namespace the arena API distinctly (`lgxa_*`)
3. **Wire up the existing test assets**: the 4 root test files (`test_basic.c`, `test_alignment.c`, `test_statistics.c`, `test_overflow.c`) + `test_framework.h` + `tests/Makefile` already exist and target exactly this header
4. **Prove the numbers**: 200× vs malloc, P99 < 0.1 μs, zero leaks (valgrind/ASan), the benchmark infrastructure already exists
5. **Launch materials already drafted**: `docs/LAUNCH_SOCIAL_MEDIA.md` (HN, Reddit, Twitter threads), `docs/LAUNCH_BLOG_POST.md` (SuperTuxKart integration story) — publish against the real repo URL (`dream1290/LGX`)

### Step 3 — Fix the code so the claims are true (the audit list from STATE.md)
Before "Production-Ready" is printed anywhere, in priority order:

| Priority | Fix |
|---|---|
| 1 | GPU pool use-after-free (store block index, not pointer) |
| 1 | Frame arena mid-frame growth invalidating outstanding pointers |
| 1 | Chaos testing mutex self-deadlock |
| 2 | Leak detector mutex wipe + size_t underflow |
| 2 | Intent allocator GPU routing order + non-Vulkan link failure |
| 2 | Memory manager leak-tracking comparison |
| 3 | Persistent heap slab accounting / TOCTOU on free |
| 3 | Health monitor uninitialized fields |
| 3 | Fiber scheduler stale ucontext + missing wake path |
| 3 | Net receive timeout never refreshing |
| 4 | Tools ring-buffer percentile wrap |
| 4 | Graphics NULL extension pointers |
| 4 | `lgx_runtime.h` `extern "C"` block |
| 4 | Integration header: implement or remove ~15 declared functions |
| 5 | Cosmetic: foreign doc-comment text, dead code, telemetry version string |

---

## 4. The Medium-Term Path (Weeks 3–12)

The go-to-market playbook (already written, `go-to-market-strategy.md` + `launch-plan.md`):

1. **Prove it works** — 5–10 real integrations (SuperTuxKart is the named target; also 0 A.D., Xonotic); publish honest benchmarks and the STK case study
2. **Build momentum** — 100–500 stars, Discord, newsletter, Reddit/HN presence, game jam sponsorship ($500–1000), Godot community outreach
3. **First revenue** — support contracts ($10K–$50K/yr) with the first paying customer within 60–90 days; the go/no-go bar is 500 stars + 10 integrations + 1 customer at month 3
4. **Platform upsell** — convert arena users into threading/graphics adopters; document the "one module at a time" upgrade path

**Watch items from the strategy's own honest assessment:**
- Business success probability is self-rated at ~40% (niche market, integration friction, incumbents)
- Performance claims will be challenged — the transparent-benchmarking policy must hold
- Valve building a competitor is the biggest external risk; open source + first-mover advantage is the stated defense

---

## 5. The Long-Term Path (2027+)

Per the vision and roadmap, conditional on the 90-day go/no-go:

1. **Ecosystem phase**: official Godot plugin (named: `lgx_gdx_profiler` GDExtension surfacing P95/P99 frame stats in-editor), Unity/Unreal plugin work, Ubuntu PPA + Fedora + Arch packages (deb already built)
2. **Complete Phase 2 of the STK story**: route hot-path allocations (`lgx_alloc_frame`) through the arena — the blog post projects +10–20% average FPS, +30–80% min FPS, −50% frame variance
3. **Evaluated options on the table**:
   - **3D/CAD expansion** (`docs/09-development/CAD_3D_EXPANSION_ANALYSIS.md`) — the codebase is assessed ~80% compatible with Blender/FreeCAD workloads; would need `lgx_math` (float64), `lgx_history` (undo/redo), streaming for >1GB meshes, CAD formats. Recommendation in-doc: dual-market positioning, gaming-first
   - **Steam Deck / SteamOS inclusion** — the arena spec's "Week 16 strategic win"
4. **Industry standard**: distros shipping LGX by default, "Powered by LGX" badge, AAA adoption

---

## 6. Decisions That Need Making Now

The direction cannot be executed until these forks are resolved:

1. **Arena API identity** — global thread-local API (`lgx_frame_begin`/`lgx_alloc`) vs. handle API (`lgx_arena_create`/`lgx_frame_alloc(arena,…)`)? The tests already written assume the handle API; the marketing copy assumes the global API.
2. **Separate repo or in-repo?** — the spec says a separate `lgx-arena` repo; the marketing copy references `github.com/yourusername/lgx-arena`. If it's a separate repo, `lgx_arena.h` must still land in this repo for the existing tests/CI.
3. **What gets fixed before launch vs. after** — the memory-safety items must be fixed before any public "production-ready" claim; the rest can be a public issue list (which itself builds credibility).
4. **CI scope** — restore the full quality pipeline (formatting, static analysis, audit scripts exist in `scripts/`) or keep the lean version?
5. **Story consistency** — one version number, one test count, one README. The three different test-count claims must converge.

---

## 7. The North Star

Whatever the immediate choices, the direction is fixed by the philosophy:

> **Prove value in 5 minutes with the arena. Earn adoption one module at a time. Build the platform that makes Linux gaming competitive with Windows — open where DirectX is closed, native where Proton is a layer, stable where the ecosystem is fragmented.**

**The next commit to make is the one that creates `lgx_arena.h`.**

---

*Companion documents: `docs/PHILOSOPHY.md` (why), `docs/CODEBASE.md` (what exists), `docs/STATE.md` (where it stands).*
