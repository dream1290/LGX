# LGX — The Philosophy

*What this project believes, why it exists, and the principles it refuses to compromise.*

---

## 1. The Mission

> **Displace Windows gaming dominance by providing a stable, high-performance, open-source gaming platform for Linux.**

The tagline that captures it: **"What DirectX is for Windows, LGX is for Linux."**

LGX is not a library. Not an allocator. Not a tool. It is an attempt to build the missing platform layer of the Linux gaming ecosystem — the layer Windows has had for 30 years and Linux has never had:

| Windows | Linux Today | Linux With LGX |
|---------|-------------|----------------|
| DirectX | Chaos (Vulkan + random libs) | LGX Runtime |
| Stable ABI | Fragmented | Stable ABI |
| One target | Many targets | One target |
| Guaranteed performance | Varies wildly | Guaranteed |

---

## 2. The Problem Being Solved

Linux gaming's fundamental problem is **fragmentation**. Developers don't target "Linux" — they target Ubuntu 22.04 and hope it works elsewhere.

```
Windows game development:
  #include <d3d11.h>     ← one API
  Target: Windows        ← one target
  Result: ships on time  ← predictable

Linux game development:
  #include <vulkan.h>    ← which version?
  #include <SDL2.h>      ← Ubuntu has 2.0.10, Arch has 2.28
  #include <???_audio.h> ← ALSA? PulseAudio? PipeWire?
  Target: "Linux"        ← 50+ distros, 10+ kernels
  Result: delayed port   ← constant firefighting
```

The numbers: **96% of Steam games target Windows first**, fewer than **25% ship a native Linux port**, and the #1 reason cited by developers is *"Linux is fragmented and unpredictable."*

### Why existing solutions fail

| Solution | The Flaw |
|----------|----------|
| **Proton/Wine** | Compatibility layer, not native performance. Permanent catch-up. |
| **Steam Runtime** | Minimal (just glibc/libs). Valve-controlled. Not a development platform. |
| **SDL** | Thin abstraction for input/window. No memory, threading, or audio engine. |
| **Raw Vulkan** | Low-level graphics only. No platform services. Fragmented ecosystem. |

**No one offers what DirectX offers: a complete, stable, high-performance gaming platform.**

---

## 3. The Core Principles

Five beliefs shape every decision in this project:

### 1. Stable ABI is everything
Games compiled against v1.0 run on v1.x forever. Symbol versioning (`LGX_1.0`, `LGX_1.1`), `struct_size`-first structs, append-only evolution, never remove a function, never change a signature. A platform you can't build against in confidence is not a platform.

### 2. Specialized for gaming, not general-purpose
Every API is designed for real-time workloads: frame arenas (< 0.1 μs), lock-free job systems, zero-copy GPU memory, intent-based allocation routing. LGX does not try to be a general-purpose library — that's what makes it fast.

### 3. Modular and incremental
Games adopt what they need: just memory (v1.0), then threading (v1.1), graphics (v1.2), and so on. No all-or-nothing commitment. Each module is an independent shared library with a well-defined contract.

### 4. Open source and community-owned
Apache 2.0. Cannot be killed or controlled by one company — unlike DirectX (Microsoft) or Steam Runtime (Valve). Every distro can ship it; every developer can contribute.

### 5. Native Linux, not a compatibility layer
Built on Vulkan, pthreads, io_uring, PipeWire. LGX leverages Linux strengths instead of hiding them. No Windows compatibility layer → no overhead → faster than DirectX on equivalent workloads.

---

## 4. The Design Ethos (How the Code Thinks)

The architecture spec (`docs/LGX_PLATFORM_ARCHITECTURE.md`) encodes a consistent design language across every module:

- **Consistent naming**: `lgx_<module>_<action>()` — `lgx_alloc_frame`, `lgx_th_job_submit`, `lgx_gfx_cmd_begin`.
- **Opaque handles**: create/destroy patterns, NULL-safe destroy, thread-safe unless documented otherwise.
- **One error system**: `lgx_result_t` + error context with severity, suggested action, and recovery steps — everywhere.
- **Struct versioning**: `struct_size` as the first field of every public struct so old binaries safely ignore new fields.
- **Performance budgets**: every module has a hard P99 budget (frame alloc < 0.1 μs, persistent alloc < 20 μs, GPU alloc < 10 μs). *"No module may exceed its budget. If it does, it's a blocker for release."*
- **Security is production**, not a phase: guard pages, canaries, input validation, chaos testing, failure injection, leak detection.

---

## 5. The Strategy Journey (How the Philosophy Evolved)

The project's thinking went through three distinct phases:

### Phase A — "The Allocator" (the wrong frame)
LGX began as a memory allocator pitched as "1000× faster than malloc." The strategy docs candidly record this as the wrong framing: *too narrow, wrong comparison.*

### Phase B — "The Platform" (the right frame)
The pivot: *"You're not building an allocator. You're building a PLATFORM."* LGX became "The Standard ABI for Linux Gaming," a nine-module platform (memory → threading → graphics → input → audio → profiling → networking → asset pipeline → tooling), with a three-phase roadmap:

- **Phase 1 (2026): Core Platform** — all nine modules, production.
- **Phase 2 (2027): Ecosystem** — engine integrations (Godot, Unity), visual tooling.
- **Phase 3 (2028): Industry Standard** — "Powered by LGX" on storefronts, distros shipping by default.

### Phase C — "The Wedge" (the entry point)
The most recent strategic turn (the staged `LGX_ARENA_SINGLE_HEADER_SPEC.md`, marked **CRITICAL — Strategic Pivot**): a full platform is a hard sell, so extract the frame arena into an stb-style single-header library (`lgx_arena.h`) that proves value in 5 minutes. The philosophy: *"Not by building the platform first and hoping developers come. But by proving value with the arena, then scaling to the platform."*

---

## 6. The Business Philosophy

From `go-to-market-strategy.md` — the project is honest about its economics:

- **Open source the commodity, monetize the differentiation**: Apache 2.0 core; commercial licenses ($10K–$200K/yr), support contracts, custom development, and training as revenue streams.
- **Keep private**: security audit reports, internal strategy, pricing, customer lists.
- **Honest SWOT**: technical success is rated 95%, adoption 60%, business 40%. The target is the *5–10% of developers for whom performance is the difference between success and failure* — performance-critical games, Linux-first developers, live-service games, indie hits.
- **Go/no-go discipline**: Month 3 must show 500 stars + 10 integrations + 1 customer; Month 12 must show $100K ARR "or consider pivot."

---

## 7. What the Philosophy Demands Next

The current moment is defined by the gap between the vision and the entry point:

- The **platform** exists on paper and largely in code (nine modules, 76 registered tests).
- The **entry point** does not exist yet: `lgx_arena.h` is specified, tested-for, and marketed — but never implemented.
- The **repository** is not yet launch-ready (build artifacts tracked, docs deleted, CI simplified, security posture in flux).

The philosophy says: finish the wedge, make the platform real for a first developer, then let the platform sell itself.

---

## 8. In One Paragraph

LGX exists because Linux gaming lacks what Windows took for granted: a stable, complete, high-performance platform that developers can target with confidence. It is built to be the DirectX of Linux — open where DirectX is closed, native where Proton is a layer, modular where Steam Runtime is monolithic — and its strategy is to earn adoption one module at a time, starting with a single header file that makes every game's allocations 200× faster.
