# API Reference

Complete API documentation for the LGX Runtime Core.

## Overview

The LGX Runtime Core provides a high-performance memory management system with:
- **Intent-based allocation** - Automatic routing to optimal allocators
- **Frame arena** - Ultra-fast per-frame allocations (P99 < 0.1μs)
- **GPU memory pool** - Pre-allocated GPU-visible memory
- **Persistent heap** - Fragmentation-resistant long-lived allocations
- **Graceful degradation** - Works across diverse hardware

## Quick Reference

### Initialization
```c
lgx_runtime_config_t* config = lgx_config_create();
lgx_runtime_init(config);
```

### Memory Allocation
```c
// Intent-based allocation (recommended)
void* ptr = lgx_alloc_frame(1024);      // Frame-scoped
void* ptr = lgx_alloc_persistent(1024); // Long-lived
void* ptr = lgx_alloc_gpu(1024);        // GPU-visible

// Generic allocation
void* ptr = lgx_alloc(1024);

// Free memory
lgx_free(ptr);
```

### Error Handling
```c
lgx_result_t result = lgx_runtime_init(config);
if (result != LGX_SUCCESS) {
    const char* error = lgx_result_to_string(result);
    printf("Error: %s\n", error);
}
```

---

## API Documentation

### Core Runtime APIs
- [lgx_runtime_api.md](lgx_runtime_api.md) - Complete LGX Runtime Core API Reference
- [lgx_runtime_integration_guide.md](../04-integration/lgx_runtime_integration_guide.md) - Integration guide and patterns

### Module APIs
- [lgx_threading_api.md](lgx_threading_api.md) - Threading and concurrency primitives
- [lgx_graphics_api.md](lgx_graphics_api.md) - Graphics and rendering API
- [lgx_audio_api.md](lgx_audio_api.md) - 3D spatial audio engine with ALSA output
- [lgx_input_api.md](lgx_input_api.md) - Input handling (gamepad, keyboard, mouse)
- [lgx_asset_api.md](lgx_asset_api.md) - Asset loading, hot-reload, and compression
- [lgx_net_api.md](lgx_net_api.md) - Networking primitives
- [lgx_profile_api.md](lgx_profile_api.md) - Profiling and performance analysis
- [lgx_tools_api.md](lgx_tools_api.md) - Development tools and utilities

### API Categories (Core Runtime)

**Initialization & Configuration**
- `lgx_config_create()` - Create configuration
- `lgx_config_destroy()` - Destroy configuration
- `lgx_config_set_*()` - Configuration setters
- `lgx_runtime_init()` - Initialize runtime
- `lgx_runtime_shutdown()` - Shutdown runtime

**Memory Allocation**
- `lgx_alloc()` - Generic allocation
- `lgx_alloc_frame()` - Frame-scoped allocation
- `lgx_alloc_persistent()` - Persistent allocation
- `lgx_alloc_gpu()` - GPU-visible allocation
- `lgx_free()` - Free memory

**Error Handling**
- `lgx_get_last_error()` - Get last error
- `lgx_result_to_string()` - Convert error to string
- `lgx_set_error_handler()` - Set custom error handler

**Health & Monitoring**
- `lgx_runtime_health_check()` - Check runtime health
- `lgx_get_counter()` - Get performance counter
- `lgx_reset_counters()` - Reset counters

**Telemetry**
- `lgx_telemetry_enable()` - Enable telemetry
- `lgx_telemetry_export()` - Export telemetry data

---

## Code Examples

See the [examples/](examples/) directory for complete working examples:
- [basic-usage.c](examples/basic-usage.c) - Basic allocation and deallocation
- [frame-allocations.c](examples/frame-allocations.c) - Frame-scoped allocations
- [error-handling.c](examples/error-handling.c) - Error handling patterns
- [gpu-allocations.c](examples/gpu-allocations.c) - GPU memory allocation

---

## Best Practices

1. **Use intent-based allocation** - Let the runtime choose the optimal allocator
2. **Check return values** - Always check for NULL and error codes
3. **Free memory** - Always free allocated memory (except frame allocations)
4. **Handle errors gracefully** - Use error handlers for production code
5. **Monitor health** - Use health checks to detect degradation

---

## Performance Characteristics

| Allocator | P99 Latency | Use Case |
|-----------|-------------|----------|
| Frame Arena | < 0.1μs | Per-frame temporary data (80% of allocations) |
| GPU Pool | < 10μs | GPU-visible memory (15% of allocations) |
| Persistent Heap | < 20μs | Long-lived data (5% of allocations) |

See [Performance Guide](../05-performance/README.md) for details.

---

## Related Documentation

- [Integration Guide](../03-integration-guide/README.md) - Integrate into your project
- [Architecture](../04-architecture/README.md) - System design
- [Performance](../05-performance/README.md) - Performance optimization

---

**Status**: To be completed in Task 11.1
