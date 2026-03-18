# LGX Runtime Core — API Reference

> **Version**: 1.0.0  
> **ABI Stability**: Guaranteed within major version  
> **Thread Safety**: All functions are thread-safe unless noted

## Table of Contents

1. [Initialization & Shutdown](#initialization--shutdown)
2. [Memory Allocation](#memory-allocation)
3. [Frame Arena](#frame-arena)
4. [GPU Memory Pool](#gpu-memory-pool)
5. [Persistent Heap](#persistent-heap)
6. [Error Handling](#error-handling)
7. [Health Monitoring](#health-monitoring)
8. [Performance Counters](#performance-counters)
9. [Telemetry](#telemetry)
10. [Trace Events](#trace-events)
11. [Logging](#logging)
12. [Platform Services](#platform-services)
13. [Lifecycle Management](#lifecycle-management)
14. [Hardware Adaptation](#hardware-adaptation)
15. [Chaos Testing](#chaos-testing)

---

## Initialization & Shutdown

### `lgx_config_create()`
```c
lgx_runtime_config_t* lgx_config_create(void);
```
Creates a new runtime configuration with sensible defaults.  
**Returns**: Opaque config handle. Must be freed with `lgx_config_destroy()`.

### Configuration Setters
```c
void lgx_config_set_log_path(lgx_runtime_config_t* config, const char* path);
void lgx_config_set_memory_pool_size(lgx_runtime_config_t* config, size_t size);
void lgx_config_set_flags(lgx_runtime_config_t* config, uint32_t flags);
void lgx_config_set_frame_arena_size(lgx_runtime_config_t* config, size_t size);
void lgx_config_set_frame_arena_max_size(lgx_runtime_config_t* config, size_t max_size);
void lgx_config_destroy(lgx_runtime_config_t* config);
```

**Available flags**:
| Flag | Description |
|------|-------------|
| `LGX_CONFIG_ENABLE_TELEMETRY` | Enable opt-in telemetry collection |
| `LGX_CONFIG_ENABLE_DEBUG_LOGGING` | Enable verbose debug logging |
| `LGX_CONFIG_STRICT_VALIDATION` | Enable strict input validation |
| `LGX_CONFIG_ENABLE_HUGE_PAGES` | Request 2MB huge pages for arenas |
| `LGX_CONFIG_ENABLE_NUMA` | Enable NUMA-aware allocation |

### `lgx_runtime_init()`
```c
lgx_result_t lgx_runtime_init(const lgx_runtime_config_t* config);
```
Initializes the LGX runtime with the given configuration. Performs parallel initialization of all subsystems.  
**Thread Safety**: Must be called from main thread before any other LGX calls.  
**Returns**: `LGX_SUCCESS` or error code.

### `lgx_runtime_shutdown()`
```c
lgx_result_t lgx_runtime_shutdown(void);
```
Shuts down the runtime, releases all resources. Drains queues, joins telemetry process.  
**Thread Safety**: Must be called from main thread after all other threads have stopped using LGX.

### Version & Compatibility
```c
lgx_version_t lgx_runtime_get_version(void);
lgx_result_t lgx_runtime_check_compatibility(const lgx_version_t* required_version);
```

---

## Memory Allocation

LGX uses an **intent-based allocation system** that automatically routes allocations to the optimal allocator based on declared lifetime and usage patterns.

### Simple Allocation
```c
void* lgx_alloc(size_t size);              // General allocation (routes to persistent heap)
void* lgx_alloc_aligned(size_t size, size_t alignment);  // Aligned allocation
void  lgx_free(void* ptr);                 // Auto-detects allocator, frees accordingly
```

### Intent-Based Allocation
```c
void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent);
```

The intent structure tells LGX how the memory will be used:

```c
typedef struct {
    size_t struct_size;                    // Always sizeof(lgx_allocation_intent_base_t)
    size_t size;                           // Allocation size in bytes
    lgx_access_pattern_t access_pattern;   // SEQUENTIAL, RANDOM, WRITE_ONCE, UNKNOWN
    lgx_lifetime_t lifetime;               // FRAME, LEVEL, SESSION, UNKNOWN
    lgx_performance_hint_t hint;           // CRITICAL_PATH, BACKGROUND, GPU_SHARED, etc.
    lgx_intent_validation_t validation_policy;  // TRUST, VALIDATE_WARN, VALIDATE_ADAPT
} lgx_allocation_intent_base_t;
```

**Routing logic**:
| Lifetime | Hint | Routes To | P99 Latency |
|----------|------|-----------|-------------|
| `FRAME` | any | Frame arena | < 0.1 μs |
| any | `GPU_SHARED` | GPU pool | < 10 μs |
| `LEVEL` / `SESSION` | any | Persistent heap | < 20 μs |

### Convenience Functions
```c
void* lgx_alloc_frame(size_t size);        // Frame-scoped (reset at frame boundary)
void* lgx_alloc_level(size_t size);        // Level-scoped (freed on level change)
void* lgx_alloc_persistent(size_t size);   // Session lifetime
void* lgx_alloc_gpu_shared(size_t size);   // GPU-visible memory
```

### Memory Statistics
```c
lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats);
lgx_result_t lgx_get_memory_usage(lgx_memory_usage_t* usage);
lgx_result_t lgx_intent_get_stats(lgx_intent_stats_t* stats);
```

---

## Frame Arena

Triple-buffered bump pointer allocator for per-frame temporary data. Handles 80% of game allocations at < 0.1 μs P99.

- **Default size**: 64 MB per arena (3 arenas = 192 MB total)
- **Alignment**: 16-byte aligned
- **Reset**: Automatic at frame boundary via `lgx_frame_reset()`
- **Overflow**: Falls back to persistent heap with warning
- **Adaptive sizing**: Doubles on overflow, up to configurable max (default 256 MB)

```c
size_t lgx_frame_arena_get_recommended_size(void);
```
Returns recommended arena size based on observed usage patterns.

---

## GPU Memory Pool

Buddy allocator for GPU-visible memory. Pre-allocates large Vulkan memory blocks and sub-allocates.

- **Buffer alignment**: 256 bytes
- **Image alignment**: 4 KB
- **Pre-allocation**: 2 GB device-local, 256 MB host-visible
- **P99 latency**: < 10 μs

---

## Persistent Heap

Segregated-fit allocator for long-lived game data (level geometry, textures, etc.).

- **16 size classes**: 16 B to 4 KB (slab allocation)
- **Large allocations**: Buddy allocator for > 4 KB
- **Fragmentation target**: < 5% over 8-hour sessions
- **Defragmentation**: Available during loading screens (100 ms budget)
- **P99 latency**: < 20 μs

---

## Error Handling

### Basic
```c
lgx_error_context_t lgx_get_last_error(void);
const char* lgx_result_to_string(lgx_result_t result);
void lgx_clear_last_error(void);
```

### Enhanced (with recovery guidance)
```c
lgx_error_context_ex_t lgx_get_last_error_ex(void);
void lgx_set_error_handler(lgx_error_callback_t callback, void* user_data);
```

The enhanced error context includes:
- **Severity**: `WARNING`, `ERROR`, `FATAL`
- **Recovery action**: `RETRY`, `DEGRADE`, `ABORT`, `SHUTDOWN`
- **Recovery steps**: Human-readable string
- **Context**: function name, file, line, timestamp

### Result Codes
| Code | Meaning |
|------|---------|
| `LGX_SUCCESS` | Operation succeeded |
| `LGX_ERROR_INVALID_PARAM` | NULL pointer or invalid argument |
| `LGX_ERROR_NOT_INITIALIZED` | `lgx_runtime_init()` not called |
| `LGX_ERROR_OUT_OF_MEMORY` | All allocators exhausted |
| `LGX_ERROR_GPU_UNAVAILABLE` | No GPU or driver issue |
| `LGX_ERROR_HARDWARE_DEGRADED` | Missing hardware feature, using fallback |
| `LGX_ERROR_RESOURCE_LIMIT_EXCEEDED` | Hit configured resource limit |

---

## Health Monitoring

```c
lgx_result_t lgx_runtime_health_check(lgx_health_status_t* status);
lgx_result_t lgx_health_monitoring_start(uint32_t interval_ms);
lgx_result_t lgx_health_monitoring_stop(void);
void lgx_health_set_alert_callback(lgx_health_alert_callback_t callback, void* user_data);
void lgx_health_set_thresholds(float mem_warn, float mem_crit, float cpu_warn, float cpu_crit);
```

### Memory Leak Detection
```c
lgx_result_t lgx_leak_detector_init(void);
lgx_result_t lgx_leak_detector_add_sample(size_t total, size_t frame, size_t gpu, size_t heap);
bool lgx_leak_detector_is_leak_detected(void);
lgx_result_t lgx_leak_detector_get_stats(lgx_leak_detector_stats_t* stats);
void lgx_leak_detector_set_alert_callback(lgx_leak_alert_callback_t cb, void* user_data);
```

---

## Performance Counters

```c
uint64_t lgx_get_counter(lgx_counter_t counter);
void lgx_reset_counters(void);
lgx_custom_counter_t lgx_register_counter(const char* name);
void lgx_increment_counter(lgx_custom_counter_t counter);
void lgx_add_to_counter(lgx_custom_counter_t counter, uint64_t value);
```

**Built-in counters**: `ALLOCATIONS`, `DEALLOCATIONS`, `CACHE_HITS`, `CACHE_MISSES`, `POOL_EXHAUSTIONS`, `INTENT_MISMATCHES`, `HARDWARE_FALLBACKS`, `NUMA_MIGRATIONS`

---

## Telemetry

Opt-in telemetry with formal privacy framework. Runs in a separate process to avoid game impact.

```c
lgx_result_t lgx_telemetry_configure(const lgx_telemetry_config_t* config);
lgx_result_t lgx_telemetry_set_enabled(bool opt_in);
lgx_privacy_policy_t lgx_telemetry_get_privacy_policy(void);
lgx_result_t lgx_telemetry_export_collected_data(const char* output_path);
```

- **IPC**: Shared memory ring buffer (lock-free)
- **Privacy**: Data anonymized with SHA-256, user can export/inspect all data
- **Overhead**: < 0.1% when enabled

---

## Trace Events

Integration with perf, Valgrind, and Tracy.

```c
lgx_result_t lgx_trace_init(void);
void lgx_trace_begin(const char* name, uint64_t data);
void lgx_trace_end(const char* name);
void lgx_trace_instant(const char* name, uint64_t data);
lgx_result_t lgx_trace_export(const char* output_path);   // JSON export
```

**Convenience macro** (auto-end on scope exit):
```c
LGX_TRACE_SCOPE("physics_step");
```

**External tool integration**:
```c
lgx_trace_enable_perf_integration(true);
lgx_trace_enable_tracy_integration(true);
```

---

## Logging

### Basic
```c
void lgx_log(lgx_log_level_t level, const char* format, ...);
void lgx_set_log_filter(lgx_log_level_t min_level);
```

### Structured (subsystem-tagged)
```c
void lgx_log_tagged(lgx_log_subsystem_t subsystem, lgx_log_level_t level, const char* fmt, ...);
void lgx_set_subsystem_filter(uint32_t subsystem_mask);
```

**Subsystems**: `CORE`, `MEMORY`, `GPU`, `FILESYSTEM`, `TELEMETRY`, `LIFECYCLE`, `HARDWARE`, `SECURITY`

### Configuration
```c
void lgx_set_log_max_size(size_t max_size_bytes);     // Default: 100 MB
void lgx_set_log_rotation_enabled(bool enabled);
void lgx_set_log_rate_limiting_enabled(bool enabled);
void lgx_set_log_rate_limit(uint64_t logs_per_second); // Default: 10000
```

---

## Platform Services

### Timing
```c
uint64_t lgx_time_now_ns(void);                // CLOCK_MONOTONIC nanoseconds
void lgx_time_sleep_ms(uint32_t milliseconds);
uint64_t lgx_time_get_precision_ns(void);
uint64_t lgx_time_measure_overhead_ns(void);
```

### Filesystem
```c
lgx_result_t lgx_fs_open(const char* path, const char* mode, lgx_file_t** file);
lgx_result_t lgx_fs_read(lgx_file_t* file, void* buf, size_t size, size_t* bytes_read);
lgx_result_t lgx_fs_write(lgx_file_t* file, const void* buf, size_t size);
lgx_result_t lgx_fs_close(lgx_file_t* file);
```

---

## Lifecycle Management

```c
lgx_result_t lgx_runtime_suspend(void);   // Save state, < 100 ms budget
lgx_result_t lgx_runtime_resume(void);    // Restore state
```

---

## Hardware Adaptation

```c
bool lgx_runtime_has_capability(lgx_capability_t cap);
lgx_result_t lgx_runtime_query_capabilities(uint32_t* capabilities);
lgx_hardware_status_t lgx_runtime_get_hardware_status(void);
```

**Hardware tiers**:
| Tier | Description |
|------|-------------|
| `OPTIMAL` | All hardware features available (huge pages, NUMA, GPU) |
| `COMPATIBLE` | Some features emulated, slight performance penalty |
| `DEGRADED` | Software fallback active, reduced performance |

The status struct includes remediation steps (e.g., "Enable huge pages: `sysctl vm.nr_hugepages=128`").

---

## Chaos Testing

Debug-only framework for stress testing error paths.

```c
lgx_result_t lgx_runtime_enable_chaos_testing(const lgx_chaos_config_t* config);
lgx_result_t lgx_runtime_disable_chaos_testing(void);
```

Scenarios: memory pressure, latency spikes, GPU timeouts, NUMA imbalance, filesystem full.

---

## Observability Levels

```c
void lgx_set_observability_level(lgx_observability_level_t level);
```

| Level | Overhead | Includes |
|-------|----------|----------|
| `NONE` | 0% | Nothing |
| `MINIMAL` | < 0.1% | Counters only |
| `NORMAL` | < 0.5% | Counters + errors + warnings |
| `DETAILED` | < 2% | + debug logs |
| `EXHAUSTIVE` | > 5% | Everything including traces |
