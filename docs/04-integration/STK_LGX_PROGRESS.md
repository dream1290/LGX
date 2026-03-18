# SuperTuxKart + LGX Integration Progress

## Status: Phase 1 Complete ✓

### Completed Steps

#### 1. Repository Analysis ✓
- Explored SuperTuxKart codebase structure
- Identified main loop in `src/main_loop.cpp`
- Found initialization in `src/main.cpp`
- Confirmed extensive use of `new`/`delete` allocations

#### 2. CMake Integration ✓
- Added `USE_LGX_RUNTIME` option to CMakeLists.txt
- Added library finding logic
- Successfully configured build with LGX enabled
- **Result**: `-- LGX Runtime enabled: /usr/lib/liblgx_runtime.so`

### Key Files Modified
1. `stk-code/CMakeLists.txt`:
   - Added option at line ~32: `option(USE_LGX_RUNTIME ...)`
   - Added library finding at line ~766-780
   - Links LGX when enabled

### Build Configuration
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_LGX_RUNTIME=ON -DCHECK_ASSETS=OFF
```

## Next Steps: Phase 2 - Runtime Initialization

### Step 1: Add LGX Initialization Code
Location: `stk-code/src/main.cpp`

Add early in `main()` function:
```cpp
#ifdef USE_LGX_RUNTIME
#include <lgx_runtime.h>

static lgx_runtime_config_t* g_lgx_config = nullptr;

static void init_lgx_runtime() {
    g_lgx_config = lgx_config_create();
    lgx_config_set_memory_pool_size(g_lgx_config, 256 * 1024 * 1024); // 256MB
    
    if (lgx_runtime_init(g_lgx_config) != LGX_SUCCESS) {
        Log::error("LGX", "Failed to initialize LGX runtime");
        lgx_config_destroy(g_lgx_config);
        g_lgx_config = nullptr;
    } else {
        Log::info("LGX", "LGX Runtime initialized successfully");
    }
}

static void shutdown_lgx_runtime() {
    if (g_lgx_config) {
        lgx_runtime_shutdown();
        lgx_config_destroy(g_lgx_config);
        g_lgx_config = nullptr;
        Log::info("LGX", "LGX Runtime shutdown complete");
    }
}
#endif
```

### Step 2: Add Frame Reset Calls
Location: `stk-code/src/main_loop.cpp`

In `MainLoop::run()` after frame updates:
```cpp
#ifdef USE_LGX_RUNTIME
#include <lgx_runtime.h>
#endif

void MainLoop::run()
{
    while (!m_abort)
    {
        // ... existing frame processing ...
        
        // After all updates, before next frame
        #ifdef USE_LGX_RUNTIME
        lgx_frame_reset();
        #endif
        
        PROFILER_POP_CPU_MARKER();
    }
}
```

### Step 3: Build and Test
```bash
# Build STK with LGX
cd stk-code/build-lgx
make -j$(nproc)

# Run and verify LGX initialization
./bin/supertuxkart --log=verbose
```

### Step 4: Baseline Performance Measurement
Before making any allocation changes, measure baseline:
```bash
# Run a consistent test
./bin/supertuxkart --track=lighthouse --laps=3 --profile=1

# Measure:
- Average FPS
- Frame time (min/max/p99)
- Memory usage
```

## Phase 3: Selective Allocation Replacement

### High-Value Targets (to be identified via profiling)
1. **Graphics subsystem**: Temporary render buffers
2. **Physics**: Collision detection temporary data
3. **Particles**: Per-frame particle data
4. **Audio**: Sound buffer management

### Approach
- Start with ONE subsystem
- Replace `new`/`delete` with `lgx_alloc_frame()`
- Measure impact
- Iterate

## Expected Timeline

- **Phase 1** (CMake): ✓ Complete
- **Phase 2** (Init/Shutdown): 1-2 hours
- **Phase 3** (Frame Reset): 1 hour
- **Phase 4** (Testing): 2-3 hours
- **Phase 5** (Allocation Replacement): 1-2 days (iterative)

## Success Criteria

### Minimum (Must Achieve)
- ✓ CMake integration works
- [ ] STK builds with LGX enabled
- [ ] STK runs without crashes
- [ ] No performance regression

### Target (Goal)
- [ ] 5-15% FPS improvement in complex scenes
- [ ] Reduced P99 frame time
- [ ] Lower memory fragmentation over time

### Stretch (Bonus)
- [ ] 20-30% improvement in allocation-heavy scenarios
- [ ] Measurable reduction in frame time variance

## Notes

- This is a **real-world integration** test
- Focus on **honest measurements**
- Document **actual results** (not theoretical)
- SuperTuxKart is mature and optimized - expect modest gains
- The value is in **proving LGX works** in production code

## Current Status

**Ready for Phase 2**: Runtime initialization code needs to be added to `src/main.cpp` and `src/main_loop.cpp`.

The build system is configured and ready. Next step is to add the actual LGX initialization and frame reset calls.
