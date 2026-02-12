#!/usr/bin/env python3
"""
Frame Arena Visualization Tool (Task 3.4.5.4.2)

Reads JSON dumps from lgx_frame_arena_dump() and generates visualizations
of arena usage over time, allocation patterns, and call site breakdowns.

Usage:
    python3 visualize_frame_arena.py <json_file> [output_dir]
    python3 visualize_frame_arena.py --multi <json_dir> [output_dir]

Examples:
    # Single snapshot
    python3 visualize_frame_arena.py /tmp/arena_dump.json ./output

    # Multiple snapshots (time series)
    python3 visualize_frame_arena.py --multi ./arena_dumps/ ./output
"""

import json
import sys
import os
from pathlib import Path

try:
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available. Install with: pip3 install matplotlib")

def load_arena_dump(json_path):
    """Load arena dump from JSON file."""
    with open(json_path, 'r') as f:
        return json.load(f)

def format_bytes(bytes_val):
    """Format bytes as human-readable string."""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if bytes_val < 1024.0:
            return f"{bytes_val:.2f} {unit}"
        bytes_val /= 1024.0
    return f"{bytes_val:.2f} TB"

def print_summary(data):
    """Print text summary of arena dump."""
    print("\n" + "="*60)
    print("FRAME ARENA SUMMARY")
    print("="*60)
    
    print(f"\nTimestamp: {data['timestamp_ns']} ns")
    print(f"Current Frame: {data['current_frame']}")
    print(f"Arena Count: {data['arena_count']}")
    
    stats = data['global_stats']
    print(f"\nGlobal Statistics:")
    print(f"  Total Allocations: {stats['total_allocations']:,}")
    print(f"  Total Bytes: {format_bytes(stats['total_bytes_allocated'])}")
    print(f"  Peak Usage: {format_bytes(stats['peak_usage_bytes'])}")
    print(f"  Overflow Count: {stats['overflow_count']}")
    print(f"  Fallback Count: {stats['fallback_count']}")
    print(f"  Fallback Bytes: {format_bytes(stats['fallback_bytes'])}")
    print(f"  Reset Count: {stats['reset_count']}")
    
    print(f"\nPer-Arena Details:")
    for arena in data['arenas']:
        status = "CURRENT" if arena['is_current'] else "idle"
        print(f"\n  Arena {arena['arena_index']} ({status}):")
        print(f"    Capacity: {format_bytes(arena['capacity'])}")
        print(f"    Usage: {format_bytes(arena['current_offset'])} ({arena['usage_percent']:.1f}%)")
        print(f"    Peak: {format_bytes(arena['peak_usage'])}")
        print(f"    Allocations: {arena['allocations']:,}")
        print(f"    Overflow: {'YES' if arena['overflow_occurred'] else 'NO'}")
        
        if arena['top_call_sites']:
            print(f"    Top Call Sites:")
            for i, site in enumerate(arena['top_call_sites'][:5], 1):
                print(f"      {i}. {site['file']}:{site['line']} - {format_bytes(site['total_bytes'])} ({site['count']} allocs)")

def plot_arena_usage(data, output_dir):
    """Generate arena usage bar chart."""
    if not HAS_MATPLOTLIB:
        return
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    arenas = data['arenas']
    indices = [a['arena_index'] for a in arenas]
    usage = [a['current_offset'] / (1024*1024) for a in arenas]  # MB
    capacity = [a['capacity'] / (1024*1024) for a in arenas]  # MB
    
    x = np.arange(len(indices))
    width = 0.35
    
    bars1 = ax.bar(x - width/2, usage, width, label='Current Usage', color='#2ecc71')
    bars2 = ax.bar(x + width/2, capacity, width, label='Capacity', color='#3498db', alpha=0.5)
    
    ax.set_xlabel('Arena Index')
    ax.set_ylabel('Size (MB)')
    ax.set_title(f'Frame Arena Usage (Frame {data["current_frame"]})')
    ax.set_xticks(x)
    ax.set_xticklabels([f"Arena {i}" for i in indices])
    ax.legend()
    ax.grid(axis='y', alpha=0.3)
    
    # Add percentage labels on bars
    for i, (bar, pct) in enumerate(zip(bars1, [a['usage_percent'] for a in arenas])):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{pct:.1f}%', ha='center', va='bottom', fontsize=9)
    
    plt.tight_layout()
    output_path = os.path.join(output_dir, 'arena_usage.png')
    plt.savefig(output_path, dpi=150)
    print(f"  ✓ Saved: {output_path}")
    plt.close()

def plot_size_histogram(data, output_dir):
    """Generate allocation size histogram."""
    if not HAS_MATPLOTLIB:
        return
    
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle(f'Allocation Size Distribution (Frame {data["current_frame"]})')
    
    for idx, arena in enumerate(data['arenas']):
        ax = axes[idx]
        histogram = arena['size_histogram']
        
        if not histogram:
            ax.text(0.5, 0.5, 'No allocations', ha='center', va='center', transform=ax.transAxes)
            ax.set_title(f'Arena {arena["arena_index"]}')
            continue
        
        labels = [f"{h['min_size']}-{h['max_size']}B" for h in histogram]
        counts = [h['count'] for h in histogram]
        
        # Limit to top 10 for readability
        if len(labels) > 10:
            labels = labels[:10]
            counts = counts[:10]
        
        ax.barh(range(len(labels)), counts, color='#e74c3c')
        ax.set_yticks(range(len(labels)))
        ax.set_yticklabels(labels, fontsize=8)
        ax.set_xlabel('Count')
        ax.set_title(f'Arena {arena["arena_index"]}')
        ax.grid(axis='x', alpha=0.3)
    
    plt.tight_layout()
    output_path = os.path.join(output_dir, 'size_histogram.png')
    plt.savefig(output_path, dpi=150)
    print(f"  ✓ Saved: {output_path}")
    plt.close()

def plot_call_sites(data, output_dir):
    """Generate call site breakdown pie chart."""
    if not HAS_MATPLOTLIB:
        return
    
    # Aggregate call sites across all arenas
    call_site_map = {}
    for arena in data['arenas']:
        for site in arena['top_call_sites']:
            key = f"{site['file']}:{site['line']}"
            if key in call_site_map:
                call_site_map[key] += site['total_bytes']
            else:
                call_site_map[key] = site['total_bytes']
    
    if not call_site_map:
        print("  ⚠ No call site data available")
        return
    
    # Sort and take top 10
    sorted_sites = sorted(call_site_map.items(), key=lambda x: x[1], reverse=True)[:10]
    labels = [site[0] for site in sorted_sites]
    sizes = [site[1] / (1024*1024) for site in sorted_sites]  # MB
    
    fig, ax = plt.subplots(figsize=(10, 8))
    colors = plt.cm.Set3(np.linspace(0, 1, len(labels)))
    
    wedges, texts, autotexts = ax.pie(sizes, labels=None, autopct='%1.1f%%',
                                        colors=colors, startangle=90)
    
    # Create legend with labels
    ax.legend(wedges, [f"{l} ({format_bytes(s*1024*1024)})" for l, s in zip(labels, sizes)],
              title="Call Sites", loc="center left", bbox_to_anchor=(1, 0, 0.5, 1),
              fontsize=8)
    
    ax.set_title(f'Top Allocation Call Sites (Frame {data["current_frame"]})')
    
    plt.tight_layout()
    output_path = os.path.join(output_dir, 'call_sites.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")
    plt.close()

def visualize_single(json_path, output_dir):
    """Visualize a single arena dump."""
    print(f"\nLoading: {json_path}")
    data = load_arena_dump(json_path)
    
    # Print text summary
    print_summary(data)
    
    # Generate visualizations
    if HAS_MATPLOTLIB:
        print(f"\nGenerating visualizations...")
        os.makedirs(output_dir, exist_ok=True)
        
        plot_arena_usage(data, output_dir)
        plot_size_histogram(data, output_dir)
        plot_call_sites(data, output_dir)
        
        print(f"\n✅ Visualizations saved to: {output_dir}")
    else:
        print("\n⚠ Matplotlib not available. Only text summary generated.")

def visualize_multi(json_dir, output_dir):
    """Visualize multiple arena dumps as time series."""
    print(f"\nScanning directory: {json_dir}")
    
    json_files = sorted(Path(json_dir).glob('*.json'))
    if not json_files:
        print(f"❌ No JSON files found in {json_dir}")
        return
    
    print(f"Found {len(json_files)} JSON files")
    
    # Load all dumps
    dumps = []
    for json_file in json_files:
        try:
            data = load_arena_dump(json_file)
            dumps.append(data)
        except Exception as e:
            print(f"  ⚠ Skipping {json_file}: {e}")
    
    if not dumps:
        print("❌ No valid dumps loaded")
        return
    
    print(f"Loaded {len(dumps)} dumps")
    
    if not HAS_MATPLOTLIB:
        print("⚠ Matplotlib not available. Cannot generate time series plots.")
        return
    
    # Generate time series plot
    print("\nGenerating time series plot...")
    os.makedirs(output_dir, exist_ok=True)
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    
    frames = [d['current_frame'] for d in dumps]
    peak_usage = [d['global_stats']['peak_usage_bytes'] / (1024*1024) for d in dumps]
    overflow_count = [d['global_stats']['overflow_count'] for d in dumps]
    
    # Plot 1: Peak usage over time
    ax1.plot(frames, peak_usage, marker='o', color='#2ecc71', linewidth=2)
    ax1.set_xlabel('Frame Number')
    ax1.set_ylabel('Peak Usage (MB)')
    ax1.set_title('Frame Arena Peak Usage Over Time')
    ax1.grid(True, alpha=0.3)
    
    # Plot 2: Overflow count over time
    ax2.plot(frames, overflow_count, marker='s', color='#e74c3c', linewidth=2)
    ax2.set_xlabel('Frame Number')
    ax2.set_ylabel('Overflow Count')
    ax2.set_title('Frame Arena Overflow Count Over Time')
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    output_path = os.path.join(output_dir, 'time_series.png')
    plt.savefig(output_path, dpi=150)
    print(f"  ✓ Saved: {output_path}")
    plt.close()
    
    print(f"\n✅ Time series visualization saved to: {output_dir}")

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    if sys.argv[1] == '--multi':
        if len(sys.argv) < 3:
            print("Error: --multi requires <json_dir>")
            sys.exit(1)
        json_dir = sys.argv[2]
        output_dir = sys.argv[3] if len(sys.argv) > 3 else './arena_viz'
        visualize_multi(json_dir, output_dir)
    else:
        json_path = sys.argv[1]
        output_dir = sys.argv[2] if len(sys.argv) > 2 else './arena_viz'
        visualize_single(json_path, output_dir)

if __name__ == '__main__':
    main()
