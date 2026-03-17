# Frame Arena Tuning Spec Update

**Date**: February 10, 2026  
**Status**: Ready for Implementation  
**Priority**: CRITICAL

## Context

Frame arena overflow detected during testing:
```
[LGX WARNING] Frame arena overflow! Requested 64 bytes, but only 0 bytes available.
Frame 0, Arena 0, Total usage: 67108864 / 67108864 bytes (100.0%)
Falling back to persistent heap for this allocation.
```

The 64MB frame arena is being completely exhausted, triggering fallback to the persistent heap. This indicates either:
1. Arena size is too small for the workload
2. Frame reset is not being called properly
3. Memory leak in frame allocations

## Spec Updates

### 1. Requirements Document Updates

**New Acceptance Criterion: AC-16 - Frame Arena Adaptive Sizing and Overflow Handling**

Added comprehensive requirements for:
- Configurable frame arena size
- Adaptive growth on overflow (double size, max 256MB)
- Early warning at 80% capacity
- Allocation pattern tracking and size recommendations
- Graceful fallback with <5% performance penalty
- Rate-limited overflow warnings
- Detailed overflow telemetry
- Frame reset verification
- Memory leak detection
- Debugging tools (histogram, call site tracking, visualization)

### 2. Design Document Updates

**Enhanced Frame Arena Design with Adaptive Sizing**

Added detailed design for:
- Enhanced `lgx_frame_arena` structure with adaptive sizing fields
- Configuration API: `lgx_config_set_frame_arena_size()`, `lgx_config_set_frame_arena_max_size()`
- Enhanced `lgx_frame_alloc()` with overflow handling and adaptive growth
- Enhanced `lgx_frame_reset()` with usage tracking and early warnings
- Debugging API: `lgx_frame_get_stats()`, `lgx_frame_arena_dump()`
- Adaptive sizing strategy (64MB → 256MB max)
- Overflow handling with rate limiting and telemetry
- Debugging tools integration

### 3. Tasks Document Updates

**New Task Section: 3.4.5 - Frame Arena Tuning and Debugging**

Added 5 major tasks with 20 subtasks:

**3.4.5.1 Investigate frame arena overflow root cause**
- Add detailed logging for allocation patterns
- Implement allocation histogram (size distribution)
- Track top allocation call sites
- Verify frame reset is being called
- Check for memory leaks

**3.4.5.2 Implement adaptive frame arena sizing**
- Add configurable arena size API
- Implement dynamic arena growth (double on overflow, max 256MB)
- Add arena size recommendation based on peak usage
- Implement per-frame usage tracking with rolling average
- Add 80% capacity early warning

**3.4.5.3 Improve frame arena overflow handling**
- Add overflow counter and rate limiting (max 1/sec)
- Implement overflow telemetry with context
- Add fallback pool statistics
- Implement overflow recovery strategy

**3.4.5.4 Add frame arena debugging tools**
- Implement `lgx_frame_arena_dump()` for allocation map export
- Add visualization tool for usage over time
- Implement allocation tagging by subsystem
- Add profiler integration (Tracy, Optick)

**3.4.5.5 Validate frame arena fixes**
- Run stress test with high allocation rate
- Verify no overflows with adaptive sizing
- Measure performance impact (<1% overhead)
- Document recommended arena sizes

## Implementation Priority

1. **IMMEDIATE** (3.4.5.1): Investigate root cause - understand why overflow is happening
2. **HIGH** (3.4.5.2): Implement adaptive sizing - fix the immediate problem
3. **HIGH** (3.4.5.3): Improve overflow handling - better diagnostics
4. **MEDIUM** (3.4.5.4): Add debugging tools - prevent future issues
5. **MEDIUM** (3.4.5.5): Validate fixes - ensure solution works

## Success Criteria

- [ ] No frame arena overflows with adaptive sizing enabled
- [ ] Performance impact of overflow handling <1%
- [ ] Clear diagnostics when overflow occurs
- [ ] Recommended arena sizes documented for different game types
- [ ] All 20 subtasks completed and validated

## Next Steps

1. Review this spec update with stakeholders
2. Begin implementation with task 3.4.5.1 (root cause investigation)
3. Implement adaptive sizing (3.4.5.2) to fix immediate issue
4. Add comprehensive debugging tools (3.4.5.4)
5. Validate with stress tests (3.4.5.5)

## Files Modified

- `.kiro/specs/lgx-runtime-core/requirements.md` - Added AC-16
- `.kiro/specs/lgx-runtime-core/design.md` - Added adaptive sizing design
- `.kiro/specs/lgx-runtime-core/tasks.md` - Added section 3.4.5 with 20 subtasks
