# LGX Profiling Module v1.5 — API Reference

> Frame-level profiler: hierarchical zones, performance counters, FPS tracking.

**Requires:** `lgx_runtime` (v1.0)
**Header:** `#include "lgx_profile.h"`
**Library:** `-llgx_profile`

---

## Context Lifecycle

| Function | Description |
|----------|-------------|
| `lgx_prof_create(config)` | Create profiling context. NULL = defaults (256 zones, 64 counters, 120 frames). |
| `lgx_prof_destroy(ctx)` | Destroy context. NULL-safe. |

---

## Frame Boundary

| Function | Description |
|----------|-------------|
| `lgx_prof_frame_begin(ctx)` | Mark frame start, reset zone stack. |
| `lgx_prof_frame_end(ctx)` | Mark frame end, store in history. |

---

## Zones (Hierarchical Timing)

| Function | Description |
|----------|-------------|
| `lgx_prof_zone_begin(ctx, name)` | Push named zone (records timestamp). Zones nest. |
| `lgx_prof_zone_end(ctx)` | Pop zone, record duration. |

**Macros:** `LGX_PROFILE_ZONE_BEGIN(ctx, name)`, `LGX_PROFILE_ZONE_END(ctx)` — same as above.

---

## Counters

| Function | Description |
|----------|-------------|
| `lgx_prof_counter_set(ctx, name, value)` | Set named counter (creates if new). |
| `lgx_prof_counter_add(ctx, name, delta)` | Increment named counter. |
| `lgx_prof_get_counter(ctx, name)` | Get value (0.0 if not found). |

---

## Query

| Function | Description |
|----------|-------------|
| `lgx_prof_get_frame_time_ms(ctx)` | Last frame time in ms. |
| `lgx_prof_get_zone_time_ms(ctx, name)` | Last frame's zone time (ms). |
| `lgx_prof_get_fps(ctx)` | Rolling FPS from frame history. |
| `lgx_prof_get_frame_count(ctx)` | Total frames profiled. |

---

## Export

| Function | Description |
|----------|-------------|
| `lgx_prof_dump_last_frame(ctx, buf, size)` | Human-readable text dump of last frame. |

---

## Features

- **Timing:** `CLOCK_MONOTONIC` nanosecond resolution
- **Zones:** Hierarchical push/pop with up to 64 nesting levels
- **History:** Ring buffer (default 120 frames) for FPS calculation
- **Zero-cost:** All functions are no-ops when `enabled = false`
- **No extra deps:** Only lgx_runtime, pre-allocated everything at init
