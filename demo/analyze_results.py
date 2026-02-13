#!/usr/bin/env python3
"""
LGX Runtime Demo Results Analyzer

This script analyzes the CSV output from demo_game_simulation and generates
visualization plots and statistical reports.

Usage:
    python3 analyze_results.py demo_results.csv
"""

import sys
import csv
import statistics
from pathlib import Path

def load_data(filename):
    """Load CSV data into a list of dictionaries."""
    data = []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                'frame': int(row['frame']),
                'frame_time_ms': float(row['frame_time_ms']),
                'allocation_time_us': float(row['allocation_time_us']),
                'frame_allocations': int(row['frame_allocations']),
                'persistent_allocations': int(row['persistent_allocations']),
                'total_memory_mb': float(row['total_memory_mb']),
                'cache_hits': int(row['cache_hits']),
                'cache_misses': int(row['cache_misses'])
            })
    return data

def calculate_statistics(data):
    """Calculate statistical metrics from the data."""
    frame_times = [d['frame_time_ms'] for d in data]
    alloc_times = [d['allocation_time_us'] for d in data]
    frame_allocs = [d['frame_allocations'] for d in data]
    memory_usage = [d['total_memory_mb'] for d in data]
    
    # Calculate cache hit rate
    last_frame = data[-1]
    total_accesses = last_frame['cache_hits'] + last_frame['cache_misses']
    hit_rate = (last_frame['cache_hits'] / total_accesses * 100) if total_accesses > 0 else 0
    
    stats = {
        'total_frames': len(data),
        'frame_time': {
            'mean': statistics.mean(frame_times),
            'median': statistics.median(frame_times),
            'stdev': statistics.stdev(frame_times) if len(frame_times) > 1 else 0,
            'min': min(frame_times),
            'max': max(frame_times),
            'p95': sorted(frame_times)[int(len(frame_times) * 0.95)],
            'p99': sorted(frame_times)[int(len(frame_times) * 0.99)]
        },
        'allocation_time': {
            'mean': statistics.mean(alloc_times),
            'median': statistics.median(alloc_times),
            'min': min(alloc_times),
            'max': max(alloc_times)
        },
        'allocations_per_frame': {
            'mean': statistics.mean(frame_allocs),
            'median': statistics.median(frame_allocs),
            'min': min(frame_allocs),
            'max': max(frame_allocs)
        },
        'memory_usage': {
            'mean': statistics.mean(memory_usage),
            'peak': max(memory_usage),
            'min': min(memory_usage)
        },
        'cache': {
            'hits': last_frame['cache_hits'],
            'misses': last_frame['cache_misses'],
            'hit_rate': hit_rate
        }
    }
    
    return stats

def print_report(stats):
    """Print a formatted statistical report."""
    print("=" * 80)
    print("LGX RUNTIME DEMO - STATISTICAL ANALYSIS")
    print("=" * 80)
    print()
    
    print(f"Total Frames Analyzed: {stats['total_frames']}")
    print()
    
    print("Frame Timing Statistics:")
    print(f"  Mean:       {stats['frame_time']['mean']:.3f} ms ({1000/stats['frame_time']['mean']:.1f} FPS)")
    print(f"  Median:     {stats['frame_time']['median']:.3f} ms")
    print(f"  Std Dev:    {stats['frame_time']['stdev']:.3f} ms")
    print(f"  Min:        {stats['frame_time']['min']:.3f} ms")
    print(f"  Max:        {stats['frame_time']['max']:.3f} ms")
    print(f"  P95:        {stats['frame_time']['p95']:.3f} ms")
    print(f"  P99:        {stats['frame_time']['p99']:.3f} ms")
    print()
    
    print("Allocation Performance:")
    print(f"  Mean Time:  {stats['allocation_time']['mean']:.2f} μs")
    print(f"  Median:     {stats['allocation_time']['median']:.2f} μs")
    print(f"  Min:        {stats['allocation_time']['min']:.2f} μs")
    print(f"  Max:        {stats['allocation_time']['max']:.2f} μs")
    print()
    
    print("Allocations per Frame:")
    print(f"  Mean:       {stats['allocations_per_frame']['mean']:.1f}")
    print(f"  Median:     {stats['allocations_per_frame']['median']:.0f}")
    print(f"  Min:        {stats['allocations_per_frame']['min']}")
    print(f"  Max:        {stats['allocations_per_frame']['max']}")
    print()
    
    print("Memory Usage:")
    print(f"  Mean:       {stats['memory_usage']['mean']:.2f} MB")
    print(f"  Peak:       {stats['memory_usage']['peak']:.2f} MB")
    print(f"  Min:        {stats['memory_usage']['min']:.2f} MB")
    print()
    
    print("Cache Performance:")
    print(f"  Hits:       {stats['cache']['hits']:,}")
    print(f"  Misses:     {stats['cache']['misses']:,}")
    print(f"  Hit Rate:   {stats['cache']['hit_rate']:.2f}%")
    print()
    
    print("=" * 80)

def generate_ascii_chart(data, key, title, unit, height=20, width=60):
    """Generate an ASCII chart for visualization."""
    values = [d[key] for d in data]
    min_val = min(values)
    max_val = max(values)
    range_val = max_val - min_val if max_val != min_val else 1
    
    print(f"\n{title}")
    print("=" * width)
    
    # Sample data points to fit width
    step = max(1, len(values) // width)
    sampled = [values[i] for i in range(0, len(values), step)]
    
    # Normalize to chart height
    normalized = [int((v - min_val) / range_val * (height - 1)) for v in sampled]
    
    # Draw chart
    for row in range(height - 1, -1, -1):
        line = ""
        for val in normalized:
            if val >= row:
                line += "█"
            else:
                line += " "
        
        # Add y-axis label
        y_val = min_val + (row / (height - 1)) * range_val
        print(f"{y_val:6.2f} {unit} |{line}|")
    
    print(" " * 11 + "-" * (width + 2))
    print(f"           Frame 0{' ' * (width - 15)}Frame {len(values)}")
    print()

def try_matplotlib_plots(data, output_dir):
    """Try to generate matplotlib plots if available."""
    try:
        import matplotlib.pyplot as plt
        import matplotlib
        matplotlib.use('Agg')  # Non-interactive backend
        
        output_path = Path(output_dir)
        output_path.mkdir(exist_ok=True)
        
        frames = [d['frame'] for d in data]
        
        # Frame times plot
        plt.figure(figsize=(12, 6))
        plt.plot(frames, [d['frame_time_ms'] for d in data], linewidth=1)
        plt.axhline(y=16.67, color='r', linestyle='--', label='60 FPS Target (16.67ms)')
        plt.xlabel('Frame Number')
        plt.ylabel('Frame Time (ms)')
        plt.title('LGX Runtime - Frame Time Performance')
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.savefig(output_path / 'frame_times.png', dpi=150, bbox_inches='tight')
        plt.close()
        
        # Memory usage plot
        plt.figure(figsize=(12, 6))
        plt.plot(frames, [d['total_memory_mb'] for d in data], linewidth=1, color='green')
        plt.xlabel('Frame Number')
        plt.ylabel('Memory Usage (MB)')
        plt.title('LGX Runtime - Memory Usage Over Time')
        plt.grid(True, alpha=0.3)
        plt.savefig(output_path / 'memory_usage.png', dpi=150, bbox_inches='tight')
        plt.close()
        
        # Allocation time plot
        plt.figure(figsize=(12, 6))
        plt.plot(frames, [d['allocation_time_us'] for d in data], linewidth=1, color='orange')
        plt.xlabel('Frame Number')
        plt.ylabel('Allocation Time (μs)')
        plt.title('LGX Runtime - Allocation Performance')
        plt.grid(True, alpha=0.3)
        plt.savefig(output_path / 'allocation_time.png', dpi=150, bbox_inches='tight')
        plt.close()
        
        # Allocations per frame
        plt.figure(figsize=(12, 6))
        plt.plot(frames, [d['frame_allocations'] for d in data], linewidth=1, color='purple')
        plt.xlabel('Frame Number')
        plt.ylabel('Allocations per Frame')
        plt.title('LGX Runtime - Allocation Count')
        plt.grid(True, alpha=0.3)
        plt.savefig(output_path / 'allocations_per_frame.png', dpi=150, bbox_inches='tight')
        plt.close()
        
        print(f"\nMatplotlib plots saved to {output_path}/")
        print("  - frame_times.png")
        print("  - memory_usage.png")
        print("  - allocation_time.png")
        print("  - allocations_per_frame.png")
        
        return True
    except ImportError:
        print("\nMatplotlib not available. Install with: pip3 install matplotlib")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_results.py <demo_results.csv>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    if not Path(filename).exists():
        print(f"Error: File '{filename}' not found")
        sys.exit(1)
    
    print(f"Loading data from {filename}...")
    data = load_data(filename)
    print(f"Loaded {len(data)} frames\n")
    
    # Calculate and print statistics
    stats = calculate_statistics(data)
    print_report(stats)
    
    # Generate ASCII charts
    generate_ascii_chart(data, 'frame_time_ms', 'Frame Time Over Time', 'ms')
    generate_ascii_chart(data, 'total_memory_mb', 'Memory Usage Over Time', 'MB')
    
    # Try to generate matplotlib plots
    output_dir = Path(filename).parent / 'plots'
    try_matplotlib_plots(data, output_dir)
    
    print("\nAnalysis complete!")

if __name__ == '__main__':
    main()
