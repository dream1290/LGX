# LGX Runtime Integration Plan for SuperTuxKart

## Overview
This document outlines the plan to integrate LGX Runtime Core into SuperTuxKart to improve memory allocation performance and reduce fragmentation.

## SuperTuxKart Analysis

### Current Memory Allocation Patterns
- **Extensive use of `new`/`delete`**: Found throughout the codebase
- **Per-frame allocations**: Graphics, physics, particles, audio
- **Persistent allocations**: Game objects, textures, meshes
- **Main loop structure**: Clear frame boundaries in `MainLoop::run()`

### Key Integration Points

1. **Main Loop** (`stk-code/src/main_loop.cpp:400`)
   - Frame start: Line ~460
   - Frame end: After `irr_driver->update()` and `GUIEngine::update()`
   - Perfect place for `lgx_frame_reset()`

2. **Initialization** (`stk-code/src/main.cpp`)
   - Early init: `initUserConfig()` at line ~1904
   - Graphics init: `irr_driver = new IrrDriver()` at line ~1953
   - LGX should init before any major allocations

3. **Shutdown** (`stk-code/src/main.cpp`)
   - Clean shutdown path exists
   - LGX shutdown should happen after all cleanup

## Integration Strategy

### Phase 1: Build System Integration ✓ NEXT
1. Add LGX as a dependency in CMakeLists.txt
2. Link against liblgx_runtime
3. Add include paths

### Phase 2: Runtime Initialization
1. Initialize LGX early in `main()` before major allocations
2. Configure frame arena size (start with 64MB, tune based on profiling)
3. Add shutdown in cleanup path

### Phase 3: Frame Arena Integration
1. Add `lgx_frame_reset()` at frame boundaries in `MainLoop::run()`
2. Identify per-frame allocations that can use frame arena
3. Replace `new`/`delete` with `lgx_alloc_frame()` for temporary data

### Phase 4: Selective Allocation Replacement
Target high-frequency allocations:
- **Graphics**: Temporary render buffers, particle data
- **Physics**: Collision detection temporary data
- **Audio**: Sound buffer management
- **GUI**: Temporary UI element data

### Phase 5: Testing & Profiling
1. Baseline performance measurement (FPS, frame time, memory usage)
2. LGX-enabled build performance measurement
3. Compare results
4. Tune arena sizes based on actual usage

## Expected Benefits

### Performance Improvements
- **10-30% reduction in allocation overhead** for frame-temporary data
- **Reduced memory fragmentation** over long play sessions
- **More predictable frame times** (reduced P99 latency)
- **Simplified memory management** (no individual frees for frame data)

### Realistic Expectations
- Not a "1000× faster" improvement
- Benefits most visible in:
  - Complex scenes with many particles
  - Long play sessions (reduced fragmentation)
  - Lower-end hardware (less GC pressure)

## Implementation - Step 1: CMake Integration

Add LGX to SuperTuxKart's build system.

## Next Actions
1. Modify stk-code/CMakeLists.txt to find and link LGX
2. Test build
3. Add minimal init/shutdown code
4. Measure baseline performance
