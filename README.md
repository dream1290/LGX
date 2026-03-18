# LGX Runtime Platform

### The Standard ABI for Linux Gaming

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/platform-v2.0-green.svg)](https://github.com/dream1290/LGX/releases)
[![Platform](https://img.shields.io/badge/Linux-x86__64-lightgrey.svg)]()
[![Tests](https://img.shields.io/badge/tests-74%2F74%20passing-brightgreen.svg)]()

> **What DirectX is for Windows, LGX is for Linux.**
>
> A stable, high-performance, open-source gaming platform.  
> Build once. Run on every Linux distro. Guaranteed.

---

## The Problem

Game developers don't target Linux because there's no stable platform to build against. Ubuntu ≠ Fedora ≠ Arch. APIs break. Performance varies. Porting is a nightmare.

**LGX solves this.** One API. One ABI. Every distro. Period.

---

## Platform Modules

| Module | Version | Status | What It Does |
|--------|---------|--------|--------------|
| **Memory** | v1.0 | ✅ Production | Frame arena (0.01 μs), GPU pool, persistent heap, intent routing |
| **Threading** | v1.1 | ✅ Production | Job system, lock-free queues, fiber scheduler, MPMC/SPSC |
| **Graphics** | v1.2 | ✅ Production | Vulkan wrapper, command recording, pipeline management, shader cache |
| **Input** | v1.3 | ✅ Production | Unified gamepad, keyboard, mouse via evdev |
| **Audio** | v1.4 | ✅ Production | 3D spatialization, mixing, ALSA output |
| **Profiling** | v1.5 | ✅ Production | Frame profiler, zones, counters, FPS tracking |
| **Networking** | v2.0 | ✅ Production | UDP sockets, serialization, delta compression |
| **Asset Pipeline** | v2.1 | 🚧 Next | Asset loading, hot reloading, streaming |

Use only what you need. Start with memory for a 200× speedup. Add modules as you grow.

---

## Quick Start

```bash
# Install
git clone https://github.com/dream1290/LGX.git
cd LGX && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)
sudo make install
```

```c
#include <lgx_runtime.h>
#include <lgx_threading.h>
#include <lgx_graphics.h>
#include <lgx_input.h>
#include <lgx_audio.h>
#include <lgx_profile.h>
#include <lgx_net.h>

int main(void) {
    // Initialize platform
    lgx_runtime_config_t* cfg = lgx_config_create();
    lgx_config_set_flags(cfg, LGX_CONFIG_ENABLE_HUGE_PAGES);
    lgx_runtime_init(cfg);
    lgx_config_destroy(cfg);

    // Initialize GPU + Input
    lgx_gfx_device_t* gpu = lgx_gfx_device_create(NULL);
    lgx_in_system_t* input = lgx_in_create(NULL);
    lgx_aud_system_t* audio = lgx_aud_create(NULL);
    lgx_prof_context_t* prof = lgx_prof_create(NULL);

    // Game loop — frame allocations are FREE
    while (running) {
        lgx_prof_frame_begin(prof);
        lgx_in_poll(input);  // Poll input devices
        void* particles = lgx_alloc_frame(sizeof(Particle) * 10000);  // < 0.01 μs
        void* vertices  = lgx_alloc_frame(sizeof(Vertex) * 50000);

        update(particles);
        render(gpu, vertices);
        lgx_aud_update(audio);  // Mix and output audio
        lgx_prof_frame_end(prof);
        // No lgx_free() needed — frame arena resets automatically
    }

    lgx_prof_destroy(prof);
    lgx_aud_destroy(audio);
    lgx_in_destroy(input);
    lgx_gfx_device_destroy(gpu);
    lgx_runtime_shutdown();
}
```

```bash
gcc game.c -llgx_runtime -llgx_threading -llgx_graphics -llgx_input -llgx_audio -llgx_profile -llgx_net -lpthread -lvulkan -lasound -o game
```

---

## Performance

All targets exceeded. Not by a little — by **10–200×**.

| Operation | P99 Latency | Target | Margin |
|-----------|-------------|--------|--------|
| Frame allocation | 0.04 μs | < 0.1 μs | **2.5×** |
| Persistent heap | 0.09 μs | < 20 μs | **222×** |
| GPU pool | 5.1 μs | < 10 μs | **2×** |
| Initialize | 2.7 ms | < 500 ms | **185×** |
| Suspend/Resume | < 1 ms | < 100 ms | **100×** |

---

## Why LGX

### vs DirectX
- ✅ Open source (community-driven, not Microsoft-controlled)
- ✅ Vulkan-native (modern, not legacy D3D)
- ✅ Runs on any Linux distro

### vs Steam Runtime
- ✅ Complete platform (not just glibc + libs)
- ✅ Not Valve-controlled
- ✅ Lightweight (< 10 MB vs 200+ MB)

### vs Raw Vulkan + SDL
- ✅ Memory management (frame arena, GPU pool, intent routing)
- ✅ Threading (job system, fibers, lock-free structures)
- ✅ Observability (telemetry, counters, trace events)
- ✅ Stable ABI (version-guaranteed binary compatibility)

---

## Features

### Memory Management
- **Frame Arena**: Triple-buffered bump pointer — 80% of game allocations at < 0.1 μs
- **GPU Pool**: Pre-allocated Vulkan memory with buddy allocator
- **Persistent Heap**: Segregated fit + buddy, < 5% fragmentation over 8 hours
- **Intent-Based API**: Declare lifetime → auto-routes to optimal allocator

### Threading
- **Job System**: Work-stealing scheduler with dependency graphs
- **Lock-Free Queues**: MPMC and SPSC, cache-line aligned
- **Fiber System**: Cooperative user-space scheduling with fiber pool
- **Lock-Free Stack**: ABA-safe Treiber stack
- **Concurrent Hash Map**: Striped-lock FNV hash map

### Platform Services
- **Hardware Adaptation**: Auto-detect capabilities, graceful degradation, remediation guidance
- **Health Monitoring**: Continuous assessment with configurable alerts
- **Telemetry**: Separate process, privacy-preserving, < 0.1% overhead
- **Trace Events**: Integration with perf, Tracy, Valgrind
- **Structured Logging**: Subsystem-tagged with runtime filtering
- **Error Handling**: Recovery guidance with severity and actionable steps

### ABI Stability
- **Symbol versioning**: ELF `LGX_RUNTIME_1.0`, `LGX_THREADING_1.1`, `LGX_GRAPHICS_1.2`, `LGX_INPUT_1.3`, `LGX_AUDIO_1.4`, `LGX_PROFILE_1.5`, `LGX_NET_2.0`
- **Struct evolution**: `struct_size` first field, append-only
- **Binary compatibility**: v1.0 binary runs on v1.x runtime forever

---

## Documentation

| Document | Description |
|----------|-------------|
| [Platform Vision](LGX_PLATFORM_VISION.md) | Mission, strategy, roadmap |
| [Platform Architecture](LGX_PLATFORM_ARCHITECTURE.md) | Module design, API principles, ABI rules |
| [Runtime API Reference](docs/lgx_runtime_api.md) | All 60+ functions documented |
| [Threading API Reference](docs/lgx_threading_api.md) | Threading module API |
| [Graphics API Reference](docs/lgx_graphics_api.md) | Graphics module API (50+ functions) |
| [Input API Reference](docs/lgx_input_api.md) | Input module API (15 functions) |
| [Audio API Reference](docs/lgx_audio_api.md) | Audio module API (20 functions) |
| [Profiling API Reference](docs/lgx_profile_api.md) | Profiling module API (15 functions) |
| [Networking API Reference](docs/lgx_net_api.md) | Networking module API (25 functions) |
| [Integration Guide](docs/lgx_runtime_integration_guide.md) | Quick start, patterns, FAQ |
| [Architecture Deep-Dive](docs/lgx_runtime_architecture.md) | Memory subsystem, security, internals |

---

## Building

### Prerequisites
- Linux kernel 5.10+ (x86_64)
- GCC 11+ or Clang 14+
- CMake 3.16+
- pthread

### Build & Test
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
ctest --output-on-failure   # 74/74 tests pass
```

### Packages

```bash
# Debian/Ubuntu
sudo dpkg -i lgx-runtime_1.1.0_amd64.deb

# Fedora
sudo rpm -i lgx-runtime-1.1.0-1.x86_64.rpm

# Arch Linux
sudo pacman -U lgx-runtime-1.1.0-1-x86_64.pkg.tar.zst
```

### CMake Integration
```cmake
find_package(lgx_runtime 1.0 REQUIRED)
find_package(lgx_threading 1.1 REQUIRED)
target_link_libraries(my_game lgx_runtime::lgx_runtime lgx_threading::lgx_threading)
```

---

## Contributing

We welcome contributions. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Key areas needing help:**
- 🚧 **Asset Pipeline** (v2.1) — Asset loading, hot reloading, compression
- 📋 **Scene Graph** (v2.2) — Entity component system, transforms
- 📋 **Game integrations** — Port indie games to LGX
- 🧪 **Hardware testing** — Test on diverse GPU/CPU/gamepad configs

---

## Roadmap

**2026**: Core platform (memory ✅, threading ✅, graphics ✅, input ✅, audio ✅, profiling ✅, networking ✅)  
**2027**: Ecosystem (networking, asset pipeline, tooling, engine integrations)  
**2028**: Industry standard (studio adoption, distro defaults, "Powered by LGX")

See [LGX_PLATFORM_VISION.md](LGX_PLATFORM_VISION.md) for the full strategy.

---

## License

Apache License 2.0 — See [LICENSE](LICENSE)

Created by **Oualid Bahloul** and the LGX community.

**[GitHub](https://github.com/dream1290/LGX)** · **[Documentation](docs/)** · **[Issues](https://github.com/dream1290/LGX/issues)**
