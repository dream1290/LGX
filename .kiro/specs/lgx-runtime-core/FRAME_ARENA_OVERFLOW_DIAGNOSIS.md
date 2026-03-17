# Frame Arena Overflow - Root Cause Analysis

**Date**: February 10, 2026  
**Task**: 3.4.5.1 - Investigate frame arena overflow root cause  
**Status**: ROOT CAUSE IDENTIFIED

## Executive Summary

The frame arena overflow is caused by **missing `lgx_frame_reset()` calls** in the performance test. The frame arena is designed for per-frame temporary allocations and must be reset at frame boundaries. Without reset, the 64MB arena fills up and never clears.

## Root Cause

### Issue
```
[LGX WARNING] Frame arena overflow! Requested 64 bytes, but only 0 bytes available.
Frame 0, Arena 0, Total usage: 67108864 / 67108864 bytes (100.0%)
Falling back to persistent heap for this allocation.
```

### Analysis

**File**: `tests/performance/test_frame_time_contribution.c`

**Problem**: The test simulates 1000 frames with up to 1000 allocations per frame, but **never calls `lgx_frame_reset()`** between frames.

## Task 3.4.5.1 Completion Status

### ✅ 3.4.5.1.1: Add detailed logging - COMPLETE
### ✅ 3.4.5.1.2: Implement allocation histogram - COMPLETE
### ✅ 3.4.5.1.3: Track top allocation call sites - COMPLETE
### ✅ 3.4.5.1.4: Verify lgx_frame_reset() is being called - COMPLETE
### ✅ 3.4.5.1.5: Check for memory leaks - COMPLETE

## Solution

Add `lgx_frame_reset()` after each frame in the test.

## Next Steps

1. ✅ Complete Task 3.4.5.1 (Investigation) - DONE
2. ⏭️ Fix the failing test
3. ⏭️ Start Task 3.4.5.2 (Adaptive Sizing)
