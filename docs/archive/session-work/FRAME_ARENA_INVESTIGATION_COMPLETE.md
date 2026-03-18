# Frame Arena Overflow Investigation - Complete 

**Date**: February 10, 2026  
**Task**: 3.4.5.1 - Investigate frame arena overflow root cause  
**Status**:  COMPLETE - All 5 subtasks finished

---

## Executive Summary

Successfully identified the root cause of frame arena overflow: **missing `lgx_frame_reset()` calls** in performance tests. The frame arena implementation is working correctly - the issue is a usage error in test code.

---

## Root Cause Analysis

### The Problem
```
[LGX WARNING] Frame arena overflow! Requested 64 bytes, but only 0 bytes available.
Frame 0, Arena 0, Total usage: 67108864 / 67108864 bytes (100.0%)
Falling back to persistent heap for this allocation.
```

### The Cause
**File**: `tests/performance/test_frame_time_contribution.c`

The test simulates 1000 frames with up to 1000 allocations per frame, but **never calls `lgx_frame_reset()`** between frames.

```c
// Current code (BROKEN):
for (int i = 0; i < frames_per_test; i++) {
    frame_times[i] = simulate_frame(alloc_count);
    // ❌ MISSING: lgx_frame_reset() should be called here!
}
```

### What Happens
1. Frame 0: Allocates ~0.6 MB of data
2. Frame 1: Allocates another ~0.6 MB (total: 1.2 MB)
3. Frame 2: Allocates another ~0.6 MB (total: 1.8 MB)
4. ...
5. Frame 112: Total reaches 67.2 MB → **OVERFLOW!**
6. All subsequent allocations fall back to persistent heap

---

## Task Completion Status

###  Task 3.4.5.1.1: Add detailed logging
**Status**: COMPLETE

**What was added**:
- Enhanced overflow warnings with allocation site information
- Detailed usage statistics in error messages
- File and line number tracking for allocations

**Code changes**:
- Modified `lgx_frame_alloc()` to include tracking
- Added call site parameter to internal allocation function
- Enhanced debug output with more context

###  Task 3.4.5.1.2: Implement allocation histogram
**Status**: COMPLETE

**What was added**:
- `allocation_histogram_t` structure with 32 logarithmic buckets
- Tracks size distribution from 16 bytes to 1 MB
- Per-arena histogram tracking
- Histogram dump in diagnostic output

**Code changes**:
- Added histogram structure to `lgx_frame_arena_t`
- Implemented `track_allocation_histogram()` function
- Implemented `get_histogram_bucket()` for logarithmic bucketing
- Integrated histogram into `lgx_frame_arena_dump_stats()`

###  Task 3.4.5.1.3: Track top allocation call sites
**Status**: COMPLETE

**What was added**:
- `allocation_call_site_t` structure
- Tracks top 100 allocation sites by file and line number
- Records total bytes and allocation count per site
- Sorted output showing top 10 sites

**Code changes**:
- Added call site tracking array to `lgx_frame_arena_t`
- Implemented `track_allocation_call_site()` function
- Integrated into allocation path
- Added sorting and display in dump function

###  Task 3.4.5.1.4: Verify lgx_frame_reset() is being called
**Status**: COMPLETE - **VERIFIED NOT BEING CALLED**

**Findings**:
- `lgx_frame_reset()` is **NOT** being called in the failing test
- Frame arena implementation is correct
- Triple-buffering works as designed
- Issue is user error, not implementation bug

**Evidence**:
- Reviewed test code: no reset calls found
- Created diagnostic test demonstrating the issue
- Confirmed arena works correctly when reset is called

###  Task 3.4.5.1.5: Check for memory leaks
**Status**: COMPLETE - **NO LEAKS FOUND**

**Findings**:
- Frame arena properly resets offset to 0 on `l