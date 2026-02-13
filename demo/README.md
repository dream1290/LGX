# LGX Runtime Core - Demo Applications

This directory contains demonstration applications that showcase LGX Runtime's capabilities in realistic scenarios.

## Game Simulation Demo

**File:** `demo_game_simulation.c`

A comprehensive demo that simulates a realistic game workload to demonstrate LGX Runtime's performance characteristics.

### Features Demonstrated

- **Frame-Based Allocation**: Automatic per-frame memory management with arena allocators
- **Persistent Allocations**: Long-lived object management for entities
- **Performance Monitoring**: Real-time metrics collection and reporting
- **Hardware Adaptation**: Automatic hardware tier detection
- **Cache Performance**: Hit/miss tracking and optimization

### Simulated Workload

The demo simulates a game with:
- Up to 1,000 entities (persistent allocations)
- Up to 5,000 particles per frame (frame allocations)
- Dynamic spawning and despawning
- Render data allocation per frame
- 60 FPS target frame rate

### Building

```bash
# From project root
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make demo_game_simulation
```

### Running

```bash
# From build directory
./demo_game_simulation
```

The demo will run for 10 seconds (600 frames) and display:
- Real-time progress bar
- Hardware tier detection
- Frame timing statistics (avg, min, max, P99)
- Allocation statistics
- Cache performance metrics
- Memory usage

### Output

The demo generates two outputs:

1. **Console Output**: Real-time statistics and final results
2. **CSV File** (`demo_results.csv`): Frame-by-frame data for analysis

### CSV Format

```
frame,frame_time_ms,allocation_time_us,frame_allocations,persistent_allocations,total_memory_mb,cache_hits,cache_misses
0,1.234,45.67,52,1,1.23,1000,10
1,1.189,43.21,51,0,1.24,2050,12
...
```

### Analyzing Results

You can analyze the CSV data using various tools:

#### Python (with pandas and matplotlib)

```python
import pandas as pd
import matplotlib.pyplot as plt

# Load data
df = pd.read_csv('demo_results.csv')

# Plot frame times
plt.figure(figsize=(12, 6))
plt.plot(df['frame'], df['frame_time_ms'])
plt.xlabel('Frame Number')
plt.ylabel('Frame Time (ms)')
plt.title('LGX Runtime - Frame Time Performance')
plt.axhline(y=16.67, color='r', linestyle='--', label='60 FPS Target')
plt.legend()
plt.savefig('frame_times.png')

# Plot memory usage
plt.figure(figsize=(12, 6))
plt.plot(df['frame'], df['total_memory_mb'])
plt.xlabel('Frame Number')
plt.ylabel('Memory Usage (MB)')
plt.title('LGX Runtime - Memory Usage Over Time')
plt.savefig('memory_usage.png')

# Plot cache hit rate
df['hit_rate'] = df['cache_hits'] / (df['cache_hits'] + df['cache_misses']) * 100
plt.figure(figsize=(12, 6))
plt.plot(df['frame'], df['hit_rate'])
plt.xlabel('Frame Number')
plt.ylabel('Cache Hit Rate (%)')
plt.title('LGX Runtime - Cache Performance')
plt.savefig('cache_performance.png')
```

#### gnuplot

```bash
# Frame times
gnuplot -e "set terminal png; set output 'frame_times.png'; \
            set xlabel 'Frame'; set ylabel 'Time (ms)'; \
            plot 'demo_results.csv' using 1:2 with lines title 'Frame Time'"

# Memory usage
gnuplot -e "set terminal png; set output 'memory_usage.png'; \
            set xlabel 'Frame'; set ylabel 'Memory (MB)'; \
            plot 'demo_results.csv' using 1:6 with lines title 'Memory Usage'"
```

#### LibreOffice Calc / Excel

1. Open `demo_results.csv` in LibreOffice Calc or Excel
2. Select data columns
3. Insert → Chart → Line Chart
4. Customize axes and labels

### Expected Results

On reference hardware (Intel Xeon, 32GB RAM):

- **Average Frame Time**: ~1-2 ms
- **P99 Frame Time**: <5 ms
- **Cache Hit Rate**: >95%
- **Memory Usage**: ~50-100 MB
- **Allocations per Frame**: ~50-150

### Customization

You can modify the demo parameters in `demo_game_simulation.c`:

```c
#define DEMO_DURATION_SECONDS 10    // Demo duration
#define TARGET_FPS 60                // Target frame rate
#define MAX_ENTITIES 1000            // Maximum entities
#define MAX_PARTICLES 5000           // Maximum particles per frame
```

### Troubleshooting

**Demo crashes or fails to start:**
- Ensure LGX Runtime is properly installed
- Check that you have sufficient memory (at least 1GB free)
- Verify library path: `export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH`

**Poor performance:**
- Build in Release mode: `cmake -DCMAKE_BUILD_TYPE=Release ..`
- Check system load: `top` or `htop`
- Verify hardware tier is OPTIMAL (not DEGRADED)

**CSV file not generated:**
- Check write permissions in current directory
- Verify disk space is available

### Performance Comparison

You can compare LGX Runtime performance against standard malloc:

```bash
# Build with malloc fallback disabled
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_MALLOC_FALLBACK=OFF ..
make demo_game_simulation
./demo_game_simulation

# Compare results with standard build
```

### Integration Example

This demo serves as a reference for integrating LGX Runtime into your application:

1. **Initialization**: Create config, set parameters, initialize runtime
2. **Per-Frame**: Use `lgx_alloc_frame()` for temporary data
3. **Persistent**: Use `lgx_alloc_persistent()` for long-lived objects
4. **Frame Reset**: Call `lgx_frame_reset()` at end of each frame
5. **Monitoring**: Collect metrics with `lgx_memory_stats()` and `lgx_get_counter()`
6. **Cleanup**: Shutdown runtime and destroy config

### Additional Demos

More demos coming soon:
- Multi-threaded workload demo
- GPU memory pool demo
- Suspend/resume demo
- Telemetry integration demo

### Support

For questions or issues with the demo:
- GitHub Issues: https://github.com/dream1290/LGX/issues
- Documentation: https://github.com/dream1290/LGX/tree/main/docs
