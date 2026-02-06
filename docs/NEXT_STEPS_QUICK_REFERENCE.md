# Next Steps - Quick Reference

## What Just Happened

✅ **Spec revised** to pivot from general-purpose allocator to specialized allocators
✅ **Requirements updated** with specialized allocator acceptance criteria
✅ **Design updated** with frame arena, GPU pool, and persistent heap designs
✅ **Tasks updated** with Month 1-3 implementation plan
✅ **Documentation created** explaining the pivot decision

## Files to Review

1. **`.kiro/specs/lgx-runtime-core/requirements.md`** - Updated requirements
2. **`.kiro/specs/lgx-runtime-core/design.md`** - New allocator designs
3. **`.kiro/specs/lgx-runtime-core/tasks.md`** - Revised implementation tasks
4. **`docs/PHASE_0_PIVOT_DECISION.md`** - Full analysis and rationale
5. **`docs/SPEC_REVISION_SUMMARY.md`** - Summary of changes
6. **`docs/BEFORE_AFTER_COMPARISON.md`** - Visual comparison

## What to Do Next

### Option 1: Review and Approve
If the revised spec looks good:
1. Review the updated requirements, design, and tasks
2. Approve the pivot decision
3. Begin Month 1: Frame Arena implementation

### Option 2: Request Changes
If you want modifications:
1. Specify what needs to change
2. I'll update the spec accordingly
3. Iterate until approved

### Option 3: Ask Questions
If you need clarification:
1. Ask about any aspect of the design
2. I'll explain in detail
3. Update documentation as needed

## Month 1: Frame Arena Implementation Plan

### Week 1: Triple-Buffered Arenas
- Allocate 3 × 64MB arenas using huge pages
- Implement frame rotation logic
- Add frame boundary detection

### Week 2: Bump Pointer Allocation
- Implement `lgx_frame_alloc(size)` with bump pointer
- Add 16-byte alignment
- Implement `lgx_frame_reset()` for frame boundary

### Week 3: Optimization and Safety
- Optimize for cache performance
- Add use-after-reset detection (debug builds)
- Add arena overflow warnings

### Week 4: Testing and Validation
- Write comprehensive tests
- Validate P99 < 0.1 μs
- Integrate with existing codebase

## Key Design Decisions to Confirm

### Frame Arena
- **Size**: 3 × 64MB = 192MB total
- **Allocation**: Bump pointer (O(1), ~0.01 μs)
- **Reset**: At frame boundary (automatic)
- **Overflow**: Fallback to persistent heap

**Question**: Does 64MB per frame seem reasonable? (Typical game: 10-50MB per frame)

### GPU Memory Pool
- **Capacity**: 2GB device-local, 256MB host-visible, 64MB host-cached
- **Allocator**: Buddy allocator (O(log n), ~10 μs)
- **Alignment**: 256B for buffers, 4KB for images

**Question**: Does 2GB VRAM budget seem reasonable for typical games?

### Persistent Heap
- **Strategy**: Segregated fit (<4KB) + buddy allocator (>4KB)
- **Defragmentation**: During loading screens (100ms budget)
- **Target**: <5% fragmentation over 8-hour sessions

**Question**: Is 100ms defragmentation budget acceptable during loading screens?

## Performance Targets to Validate

| Allocator | P99 Target | How to Measure |
|-----------|------------|----------------|
| Frame arena | < 0.1 μs | Benchmark 1M allocations, measure P99 |
| GPU pool | < 10 μs | Benchmark GPU allocations, measure P99 |
| Persistent heap | < 20 μs | Benchmark long-lived allocations, measure P99 |

## Success Criteria

### Month 1 (Frame Arena)
- ✅ P99 < 0.1 μs (100 nanoseconds)
- ✅ Handles 80% of typical game allocations
- ✅ Zero fragmentation
- ✅ Automatic reset at frame boundaries

### Month 2 (GPU Pool)
- ✅ P99 < 10 μs
- ✅ Pre-allocated GPU memory (no runtime allocation)
- ✅ Alignment guarantees (256B buffers, 4KB images)
- ✅ Vulkan integration

### Month 3 (Persistent Heap)
- ✅ P99 < 20 μs
- ✅ <5% fragmentation over 8-hour sessions
- ✅ Defragmentation during loading screens
- ✅ Leak detection

## Questions to Answer

### Technical Questions
1. Should frame arena size be configurable or fixed at 64MB?
2. Should we support manual frame reset or only automatic?
3. Should GPU pool sizes be configurable or fixed?
4. Should defragmentation be automatic or manual?

### Process Questions
1. Do you want to review the spec before implementation starts?
2. Do you want weekly progress updates during Month 1?
3. Do you want to approve each month's work before proceeding to the next?

## How to Proceed

### If you're ready to start:
```
"Begin Month 1: Frame Arena implementation"
```

### If you want to review first:
```
"Let me review the spec, I'll get back to you"
```

### If you have questions:
```
"I have questions about [specific topic]"
```

### If you want changes:
```
"Change [specific aspect] to [desired approach]"
```

## Contact Points

- **Spec files**: `.kiro/specs/lgx-runtime-core/`
- **Documentation**: `docs/`
- **Current status**: Phase 0 complete, Phase 1 ready to start
- **Next milestone**: Month 1 Week 1 (Triple-buffered arenas)

---

**Status**: ✅ Spec revision complete, awaiting approval to proceed
**Next Action**: Your decision on how to proceed
**Timeline**: 3 months for Phase 1 (Month 1: Frame arena, Month 2: GPU pool, Month 3: Persistent heap)

