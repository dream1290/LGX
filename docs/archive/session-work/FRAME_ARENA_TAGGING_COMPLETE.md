# Frame Arena Allocation Tagging - Task 3.4.5.4.3 Complete

## Summary

Successfully implemented allocation tagging for the frame arena allocator, allowing developers to label allocations by subsystem for better debugging and profiling.

## Implementation Details

### 1. Data Structures

Added `allocation_tag_t` structure to track allocations by subsystem:
```c
typedef struct {
    const char* tag;            // Tag name (e.g., "physics", "rendering")
    size_t total_bytes;         // Total bytes allocated with this tag
    uint64_t count;             // Number of allocations with this tag
    size_t peak_bytes;          // Peak bytes for this tag in current frame
} allocation_tag_t;
```

### 2. Core Functions

- `track_allocation_tag()` - Tracks allocations by tag name
- `lgx_frame_alloc_tagged_internal()` - Allocates memory with a subsystem tag
- `lgx_frame_alloc_tagged()` - Macro for easy use (captures file/line automatically)

### 3. Integration Points

#### Statistics Export
Added tag statistics to `lgx_frame_arena_dump_stats()`:
- Displays all tags sorted by total bytes
- Shows total bytes, allocation count, and peak bytes per tag

#### JSON Export
Added tag information to `lgx_frame_arena_dump()`:
- Exports all tags with their statistics
- Included in per-arena JSON objects

#### Frame Reset
Added tag reset logic in `lgx_frame_reset()`:
- Resets `total_bytes` and `count` for each tag
- Preserves `peak_bytes` for statistics

### 4. API

**Header Declaration** (`include/lgx/lgx_runtime_internal.h`):
```c
void* lgx_frame_alloc_tagged_internal(size_t size, const char* tag, const char* file, int line);

#ifndef NDEBUG
#define lgx_frame_alloc_tagged(size, tag) \
    lgx_frame_alloc_tagged_internal((size), (tag), __FILE__, __LINE__)
#else
#define lgx_frame_alloc_tagged(size, tag) \
    lgx_frame_alloc_tagged_internal((size), (tag), "unknown", 0)
#endif
```

**Version Script** (`lgx_runtime.map`):
- Added `lgx_frame_alloc_tagged_internal` to exported symbols

### 5. Test Coverage

Created comprehensive test (`tests/manual/test_frame_arena_tagging.c`):
- Simulates AAA game workload with multiple subsystems
- Tests physics, rendering, audio, AI, and networking allocations
- Verifies tag tracking in statistics and JSON export
- Validates untagged allocations (labeled as "untagged")

## Test Results

```
✅ Runtime initialized
✅ Completed 10 frames
✅ Exported allocation map with tags to /tmp/frame_arena_tags.json
✅ Test completed successfully
```

### Tag Statistics Output

```
Allocation Tags by Subsystem:
   1. rendering            - 122880 bytes (4 allocations, peak: 122880 bytes)
   2. physics              - 15360 bytes (4 allocations, peak: 15360 bytes)
   3. ai                   - 14336 bytes (3 allocations, peak: 14336 bytes)
   4. audio                - 7168 bytes (3 allocations, peak: 7168 bytes)
   5. networking           - 3072 bytes (2 allocations, peak: 3072 bytes)
```

### JSON Export Sample

```json
"allocation_tags": [
  {"tag": "rendering", "total_bytes": 122880, "count": 4, "peak_bytes": 122880},
  {"tag": "physics", "total_bytes": 15360, "count": 4, "peak_bytes": 15360},
  {"tag": "ai", "total_bytes": 14336, "count": 3, "peak_bytes": 14336},
  {"tag": "audio", "total_bytes": 7168, "count": 3, "peak_bytes": 7168},
  {"tag": "networking", "total_bytes": 3072, "count": 2, "peak_bytes": 3072}
]
```

## Usage Example

```c
// Allocate with subsystem tag
void* physics_data = lgx_frame_alloc_tagged(4096, "physics");
void* render_data = lgx_frame_alloc_tagged(8192, "rendering");
void* audio_buffer = lgx_frame_alloc_tagged(2048, "audio");

// Dump statistics to see tag breakdown
lgx_frame_arena_dump_stats(stdout);

// Export to JSON for visualization
lgx_frame_arena_dump("/tmp/frame_arena.json");
```

## Benefits

1. **Subsystem Profiling** - Identify which subsystems use the most frame memory
2. **Memory Budgeting** - Track memory usage per game system
3. **Debugging** - Quickly identify memory-heavy subsystems
4. **Optimization** - Target specific subsystems for memory reduction
5. **Visualization** - JSON export enables graphical analysis tools

## Files Modified

- `src/runtime/lgx_frame_arena.c` - Core implementation
- `include/lgx/lgx_runtime_internal.h` - API declarations
- `lgx_runtime.map` - Symbol exports
- `tests/manual/test_frame_arena_tagging.c` - Test coverage

## Next Steps

Task 3.4.5.4.4: Add frame arena profiler integration (Tracy, Optick)
