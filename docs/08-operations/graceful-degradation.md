# Graceful Degradation Framework

## Overview

The LGX Runtime implements comprehensive graceful degradation to handle hardware diversity. When hardware features are unavailable, the runtime automatically falls back to software implementations with minimal performance impact.

## Software Fallbacks (Task 4.2.1)

### 1. Huge Pages Fallback

**Hardware Feature**: 2MB huge pages for reduced TLB misses

**Fallback Strategy**:
- Primary: Try explicit huge page allocation via `MAP_HUGETLB`
- Fallback 1: Use transparent huge pages (THP) via `madvise(MADV_HUGEPAGE)`
- Fallback 2: Regular 4KB pages via standard `mmap()`

**Performance Impact**: 3-5% slower allocation due to increased TLB misses

**Implementation**: `src/runtime/lgx_hugepages.c`

**Detection**:
```c
lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
if (status.missing_capabilities & (1 << LGX_CAP_HUGE_PAGES)) {
    // Huge pages unavailable, using fallback
}
```

### 2. SIMD Fallback

**Hardware Feature**: AVX2 instructions for parallel operations

**Fallback Strategy**:
- Primary: AVX2 SIMD operations (4 pointers at once)
- Fallback: Scalar operations (1 pointer at a time)

**Performance Impact**: 10-15% slower for buddy allocator searches

**Implementation**: `src/runtime/lgx_simd_ops.c`

**Auto-Detection**:
```c
void lgx_simd_detect_features(void);  // Called at init
bool lgx_simd_has_avx2(void);         // Check if AVX2 available

// All SIMD functions automatically use fallback:
int lgx_simd_find_nonempty_slot(void** slots, int count);
bool lgx_simd_all_null(void** slots, int count);
int lgx_simd_count_nonempty(void** slots, int count);
```

### 3. GPU Fallback

**Hardware Feature**: Dedicated GPU with Vulkan support

**Fallback Strategy**:
- Primary: GPU memory allocation via Vulkan
- Fallback: Software rendering (CPU-based)

**Performance Impact**: Significant (10-100x slower for graphics)

**Implementation**: `src/runtime/lgx_gpu_pool.c`

**Detection**:
```c
lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
if (status.missing_capabilities & (1 << LGX_CAP_DX11_TRANSLATION)) {
    // No GPU, software rendering only
}
```

### 4. NUMA Fallback

**Hardware Feature**: NUMA-aware memory allocation

**Fallback Strategy**:
- Primary: NUMA-aware allocation with node affinity
- Fallback: Standard allocation (single memory pool)

**Performance Impact**: Minimal on single-socket systems, 5-10% on multi-socket

**Implementation**: `src/runtime/lgx_hardware_adapter.c`

**Detection**:
```c
lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
if (status.missing_capabilities & (1 << LGX_CAP_NUMA_AWARENESS)) {
    // Single NUMA node, no optimization needed
}
```

## Degradation Reporting (Task 4.2.2)

### User-Friendly Explanations

The runtime provides clear, actionable information about degraded configurations:

```c
lgx_hardware_status_t status = lgx_runtime_get_hardware_status();

printf("Hardware Tier: %s\n", 
    status.achieved_tier == LGX_HW_TIER_OPTIMAL ? "OPTIMAL" :
    status.achieved_tier == LGX_HW_TIER_COMPATIBLE ? "COMPATIBLE" : "DEGRADED");

printf("Reason: %s\n", status.degradation_reason);
printf("Impact: %s\n", status.performance_impact_estimate);
printf("Fix: %s\n", status.remediation_steps);
```

**Example Output**:
```
Hardware Tier: COMPATIBLE
Reason: Huge pages unavailable, Single NUMA node
Impact: 3-5% slower allocation, No NUMA optimization
Fix: Enable huge pages: echo 1024 > /proc/sys/vm/nr_hugepages
```

## Feature Flags (Task 4.2.3)

### Optional Capabilities

The runtime uses capability flags to track optional features:

```c
// Capability flags (from lgx_types.h)
typedef enum lgx_capability {
    LGX_CAP_HUGE_PAGES = 0,
    LGX_CAP_NUMA_AWARENESS = 1,
    LGX_CAP_DX11_TRANSLATION = 5,
    LGX_CAP_DX12_TRANSLATION = 6,
    // ... more capabilities
} lgx_capability_t;

// Check individual capability
bool has_huge_pages = lgx_runtime_has_capability(LGX_CAP_HUGE_PAGES);

// Get all capabilities as bitmask
uint32_t capabilities = 0;
lgx_runtime_query_capabilities(&capabilities);

if (capabilities & (1 << LGX_CAP_HUGE_PAGES)) {
    // Huge pages available
}
```

### Runtime Configuration

Features can be disabled at runtime for testing:

```c
lgx_runtime_config_t* config = lgx_config_create();

// Disable optional features for testing
lgx_config_set_flags(config, LGX_CONFIG_DISABLE_HUGE_PAGES);
lgx_config_set_flags(config, LGX_CONFIG_DISABLE_SIMD);

lgx_runtime_init(config);
```

## Degradation Impact Measurement (Task 4.2.4)

### Performance Tracking

The runtime tracks performance impact of fallbacks:

```c
// Get memory statistics
lgx_memory_stats_t stats;
lgx_memory_stats(&stats);

printf("Allocation P99: %.2f μs\n", stats.allocation_p99_us);
printf("Cache hit rate: %.1f%%\n", stats.cache_hit_rate * 100);

// Get hardware status
lgx_hardware_status_t hw_status = lgx_runtime_get_hardware_status();
printf("Performance impact: %s\n", hw_status.performance_impact_estimate);
```

### Telemetry Integration

Degradation events are logged for analysis:

```c
// Telemetry automatically tracks:
// - Fallback usage frequency
// - Performance impact per fallback
// - Hardware tier distribution
// - Remediation success rate

lgx_telemetry_export("telemetry.json");
```

## Hardware Tier Classification

### Tier Definitions

**OPTIMAL** (Native Hardware):
- All hardware features available
- No fallbacks needed
- Best performance

**COMPATIBLE** (Emulated Features):
- Some hardware features missing
- Software fallbacks active
- Minor performance penalty (3-10%)

**DEGRADED** (Software Fallback):
- Critical hardware missing (e.g., no GPU)
- Significant fallbacks required
- Major performance penalty (10-100%)

### Automatic Adaptation

The runtime automatically adapts to hardware tier:

```c
// Frame arena uses huge pages if available
lgx_frame_arena_init();  // Automatically uses huge pages or fallback

// GPU pool adapts to available memory types
lgx_gpu_pool_init(instance, physical_device, device);  // Auto-detects ReBAR, device-local, etc.

// SIMD operations auto-detect AVX2
lgx_simd_find_nonempty_slot(slots, count);  // Uses AVX2 or scalar
```

## Testing Degradation

### Simulating Missing Hardware

For testing, you can simulate missing hardware:

```bash
# Disable huge pages
echo 0 > /proc/sys/vm/nr_hugepages

# Disable GPU (blacklist driver)
echo "blacklist i915" > /etc/modprobe.d/blacklist-gpu.conf

# Test with SIMD disabled
LGX_DISABLE_SIMD=1 ./your_app
```

### Validation

Run tests to verify fallbacks work:

```bash
# Test hardware tier classification
./build/test_hardware_tier

# Test SIMD fallback
./build/test_simd_fallback

# Test GPU capability detection
./build/test_gpu_capability
```

## Best Practices

1. **Always check hardware status** at startup to inform users
2. **Log degradation events** for debugging and telemetry
3. **Provide remediation guidance** to help users optimize
4. **Test on diverse hardware** to ensure fallbacks work
5. **Monitor performance impact** to validate fallback efficiency

## Performance Expectations

| Hardware Tier | Frame Arena P99 | GPU Pool P99 | Persistent Heap P99 |
|---------------|-----------------|--------------|---------------------|
| OPTIMAL       | 0.01 μs         | 5-10 μs      | 10-20 μs            |
| COMPATIBLE    | 0.01 μs         | 10-15 μs     | 15-25 μs            |
| DEGRADED      | 0.02 μs         | N/A (no GPU) | 20-30 μs            |

## Conclusion

The graceful degradation framework ensures the LGX Runtime works on diverse hardware configurations while providing clear feedback and remediation guidance to users. All fallbacks are automatic, transparent, and well-tested.
