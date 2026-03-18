# LGX Graphics Module v1.2 — API Reference

> Thin Vulkan wrapper providing device management, command recording,
> pipeline state, resource lifecycle, descriptor sets, and shader caching.

**Requires:** `lgx_runtime` (v1.0), Vulkan 1.3+
**Header:** `#include "lgx_graphics.h"`
**Library:** `-llgx_graphics`

---

## Device

| Function | Description |
|----------|-------------|
| `lgx_gfx_device_create(config)` | Create GPU device (VkInstance + VkDevice + queues). NULL config = defaults. |
| `lgx_gfx_device_destroy(device)` | Destroy device and all Vulkan objects. NULL-safe. |
| `lgx_gfx_device_get_gpu_info(device, info)` | Query GPU name, VRAM, Vulkan version, feature support. |
| `lgx_gfx_device_wait_idle(device)` | Block until all GPU work completes. |

### lgx_gfx_device_config_t

| Field | Type | Description |
|-------|------|-------------|
| `struct_size` | `size_t` | Must be `sizeof(lgx_gfx_device_config_t)` |
| `app_name` | `const char*` | Application name (debug tools) |
| `app_version` | `uint32_t` | Application version |
| `gpu_preference` | enum | `DISCRETE` / `INTEGRATED` / `ANY` |
| `enable_validation` | `bool` | Enable Vulkan validation layers |
| `enable_debug_markers` | `bool` | Enable GPU debug markers |

---

## Swapchain

| Function | Description |
|----------|-------------|
| `lgx_gfx_swapchain_create(device, config)` | Create swapchain from X11/Wayland window handle. |
| `lgx_gfx_swapchain_destroy(swapchain)` | Destroy swapchain + sync objects. NULL-safe. |
| `lgx_gfx_swapchain_acquire(swapchain, &idx)` | Acquire next image for rendering. Returns image index. |
| `lgx_gfx_swapchain_present(swapchain)` | Present rendered frame to screen. |
| `lgx_gfx_swapchain_resize(swapchain, w, h)` | Recreate swapchain after window resize. |

### lgx_gfx_swapchain_config_t

| Field | Type | Description |
|-------|------|-------------|
| `native_window` | `void*` | X11 `Window` or `wl_surface*` |
| `native_display` | `void*` | X11 `Display*` or `wl_display*` |
| `width`, `height` | `uint32_t` | Initial dimensions |
| `present_mode` | enum | `FIFO` (vsync) / `MAILBOX` (triple) / `IMMEDIATE` |
| `image_count` | `uint32_t` | 2 = double buffer, 3 = triple |
| `is_wayland` | `bool` | `true` for Wayland, `false` for X11 |

---

## Buffers

| Function | Description |
|----------|-------------|
| `lgx_gfx_buffer_create(dev, size, usage, host_visible)` | Create GPU buffer. |
| `lgx_gfx_buffer_destroy(buffer)` | Destroy buffer. NULL-safe. |
| `lgx_gfx_buffer_map(buffer)` | Map host-visible buffer for CPU access. Returns `void*`. |
| `lgx_gfx_buffer_unmap(buffer)` | Unmap a mapped buffer. |
| `lgx_gfx_buffer_upload(buffer, data, size, offset)` | Upload data. Uses staging buffer for device-local. |

**Usage flags** (combinable with `|`):
`VERTEX` · `INDEX` · `UNIFORM` · `STORAGE` · `TRANSFER`

---

## Images

| Function | Description |
|----------|-------------|
| `lgx_gfx_image_create(dev, w, h, format, usage)` | Create 2D image with view. |
| `lgx_gfx_image_destroy(image)` | Destroy image + view + memory. NULL-safe. |
| `lgx_gfx_image_upload(image, pixels, size)` | Upload pixels via staging buffer + layout transitions. |

**Formats:** `RGBA8_UNORM` · `RGBA8_SRGB` · `BGRA8_*` · `R32/RG32/RGB32/RGBA32_SFLOAT` · `RGBA16_SFLOAT` · `D32_SFLOAT` · `D24_S8`

---

## Shaders

| Function | Description |
|----------|-------------|
| `lgx_gfx_shader_create(dev, spirv, size, stage, entry)` | Load SPIR-V from memory. |
| `lgx_gfx_shader_load(dev, filepath, stage)` | Load SPIR-V from file. |
| `lgx_gfx_shader_destroy(shader)` | Destroy shader module. NULL-safe. |

**Stages:** `VERTEX` · `FRAGMENT` · `COMPUTE` (reserved)

---

## Pipelines

| Function | Description |
|----------|-------------|
| `lgx_gfx_pipeline_create(dev, config)` | Create graphics pipeline. Uses device pipeline cache. |
| `lgx_gfx_pipeline_destroy(pipeline)` | Destroy pipeline. NULL-safe. |

### lgx_gfx_pipeline_config_t

Vertex/fragment shaders, vertex attributes + stride, topology, polygon mode, cull mode, depth test/write, blend enable, render pass.

---

## Render Passes

| Function | Description |
|----------|-------------|
| `lgx_gfx_renderpass_create(dev, config)` | Create render pass with color + depth attachments. |
| `lgx_gfx_renderpass_destroy(renderpass)` | Destroy render pass. NULL-safe. |

---

## Descriptor Sets

| Function | Description |
|----------|-------------|
| `lgx_gfx_descriptor_set_create(dev, config)` | Create descriptor set with UBO/SSBO/sampler bindings. |
| `lgx_gfx_descriptor_set_destroy(desc_set)` | Destroy pool + layout + set. NULL-safe. |
| `lgx_gfx_cmd_bind_descriptor_set(cmd, pipe, desc)` | Bind descriptor set during recording. |

### lgx_gfx_descriptor_binding_t

| Field | Description |
|-------|-------------|
| `binding` | Shader binding index |
| `type` | `UNIFORM_BUFFER` / `STORAGE_BUFFER` / `COMBINED_SAMPLER` / `STORAGE_IMAGE` |
| `stage` | Shader stage visibility |
| `buffer` / `image` | Resource to bind |
| `buffer_offset`, `buffer_range` | Buffer sub-range (0 = whole) |

---

## Command Buffers

| Function | Description |
|----------|-------------|
| `lgx_gfx_cmd_begin(dev)` | Allocate and begin recording. |
| `lgx_gfx_cmd_end(cmd)` | End recording. |
| `lgx_gfx_cmd_submit(dev, cmd)` | Submit to graphics queue and wait. Frees cmd. |
| `lgx_gfx_cmd_begin_renderpass(cmd, rp, sc, idx, r,g,b,a)` | Begin render pass with clear color. |
| `lgx_gfx_cmd_end_renderpass(cmd)` | End render pass. |
| `lgx_gfx_cmd_bind_pipeline(cmd, pipeline)` | Bind graphics pipeline. |
| `lgx_gfx_cmd_bind_vertex_buffer(cmd, buffer)` | Bind vertex buffer at binding 0. |
| `lgx_gfx_cmd_bind_index_buffer(cmd, buffer)` | Bind index buffer (uint32). |
| `lgx_gfx_cmd_draw(cmd, verts, instances, first_v, first_i)` | Non-indexed draw. |
| `lgx_gfx_cmd_draw_indexed(cmd, indices, instances, first, offset, first_i)` | Indexed draw. |
| `lgx_gfx_cmd_set_viewport(cmd, x, y, w, h, min_d, max_d)` | Set dynamic viewport. |
| `lgx_gfx_cmd_set_scissor(cmd, x, y, w, h)` | Set dynamic scissor rect. |

---

## Shader Cache

| Function | Description |
|----------|-------------|
| `lgx_gfx_shader_cache_init(dev, config)` | Init cache. Loads existing `pipeline_cache.bin` from disk. |
| `lgx_gfx_shader_cache_flush(dev)` | Flush pipeline cache to disk. |
| `lgx_gfx_shader_cache_destroy(dev)` | Flush + cleanup. |

**Default path:** `~/.cache/lgx/shader_cache/`
**Default max size:** 256 MB

---

## Statistics

| Function | Description |
|----------|-------------|
| `lgx_gfx_get_stats(dev, stats)` | Get GPU name, VRAM, resource counts, frame counters. |
| `lgx_gfx_reset_frame_stats(dev)` | Zero per-frame counters (call at frame start). |

### lgx_gfx_stats_t

GPU name · Vulkan version · device memory · active buffers/images/pipelines/shaders · draw calls · pipeline switches · vertices submitted · shader cache hits/misses/size.
