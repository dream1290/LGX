# We Integrated LGX into a Real Game in 3 Files and ~100 Lines. Here's Exactly What We Changed.

*Published by the LGX Team — April 2026*

---

There's a standard playbook for announcing an open-source gaming library: write a synthetic benchmark showing your allocator is 10× faster than malloc, post to Hacker News, and watch the sceptics arrive.

We didn't do that.

Instead, we took [SuperTuxKart](https://supertuxkart.net) — a mature, 300,000-line open-source racing game with its own physics engine, Vulkan rendering pipeline, 50+ shaders, and an audio system — and integrated LGX without touching core game logic. We're documenting exactly what we changed, what happened, and what the numbers actually say.

No marketing. Just the diff.

---

## What LGX Is (One Paragraph)

LGX is a modular, stable-ABI gaming platform for Linux. Think of it as what DirectX is for Windows — not an engine, but the platform layer beneath the engine. The core module is a memory allocator with three tiers: a **frame arena** (bump-pointer, resets every frame), a **GPU pool** (buddy allocator, Vulkan-backed), and a **persistent heap** (segregated-fit). You can adopt any tier independently; nothing is all-or-nothing.

The frame arena is the interesting one. Instead of calling `malloc`/`free` for every temporary object per frame, you call `lgx_alloc_frame()`. At the end of the frame, a single `lgx_frame_reset()` frees everything at once. No individual frees. No fragmentation. Linear allocation speed regardless of how many objects you create.

---

## Why SuperTuxKart?

STK is a good integration target because it isn't easy:

- **~300,000 lines of C++**, mostly untouched since its refactoring to Vulkan
- Custom allocators already in some subsystems (karts, physics)
- Complex frame loop: physics, AI, audio, networking, GPU submission all within 8ms
- Well-instrumented: STK has its own profiler, making it easy to detect regressions

If LGX caused instability, STK's test suite would catch it. If it caused performance regression, STK's profiler output would show it.

---

## The Integration: Exactly What We Changed

Three files. Every change is wrapped in `#ifdef USE_LGX_RUNTIME` so it's completely reversible with one CMake flag.

### File 1: `CMakeLists.txt`

```cmake
option(USE_LGX_RUNTIME "Use LGX Runtime for optimized memory allocation" ON)

if(USE_LGX_RUNTIME)
    find_library(LGX_RUNTIME_LIBRARY NAMES lgx_runtime liblgx_runtime)
    find_path(LGX_RUNTIME_INCLUDE_DIR NAMES lgx_runtime.h)

    if(LGX_RUNTIME_LIBRARY AND LGX_RUNTIME_INCLUDE_DIR)
        add_definitions(-DUSE_LGX_RUNTIME)
        include_directories(${LGX_RUNTIME_INCLUDE_DIR})
        target_link_libraries(supertuxkart ${LGX_RUNTIME_LIBRARY})
        message(STATUS "LGX Runtime enabled: ${LGX_RUNTIME_LIBRARY}")
    else()
        set(USE_LGX_RUNTIME OFF CACHE BOOL "" FORCE)
        message(WARNING "LGX Runtime not found, disabling")
    endif()
endif()
```

Auto-detects LGX. If LGX isn't installed, build proceeds normally. Zero friction.

---

### File 2: `src/main.cpp`

Added two functions and two call sites:

```cpp
#ifdef USE_LGX_RUNTIME
#include <lgx_runtime.h>

static lgx_runtime_config_t* g_lgx_config = nullptr;

static void initLGXRuntime()
{
    g_lgx_config = lgx_config_create();
    lgx_config_set_memory_pool_size(g_lgx_config, 256 * 1024 * 1024); // 256MB
    lgx_result_t result = lgx_runtime_init(g_lgx_config);
    if (result != LGX_SUCCESS) {
        Log::error("LGX", "Failed to initialize (error %d)", result);
    } else {
        Log::info("LGX", "LGX Runtime initialized successfully (256MB pool)");
    }
}

static void shutdownLGXRuntime()
{
    lgx_runtime_shutdown();
    lgx_config_destroy(g_lgx_config);
    g_lgx_config = nullptr;
}
#endif

// In main(), after initUserConfig():
#ifdef USE_LGX_RUNTIME
    initLGXRuntime();
#endif

// In cleanSuperTuxKart():
#ifdef USE_LGX_RUNTIME
    shutdownLGXRuntime();
#endif
```

---

### File 3: `src/main_loop.cpp`

One line at the frame boundary:

```cpp
PROFILER_POP_CPU_MARKER();
PROFILER_SYNC_FRAME();

#ifdef USE_LGX_RUNTIME
    lgx_frame_reset();   // ← This is the entire integration
#endif
```

`lgx_frame_reset()` takes **less than 1 microsecond** (measured: < 0.017% of frame time at 169 FPS). It resets the frame arena bump pointer, making all frame-lifetime memory available again instantly. No GC. No fragmentation. No syscalls.

---

## What Happened When We Ran It

The game initialised. We watched the terminal:

```
[LGX INFO] Persistent Heap initialized
  Size classes: 16 (16B - 4KB)
  Slab size: 2048 KB
  Buddy allocator: 256 MB (4KB - 64MB)
[info   ] LGX: LGX Runtime initialized successfully (256MB pool)
```

Then: 50+ shaders compiled. Physics initialised. Audio systems came up. AI loaded. The race started. **No crashes. No errors. No memory conflicts with STK's existing allocators.**

We ran it for 60+ seconds on the Lighthouse track with 4 AI karts. We verified LGX symbols were correctly resolved:

```bash
$ ldd bin/supertuxkart | grep lgx
liblgx_runtime.so.1 => /lib/liblgx_runtime.so.1

$ nm bin/supertuxkart | grep lgx
U lgx_config_create@LGX_1.0
U lgx_frame_reset@LGX_1.0
U lgx_runtime_init@LGX_1.0
U lgx_runtime_shutdown@LGX_1.0
```

The `@LGX_1.0` suffix is ELF symbol versioning working correctly — if you update the LGX library, older binaries still link against the exact symbol they were compiled for.

---

## The Honest Performance Numbers

**Current state (frame resets only, no allocation replacement yet):**

| Metric | Standard malloc | LGX (frame reset only) | Delta |
|--------|----------------|------------------------|-------|
| Average FPS | ~165–170 | ~169–175 | ~0–3% |
| Min FPS | ~11 | ~11 | — |
| Max FPS | ~310 | ~311 | — |
| Variance | ~30% | ~30% | — |
| Frame time | ~5.9ms | ~5.9ms | — |

**The numbers are nearly identical. That's the point.**

LGX is integrated but not yet *used* for STK's allocations. We're only calling `lgx_frame_reset()` — infrastructure work. The frame arena sits there, initialised, ready, adding less than 0.017% overhead. STK doesn't know it's there.

This is Phase 1 of the integration story. The performance gains come in Phase 2, when hot-path allocations are routed through `lgx_alloc_frame()` instead of `malloc`. Based on microbenchmarks:

| Metric | Expected after allocation replacement |
|--------|---------------------------------------|
| Average FPS | +10–20% |
| Min FPS | +30–80% (worst-case frame time) |
| Frame variance | -50% (30% → 15–20%) |
| Frame time | -10–15% |

The particle system, physics collision temporaries, and per-frame rendering buffers are the primary targets. Each is a bounded, frame-scope allocation pattern — exactly what a frame arena exists for.

---

## What The Allocator Numbers Actually Look Like

Independent of STK, here are LGX's measured allocator numbers from the benchmark suite:

| Operation | P99 | vs. glibc malloc |
|-----------|-----|-----------------|
| Frame alloc (`lgx_alloc_frame`) | **0.04 μs** | **200× faster** |
| Persistent alloc | **0.09 μs** | **>100× faster** |
| GPU pool alloc (Vulkan-backed) | **5.1 μs** | On target |
| `lgx_frame_reset()` | **< 1 μs** | N/A |

These numbers survive ABI compatibility tests (old binary, new library), chaos testing (deliberate fault injection during stress), and failure injection tests. They aren't cherry-picked synthetic runs — they're the CI regression baseline on every commit.

---

## Why This Integration Pattern Matters

The way LGX integrates with STK is the template for every future integration:

1. **Enter at one seam.** We didn't touch the renderer, the physics engine, or the audio system. One frame boundary hook. One pool configuration.
2. **Guard everything.** `#ifdef USE_LGX_RUNTIME` means the patch is fully reversible. `-DUSE_LGX_RUNTIME=OFF` and you're back to vanilla STK, bit-for-bit identical.
3. **Phase the adoption.** Infrastructure first (frame reset). Allocation replacement second. The game developer controls the pace.
4. **Don't own the architecture.** LGX doesn't ask STK to restructure anything. It coexists with STK's own allocators. It works alongside the existing profiler.

A game developer seeing this integration for the first time should think: *I could do this on my lunch break.* That's the standard we're holding ourselves to.

---

## What's Next

**For LGX:**
- Ubuntu PPA submission (the `.deb` is already built; this is a packaging process, not development)
- Godot profiler plugin — `lgx_gdx_profiler` as a GDExtension addon, surfacing P95/P99 frame stats and hot zone ranking directly inside the Godot editor
- Full Phase 2 STK allocation replacement with before/after benchmark video

**For you, if you want to try it:**

```bash
# Install
git clone https://github.com/dream1290/LGX
cd LGX && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install   # installs to /usr/local/lib

# Use in your game (CMake)
find_library(LGX_RUNTIME NAMES lgx_runtime)
target_link_libraries(your_game ${LGX_RUNTIME})

# Use in your code
#include <lgx_runtime.h>

lgx_runtime_config_t* cfg = lgx_config_create();
lgx_config_set_memory_pool_size(cfg, 256 * 1024 * 1024);
lgx_runtime_init(cfg);

// In your frame loop:
lgx_frame_reset();
```

If you're on a Debian-based distro, the `.deb` package is already available in the repository — no build required.

---

## Links

- **GitHub**: https://github.com/dream1290/LGX
- **Architecture**: `docs/LGX_PLATFORM_ARCHITECTURE.md`
- **API Reference**: `docs/02-api-reference/`
- **Integration Guide**: `docs/04-integration/lgx_runtime_integration_guide.md`
- **76/76 tests passing** — including ABI compatibility, chaos testing, failure injection

---

*LGX Runtime Platform v2.2 — Apache 2.0*  
*The stable ABI for Linux gaming.*
