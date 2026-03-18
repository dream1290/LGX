# LGX Tooling Module v2.2 — API Reference

> Platform-level developer diagnostics (like DirectX PIX).

**Requires:** `lgx_runtime` (v1.0)
**Header:** `#include "lgx_tools.h"`
**Library:** `-llgx_tools -lm`

---

## Performance Analyzer

| Function | Description |
|----------|-------------|
| `lgx_perf_create(config)` | Create. NULL = defaults (1000 frames, 256 zones). |
| `lgx_perf_destroy(analyzer)` | Destroy. NULL-safe. |
| `lgx_perf_record_frame(a, time_ms)` | Record frame time. |
| `lgx_perf_record_zone(a, name, time_ms)` | Record zone time (accumulates). |
| `lgx_perf_get_avg_fps(a)` | Average FPS over history. |
| `lgx_perf_get_frame_stats(a, stats)` | min/max/avg/p95/p99/stddev. |
| `lgx_perf_get_hot_zones(a, out, max)` | Top N zones by total time (sorted). |
| `lgx_perf_dump_report(a, buf, max)` | Human-readable text report. |
| `lgx_perf_export_csv(a, path)` | Export frame data to CSV. |

---

## Memory Tracker

| Function | Description |
|----------|-------------|
| `lgx_memtrack_create(config)` | Create. NULL = defaults (4096 tracked allocs). |
| `lgx_memtrack_destroy(tracker)` | Destroy. NULL-safe. |
| `lgx_memtrack_record_alloc(t, ptr, size, tag)` | Record allocation with label. |
| `lgx_memtrack_record_free(t, ptr)` | Record free. |
| `lgx_memtrack_get_active_bytes(t)` | Current live bytes. |
| `lgx_memtrack_get_peak_bytes(t)` | High-water mark. |
| `lgx_memtrack_get_alloc_count(t)` | Lifetime alloc count. |
| `lgx_memtrack_find_leaks(t, out, max)` | List unfreed allocations. |
| `lgx_memtrack_dump_report(t, buf, max)` | Human-readable memory report. |
| `lgx_memtrack_export_csv(t, path)` | Export alloc log to CSV. |

---

## Features

- **No GUI** — text reports + CSV export for external visualization
- **Statistical analysis** — percentiles via sorted copy + qsort
- **Hot zone ranking** — sorted by cumulative time, shows avg per-call
- **Leak detection** — reports ptr, size, tag, alloc sequence number
- **Zero deps** — just lgx_runtime + libm
