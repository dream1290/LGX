# LGX Runtime — Integration Guide

> Get your game running on LGX in under 2 hours.

## Quick Start

### 1. Install

```bash
# Ubuntu/Debian
sudo apt install libgx-runtime-dev

# Fedora
sudo dnf install lgx-runtime-devel

# Arch Linux
sudo pacman -S lgx-runtime

# From source
git clone https://github.com/dream1290/LGX.git
cd LGX && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && sudo make install
```

### 2. Link

**CMake** (recommended):
```cmake
find_package(lgx_runtime 1.0 REQUIRED)
target_link_libraries(my_game lgx_runtime::lgx_runtime)
```

**pkg-config**:
```bash
gcc my_game.c $(pkg-config --cflags --libs lgx_runtime) -o my_game
```

**Manual**:
```bash
gcc my_game.c -I/usr/include -llgx_runtime -lpthread -o my_game
```

### 3. Use

```c
#include <lgx_runtime.h>

int main(void) {
    // Initialize with defaults
    lgx_runtime_config_t* cfg = lgx_config_create();
    lgx_config_set_flags(cfg, LGX_CONFIG_ENABLE_HUGE_PAGES);
    lgx_runtime_init(cfg);
    lgx_config_destroy(cfg);

    // Allocate memory using the right allocator automatically
    void* frame_data = lgx_alloc_frame(4096);      // Frame arena (< 0.1 μs)
    void* level_data = lgx_alloc_level(1024 * 1024); // Persistent heap
    void* gpu_buf = lgx_alloc_gpu_shared(65536);     // GPU pool

    // Game loop
    while (running) {
        // Frame-scoped allocations are FREE to "free"
        void* particles = lgx_alloc_frame(sizeof(Particle) * 10000);
        void* vertices = lgx_alloc_frame(sizeof(Vertex) * 50000);

        // ... render frame ...

        // All frame allocations reset automatically
    }

    lgx_free(level_data);
    lgx_free(gpu_buf);
    lgx_runtime_shutdown();
}
```

---

## Common Patterns

### Pattern 1: Game Loop with Frame Arena

```c
while (game_is_running) {
    // All lgx_alloc_frame() calls use the bump allocator
    // Zero overhead, no fragmentation, automatic cleanup

    // Physics scratch space
    void* contacts = lgx_alloc_frame(sizeof(Contact) * max_contacts);

    // Particle system
    void* particles = lgx_alloc_frame(particle_count * sizeof(Particle));

    // Render command buffer
    void* cmds = lgx_alloc_frame(sizeof(RenderCmd) * cmd_count);

    update_physics(contacts);
    update_particles(particles);
    submit_render(cmds);

    // Frame boundary — arena resets, all alloc_frame memory is reclaimed
    // No lgx_free() needed for frame allocations!
}
```

### Pattern 2: Level Loading

```c
void load_level(const char* path) {
    // Persistent allocations live until explicitly freed
    level_mesh = lgx_alloc_persistent(mesh_size);
    level_textures = lgx_alloc_persistent(texture_size);

    // GPU-shared memory for render resources
    vertex_buffer = lgx_alloc_gpu_shared(vb_size);
    index_buffer = lgx_alloc_gpu_shared(ib_size);

    load_mesh_data(path, level_mesh);
    upload_to_gpu(vertex_buffer, level_mesh);
}

void unload_level(void) {
    lgx_free(level_mesh);
    lgx_free(level_textures);
    lgx_free(vertex_buffer);
    lgx_free(index_buffer);
}
```

### Pattern 3: Health Monitoring

```c
void on_health_alert(const lgx_health_status_t* status, void* user_data) {
    lgx_log(LGX_LOG_WARN, "Health alert: %s", status->degradation_reason);
}

// Start monitoring with 1-second interval
lgx_health_set_alert_callback(on_health_alert, NULL);
lgx_health_set_thresholds(0.8f, 0.95f, 0.9f, 0.99f);
lgx_health_monitoring_start(1000);
```

### Pattern 4: Performance Profiling

```c
// Trace scope (auto-end)
{
    LGX_TRACE_SCOPE("physics_update");
    simulate_physics();
}

// Manual trace
lgx_trace_begin("render_pass", frame_number);
render_scene();
lgx_trace_end("render_pass");

// Export to JSON (viewable in chrome://tracing)
lgx_trace_export("/tmp/game_trace.json");
```

---

## Build System Integration

### CMake (Full Example)

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGame LANGUAGES C)

find_package(lgx_runtime 1.0 REQUIRED)

add_executable(my_game
    src/main.c
    src/renderer.c
    src/physics.c
)

target_link_libraries(my_game PRIVATE lgx_runtime::lgx_runtime)

# Optional: enable debug features
target_compile_definitions(my_game PRIVATE
    $<$<CONFIG:Debug>:LGX_DEBUG=1>
)
```

### Bazel

```python
cc_binary(
    name = "my_game",
    srcs = ["main.c"],
    deps = ["@lgx_runtime//:lgx_runtime"],
)
```

---

## ABI Stability

LGX guarantees **backward binary compatibility** within a major version:

- Games compiled against v1.0 headers run against v1.1, v1.2, etc.
- New fields are always added at the end of structs
- All structs use `struct_size` as the first field for forward compatibility
- Symbols are versioned via ELF symbol versioning (`LGX_RUNTIME_1.0`)

**Check compatibility at runtime**:
```c
lgx_version_t required = { .major = 1, .minor = 0, .patch = 0,
                            .struct_size = sizeof(lgx_version_t) };
if (lgx_runtime_check_compatibility(&required) != LGX_SUCCESS) {
    fprintf(stderr, "LGX version too old!\n");
    exit(1);
}
```

---

## Troubleshooting

### Common Issues

| Problem | Solution |
|---------|----------|
| "Not initialized" error | Call `lgx_runtime_init()` before any LGX API |
| High allocation latency | Ensure huge pages enabled: `sysctl vm.nr_hugepages=128` |
| Frame arena overflow | Increase with `lgx_config_set_frame_arena_size(cfg, 128*1024*1024)` |
| GPU pool empty | Check `lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)` |
| Memory leak warnings | Call `lgx_free()` for persistent/GPU allocations |

### Debug Mode

Compile with `-DLGX_DEBUG=1` to enable:
- Use-after-reset detection for frame arena
- Guard pages around allocations
- Memory canaries for corruption detection
- Double-free detection
- Intent mismatch warnings

### FAQ

**Q: Do I need to `lgx_free()` frame allocations?**  
A: No. Frame allocations are automatically reclaimed at frame boundary. Calling `lgx_free()` on frame memory is a safe no-op.

**Q: Can I mix LGX with `malloc()`?**  
A: Yes. LGX does not replace your system allocator. Use `lgx_alloc_*()` for performance-critical paths and `malloc()` for everything else.

**Q: Is LGX thread-safe?**  
A: Yes. All public APIs are thread-safe. The frame arena uses per-thread local arenas. The persistent heap uses lock-free free lists.

**Q: What if huge pages aren't available?**  
A: LGX degrades gracefully to standard 4 KB pages. Check `lgx_runtime_get_hardware_status()` for details.
