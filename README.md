# LGX

A memory runtime for Linux games.

Most of a frame's allocations are temporary — particles, physics temporaries,
per-frame buffers. They live for one frame and die. `malloc` is built for a much
harder problem than that, and you pay for the difference in frame time.

LGX gives you an arena: allocate by advancing a pointer, reclaim the whole frame
at once.

```c
#include <lgx_runtime.h>

while (running) {
    Particle* particles = lgx_alloc_frame(sizeof(Particle) * 4096);
    simulate(particles, 4096);

    // next frame: the arena is reset, nothing is freed one-by-one
}
```

No `free()` calls. No fragmentation across frames. No allocation bookkeeping in
your game loop.

---

## Install

```bash
git clone https://github.com/dream1290/LGX.git
cd LGX
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

Then link against it:

```bash
gcc game.c -o game $(pkg-config --cflags --libs lgx_runtime)
```

Or from CMake, using what the install actually provides:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LGX_RUNTIME REQUIRED IMPORTED_TARGET lgx_runtime)
target_link_libraries(my_game PRIVATE PkgConfig::LGX_RUNTIME)
```

Requires Linux 5.10+ on x86_64, CMake 3.16+, GCC 11+ or Clang 10+. Vulkan and
ALSA are optional — without them the GPU pool and audio module are skipped and
everything else still builds.

---

## What's in it

**Memory (v1.0)** — the core, and the only module most projects need.

- **Frame arena** — bump-pointer allocation, reset per frame. Three rotating
  buffers, so the GPU can still be reading frame N-1 while you write frame N.
- **Persistent heap** — segregated-fit plus buddy, for data that outlives a frame.
- **GPU pool** — Vulkan-backed buddy allocator for device-visible memory.
- **Intent-based routing** — declare how long something lives and the allocator
  picks the right pool:

```c
lgx_alloc_frame(n);        // frame-scoped  → frame arena
lgx_alloc_persistent(n);   // long-lived    → persistent heap
lgx_alloc_gpu_shared(n);   // CPU+GPU       → GPU pool
```

**Threading (v1.1)** — work-stealing job system, lock-free MPMC/SPSC queues,
fibers, contention diagnostics.

**Graphics (v1.2) · Input (v1.3) · Audio (v1.4) · Profiling (v1.5) ·
Networking (v2.0) · Assets (v2.1) · Tooling (v2.2)** — separate libraries,
each usable on its own. See [`docs/LGX_PLATFORM_ARCHITECTURE.md`](docs/LGX_PLATFORM_ARCHITECTURE.md).

---

## What it costs

Measured with the benchmark in this repository, pinned to one CPU, with both
sides actually writing their memory. Full data with provenance:
[`benchmarks/baseline_frame_arena.txt`](benchmarks/baseline_frame_arena.txt).

Per allocation, median:

| Size | Frame arena | glibc malloc | Bare bump pointer |
|------|-------------|--------------|-------------------|
| 16B  | 26 ns | 65 ns | 2 ns |
| 64B  | 26 ns | 34 ns | 2 ns |
| 256B | 29 ns | 35 ns | 4 ns |
| 1KB  | 39 ns | — | 24 ns |
| 4KB  | 116 ns | — | 104 ns |

Two honest caveats on this table:

- The three columns are **not the same workload**. `malloc` is measured as a
  single alloc/free pair; the arena and the bare bump pointer allocate 64 times
  per frame and reset. They are close enough to compare at a glance, not close
  enough to compute a ratio from.
- `malloc` has no 1KB/4KB row because those sizes route to a different glibc
  path; we did not measure them.

Read the shape of it: the arena is in the same order of magnitude as glibc
`malloc`, roughly 2–3× slower than it. It is **not faster**, and we do not claim
it is.

The last column is the point. A bare bump pointer — offset advance and nothing
else — is another 5–10× cheaper. LGX currently sits between the two, paying
roughly 13–25 ns per allocation for instrumentation and safety checks on top of
the bare pointer. Locating that overhead is
[Plan 01, Task 7](docs/superpowers/plans/2026-10-11-correctness-and-arena-performance.md).

So what does the arena buy you? Not raw latency:

- **No `free()` per allocation.** Reclamation is one pointer reset per frame.
- **No fragmentation across frames.** The memory is reused, not reallocated.
- **Stable addresses within a frame**, so structures can point at each other
  freely.
- **No per-allocation bookkeeping in your loop**, even when allocation stats,
  call-site tracking, and overflow detection are compiled in.

If your workload is dominated by glibc's fastbin/tcache path, `malloc` is fine
and this library is not for you. If your frame does thousands of allocations
that all die together, the arena is the right shape — and we will keep measuring
until it earns the rest of the gap.

Raw benchmark data, with provenance and reproducibility commands:
[`benchmarks/baseline_frame_arena.txt`](benchmarks/baseline_frame_arena.txt).

---

## A retraction

Earlier versions of this README claimed the frame arena was **200× faster than
malloc**. That number was wrong, and we retract it here rather than quietly
editing it.

It came from comparing LGX against an asserted 10–30 μs `malloc` cost. The
project's own benchmark measured glibc `malloc` at ~20 ns for a small hot
allocation — roughly a thousand times cheaper than the figure the claim was
built on. The "200×" was a ratio against a baseline we had not actually
measured.

Everything on this page is reproducible with:

```bash
./build/benchmarks/benchmark_benchmark_allocation_throughput
```

---

## Status

**v1.0.1.** Builds clean from a fresh checkout with zero warnings under
`-Wall -Wextra -Werror`, and the public header compiles and links from both C11
and C++17.

Honest inventory of what is and isn't done:

| | |
|---|---|
| Clean build, no warnings | ✅ |
| C++ consumability gate | ✅ |
| 76 registered tests | ✅ — but see below |
| Tests that can actually fail | **26 of 77 registered tests cannot report failure** |
| Memory safety | ❌ 3 known defects, tracked in [Plan 01](docs/superpowers/plans/README.md) |
| CI | ✅ 3 jobs on every push |

The 26 are tests whose only failure mechanism is `assert()`, which
`-DNDEBUG` compiles out of every Release build. We would rather tell you than
have you find it. CI enforces a ratchet so that number can only go down.

Three memory-safety defects are known and located: a use-after-free in the GPU
pool's handle bookkeeping, pointer invalidation when the frame arena grows
mid-frame, and a self-deadlock in chaos testing. Fixes and regression tests are
written up in [Plan 01](docs/superpowers/plans/2026-10-11-correctness-and-arena-performance.md).
We are not claiming production readiness until they are closed.

---

## Development

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug   # ASan + UBSan
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

```bash
scripts/verify_test_integrity.sh     # tests that cannot fail are a defect
scripts/count_tests.sh build         # registered / verifying / incapable
scripts/verify_benchmark_methodology.sh
```

Each of these is a CI gate, not a convenience. They fail closed.

Remediation plans, in dependency order, live in
[`docs/superpowers/plans/`](docs/superpowers/plans/README.md).

---

## Documentation

- [Platform architecture](docs/LGX_PLATFORM_ARCHITECTURE.md) — module contracts, ABI rules
- [API reference](docs/02-api-reference/) — functions, types, error handling
- [Integration guide](docs/04-integration/) — CMake, pkg-config, build integration
- [Remediation plans](docs/superpowers/plans/README.md) — what is broken and the order to fix it
- [Audit report](docs/AUDIT_REPORT.md) — the independent review that started this

---

## License

Apache 2.0 — see [LICENSE](LICENSE).