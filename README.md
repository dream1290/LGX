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

## Performance: every claim, its status, its evidence

Nothing on this page claims anything the harness does not print. The snapshot
is in
[`docs/05-performance/arena-hotpath-profile.md`](docs/05-performance/arena-hotpath-profile.md)
("Final measurement") and duplicately in
[`benchmarks/baseline_frame_arena.txt`](benchmarks/baseline_frame_arena.txt);
`scripts/verify_arena_ratios.sh` fails CI when a fresh run deviates from that
snapshot or the size ladder grows non-linearly.

| Claim | Status | Evidence (command) | Commit |
|---|---|---|---|
| "200× faster than malloc" | **retracted** | refuted by our own benchmark (fractions carved from an asserted, never-measured malloc baseline) | [`c7147d4`](https://github.com/dream1290/LGX/commit/c7147d4) |
| Frame arena, 16B, is 5.7× slower than a bare bump pointer (468ns vs 82ns per frame) | measured | same command, `FRAME_RESET_PATTERN size=16B` vs `FLOOR_LINE bump_floor_16B` | [`54167d2`](https://github.com/dream1290/LGX/commit/54167d2) |
| Frame arena runs a whole game-frame pattern (64 mixed-size objects) 1.27× slower than glibc malloc, same run | measured | same command, `MIXED_RATIO` line | [`f839501`](https://github.com/dream1290/LGX/commit/f839501) |
| GPU pool: all four memory-safety defects fixed, owned memory returned | fixed | `ctest -R test_gpu_pool` in a Vulkan build | [`4bdb849`](https://github.com/dream1290/LGX/commit/4bdb849) |
| Non-Vulkan build | builds and tests | `no-vulkan` CI job | [`aa747eb`](https://github.com/dream1290/LGX/commit/aa747eb) |

Every command above runs today from a fresh clone and prints the cited number
(shown as of the snapshot date; bases move, so re-read your own run rather than
quoting history). No row says "production-ready".

So what does the arena buy you? Not raw latency per allocation:

- **No `free()` per allocation.** Reclamation is one pointer reset per frame.
- **No fragmentation across frames.** The memory is reused, not reallocated.
- **Stable addresses within a frame**, so structures can point at each other
  freely.
- **No per-allocation bookkeeping in your loop**, even when allocation stats,
  call-site tracking, and overflow detection are compiled in.

If your workload is dominated by glibc's fastbin/tcache path, `malloc` is fine
and this library is not for you. If your frame does thousands of allocations
that all die together, the arena is the right shape — and the tables above say
plainly at what latency.

```bash
./build/benchmarks/benchmark_benchmark_allocation_throughput
```

---

## Status

**v1.0.1.** Builds clean from a fresh checkout with zero warnings under
`-Wall -Wextra -Werror`, and the public header compiles and links from both C11
and C++17.

Honest inventory of what is and isn't done (census: fresh clone, 2026-10-19,
`scripts/count_tests.sh build`):

| | |
|---|---|
| Clean build, no warnings | ✅ |
| C++ consumability gate | ✅ |
| 82 registered tests / 81/82 pass in RelWithDebInfo | ✅ — the 1 failure is the known `test_telemetry_anonymization` NDEBUG-only assert gap (allowlist) |
| Tests that can actually fail | **26 of 82 registered tests cannot report failure** (NDEBUG assert-only; Plan 03's backlog) |
| Memory safety | ✅ the 14 defects from the completion plan are fixed on `fix/phase1-safety` (GPU pool ×4 + intent routing, frame-arena growth, chaos self-deadlock, net recv timeout, leak-detector init, adapter status, usage stats), each with a test that failed before the fix |
| CI | ✅ 3 jobs on every push (build-and-test incl. the arena ratio gate, asan-ubsan, no-vulkan) |

The 26 are tests whose only failure mechanism is `assert()`, which
`-DNDEBUG` compiles out of every Release build. We would rather tell you than
have you find it. CI enforces a ratchet so that number can only go down.

Those defects and their fixes are written up in
[`2026-10-18-phase1-completion.md`](docs/superpowers/plans/2026-10-18-phase1-completion.md),
which superseded Plan 01 and was executed on `fix/phase1-safety` (2026-10-19).
We are still not claiming production readiness.

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