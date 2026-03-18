# LGX Runtime Core - Telemetry & Privacy Guide

## Overview

The LGX Runtime Core includes an enhanced telemetry system with strong privacy guarantees. This guide explains what data is collected, how privacy is protected, and how to use telemetry for performance optimization.

## Privacy-First Design

### Core Principles

1. **Opt-in Only**: Telemetry is disabled by default and requires explicit user consent
2. **Transparency**: Users can inspect exactly what data is collected
3. **No PII**: Never collects personally identifiable information
4. **User Control**: Users can configure what data is collected
5. **Local Storage**: Data stays on user's machine unless explicitly exported

### What We NEVER Collect

❌ **Absolutely Prohibited**:
- User names or account information
- File paths or directory structures
- Process names or command-line arguments
- IP addresses or network information
- Any personally identifiable information (PII)

### What We CAN Collect (With Consent)

 **Performance Metrics** (Safe, no privacy risk):
- Frame times (milliseconds)
- Memory usage (megabytes)
- Allocation sizes (bytes)
- Cache hit rates (percentages)

⚠️ **Hardware Information** (Disabled by default, fingerprinting risk):
- CPU model (e.g., "Intel Core i7-9700K")
- GPU model (e.g., "NVIDIA GeForce RTX 3080")
- Kernel version (e.g., "Linux 6.1.0")

## Privacy Policy Configuration

### Default Privacy Policy

```c
lgx_privacy_policy_t default_policy = {
    .struct_size = sizeof(lgx_privacy_policy_t),
    
    // Safe metrics (enabled by default)
    .collect_frame_times = true,
    .collect_allocation_sizes = true,
    
    // Hardware info (disabled by default - fingerprinting risk)
    .collect_cpu_model = false,
    .collect_gpu_model = false,
    .collect_kernel_version = false,
    
    // Anonymization
    .add_noise = false,  // No noise by default
    .noise_stddev = 0.05,  // 5% noise if enabled
    .aggregate_only = true  // Only send aggregates, not raw data
};
```

### Custom Privacy Policy

```c
// Example: Enable hardware info collection for debugging
lgx_telemetry_config_t config = {
    .struct_size = sizeof(lgx_telemetry_config_t),
    .enabled = true,
    .ring_buffer_size = 10000,
    .overflow_policy = LGX_TEL_SAMPLE,
    .adaptive_sampling = true,
    .min_sample_rate = 0.01,  // 1% minimum
    
    .privacy_policy = {
        .struct_size = sizeof(lgx_privacy_policy_t),
        .collect_frame_times = true,
        .collect_allocation_sizes = true,
        .collect_cpu_model = true,  // Enable for debugging
        .collect_gpu_model = true,  // Enable for debugging
        .collect_kernel_version = false,
        .add_noise = false,
        .aggregate_only = true
    }
};

lgx_result_t result = lgx_telemetry_configure(&config);
```

## Enabling Telemetry

### Step 1: Get User Consent

**CRITICAL**: Always ask for explicit user consent before enabling telemetry.

```c
// Example: Ask user for consent
bool user_consent = ask_user_for_telemetry_consent();

if (user_consent) {
    lgx_result_t result = lgx_telemetry_enable(true);
    if (result == LGX_SUCCESS) {
        printf("Telemetry enabled with user consent\n");
    }
} else {
    printf("Telemetry disabled (no user consent)\n");
}
```

### Step 2: Verify Privacy Policy

Users should be able to inspect the privacy policy:

```c
lgx_privacy_policy_t policy = lgx_telemetry_get_privacy_policy();

printf("Telemetry Privacy Policy:\n");
printf("  Collect frame times: %s\n", policy.collect_frame_times ? "YES" : "NO");
printf("  Collect allocations: %s\n", policy.collect_allocation_sizes ? "YES" : "NO");
printf("  Collect CPU model: %s\n", policy.collect_cpu_model ? "YES" : "NO");
printf("  Collect GPU model: %s\n", policy.collect_gpu_model ? "YES" : "NO");
printf("  Aggregate only: %s\n", policy.aggregate_only ? "YES" : "NO");
```

## Adaptive Sampling

### Purpose

Adaptive sampling prevents telemetry buffer overflow by automatically reducing sample rate when buffer fills up.

### How It Works

```
Buffer Fullness    Sample Rate    Behavior
─────────────────────────────────────────────
< 50%              100%           Sample everything
50-75%             50%            Sample half
75-90%             10%            Sample 10%
> 90%              1% (min)       Emergency mode
```

### Configuration

```c
lgx_telemetry_config_t config = {
    .adaptive_sampling = true,
    .min_sample_rate = 0.01,  // 1% minimum (emergency mode)
    .ring_buffer_size = 10000,
    .overflow_policy = LGX_TEL_SAMPLE  // Use adaptive sampling
};
```

### Overflow Policies

1. **LGX_TEL_DROP_OLDEST**: Ring buffer behavior (drop oldest events)
2. **LGX_TEL_DROP_NEWEST**: Preserve history (drop new events)
3. **LGX_TEL_SAMPLE**: Adaptive sampling (recommended)

## Correlation Analysis

### What Is Correlation Analysis?

Correlation analysis automatically identifies relationships between events to help diagnose performance issues.

### Supported Correlations

#### 1. Allocation Bursts → Frame Spikes

**Detection**: Looks for allocation bursts (>10 allocations in 16ms) before frame spikes.

**Example Output**:
```json
{
  "type": "allocation_burst_frame_spike",
  "confidence": 0.85,
  "description": "Frame-time spikes correlate with allocation bursts",
  "recommendation": "Pre-allocate memory or use frame arena for temporary allocations"
}
```

**Action**: Use frame arena for temporary allocations instead of heap.

#### 2. Memory Growth Trend (Leak Detection)

**Detection**: Uses linear regression to detect upward memory trend.

**Example Output**:
```json
{
  "type": "memory_growth_trend",
  "confidence": 0.70,
  "description": "Memory usage increasing over time (potential leak)",
  "recommendation": "Check for memory leaks, enable allocation tracking"
}
```

**Action**: Enable allocation tracking and check for leaks.

## Exporting Telemetry Data

### Export for User Inspection

Users can export telemetry data to inspect what was collected:

```c
const char* output_path = "/tmp/lgx_telemetry.json";
lgx_result_t result = lgx_telemetry_export_collected_data(output_path);

if (result == LGX_SUCCESS) {
    printf("Telemetry data exported to %s\n", output_path);
    printf("You can inspect this file to see what data was collected.\n");
}
```

### Export Format

```json
{
  "lgx_version": "1.0.0",
  "export_timestamp": 1707398400000000000,
  "user_consent": true,
  
  "privacy_policy": {
    "collect_frame_times": true,
    "collect_allocation_sizes": true,
    "collect_cpu_model": false,
    "collect_gpu_model": false,
    "aggregate_only": true
  },
  
  "sampling": {
    "adaptive_sampling": true,
    "current_sample_rate": 0.5000,
    "events_collected": 5432,
    "events_dropped": 123
  },
  
  "frame_times": {
    "average_ms": 16.67,
    "min_ms": 14.23,
    "max_ms": 45.12,
    "frame_count": 10000,
    "spike_count": 15
  },
  
  "memory": {
    "average_mb": 150,
    "peak_mb": 180,
    "samples": 1000
  },
  
  "allocations": {
    "total": 50000
  },
  
  "correlations": [
    {
      "type": "allocation_burst_frame_spike",
      "confidence": 0.85,
      "description": "Frame-time spikes correlate with allocation bursts",
      "recommendation": "Pre-allocate memory or use frame arena"
    }
  ],
  
  "crashes": 0
}
```

## Using Telemetry for Optimization

### Example 1: Diagnosing Frame Spikes

```c
// Enable telemetry
lgx_telemetry_enable(true);

// Run game for a while
run_game_loop();

// Export and analyze
lgx_telemetry_export_collected_data("/tmp/telemetry.json");

// Check correlations in exported JSON
// If "allocation_burst_frame_spike" found:
//   → Use frame arena for temporary allocations
```

### Example 2: Detecting Memory Leaks

```c
// Enable telemetry with memory tracking
lgx_telemetry_config_t config = {
    .enabled = true,
    .privacy_policy = {
        .collect_frame_times = true,
        .collect_allocation_sizes = true
    }
};
lgx_telemetry_configure(&config);
lgx_telemetry_enable(true);

// Run game for extended period
run_game_for_hours();

// Export and check for memory growth trend
lgx_telemetry_export_collected_data("/tmp/telemetry.json");

// If "memory_growth_trend" found:
//   → Enable allocation tracking
//   → Check for missing lgx_free() calls
```

## Best Practices

### 1. Always Get User Consent

```c
// GOOD: Ask user first
bool consent = ask_user();
lgx_telemetry_enable(consent);

// BAD: Enable without asking
lgx_telemetry_enable(true);  // ❌ No consent!
```

### 2. Show Privacy Policy

```c
// GOOD: Show what's collected
lgx_privacy_policy_t policy = lgx_telemetry_get_privacy_policy();
show_privacy_policy_to_user(&policy);

// BAD: Hide what's collected
// (No transparency)
```

### 3. Allow Data Inspection

```c
// GOOD: Let user inspect data
if (user_wants_to_inspect()) {
    lgx_telemetry_export_collected_data("/tmp/my_data.json");
    open_file_in_editor("/tmp/my_data.json");
}

// BAD: Hide collected data
// (No transparency)
```

### 4. Disable in Production (Optional)

```c
#ifdef DEBUG_BUILD
    // Enable telemetry in debug builds
    lgx_telemetry_enable(true);
#else
    // Disable in production (or ask user)
    bool consent = ask_user();
    lgx_telemetry_enable(consent);
#endif
```

### 5. Use Adaptive Sampling

```c
// GOOD: Enable adaptive sampling
config.adaptive_sampling = true;
config.min_sample_rate = 0.01;  // 1% minimum

// BAD: No sampling (buffer overflow)
config.adaptive_sampling = false;  // ❌ Will overflow!
```

## Troubleshooting

### Issue: Buffer Overflow

**Symptoms**: `events_dropped` is very high

**Solution**:
```c
// Enable adaptive sampling
config.adaptive_sampling = true;
config.min_sample_rate = 0.01;

// Or increase buffer size
config.ring_buffer_size = 50000;  // Larger buffer
```

### Issue: No Correlations Found

**Symptoms**: `correlations` array is empty

**Causes**:
1. Not enough data collected (need >100 events)
2. No actual correlations exist
3. Sample rate too low

**Solution**:
```c
// Collect more data
run_game_longer();

// Increase sample rate
config.min_sample_rate = 0.10;  // 10% minimum
```

### Issue: Privacy Concerns

**Symptoms**: User worried about data collection

**Solution**:
```c
// Show privacy policy
lgx_privacy_policy_t policy = lgx_telemetry_get_privacy_policy();
printf("We collect: frame times, memory usage\n");
printf("We DON'T collect: file paths, user names, IP addresses\n");

// Let user inspect data
lgx_telemetry_export_collected_data("/tmp/inspect.json");
printf("You can inspect the data at /tmp/inspect.json\n");

// Disable if user still concerned
lgx_telemetry_enable(false);
```

## Summary

The LGX telemetry system provides powerful performance insights while respecting user privacy:

- **Opt-in only**: Requires explicit user consent
- **Transparent**: Users can inspect collected data
- **Privacy-first**: Never collects PII
- **Adaptive sampling**: Prevents buffer overflow
- **Correlation analysis**: Automatically diagnoses issues
- **User control**: Configurable privacy policy

For questions or concerns, see the [LGX Runtime Core documentation](../README.md).
