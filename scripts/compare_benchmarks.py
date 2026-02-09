#!/usr/bin/env python3
"""
Performance Regression Detection Script

Compares current benchmark results against baseline and detects regressions.
"""

import sys
import os
import argparse
from typing import Dict, List, Tuple
import statistics

def parse_benchmark_file(filepath: str) -> Dict[str, str]:
    """Parse a benchmark results file into a dictionary."""
    results = {}
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if '=' in line:
                    key, value = line.split('=', 1)
                    results[key] = value
    except FileNotFoundError:
        print(f"Warning: File not found: {filepath}", file=sys.stderr)
        return {}
    return results

def compare_metric(baseline_val: float, current_val: float, threshold_percent: float = 5.0) -> Tuple[bool, float]:
    """
    Compare a metric against baseline.
    
    Returns:
        (is_regression, percent_change)
    """
    if baseline_val == 0:
        return False, 0.0
    
    percent_change = ((current_val - baseline_val) / baseline_val) * 100.0
    is_regression = percent_change > threshold_percent
    
    return is_regression, percent_change

def format_time_ns(ns: int) -> str:
    """Format nanoseconds in human-readable form."""
    if ns < 1000:
        return f"{ns} ns"
    elif ns < 1000000:
        return f"{ns/1000:.2f} μs"
    else:
        return f"{ns/1000000:.2f} ms"

def compare_benchmarks(baseline_dir: str, current_dir: str, threshold: float = 5.0, variance: float = 2.0) -> int:
    """
    Compare benchmark results and detect regressions.
    
    Args:
        baseline_dir: Directory containing baseline results
        current_dir: Directory containing current results
        threshold: Regression threshold percentage (default 5%)
        variance: Allowed variance percentage (default 2%)
    
    Returns:
        0 if no regressions, 1 if regressions detected
    """
    
    benchmark_files = [
        'benchmark_results_init.txt',
        'benchmark_results_alloc.txt',
        'benchmark_results_frame.txt',
        'benchmark_results_memory.txt'
    ]
    
    regressions_found = False
    total_comparisons = 0
    regression_count = 0
    
    print("=" * 80)
    print("PERFORMANCE REGRESSION DETECTION REPORT")
    print("=" * 80)
    print(f"Threshold: {threshold}% regression")
    print(f"Variance:  {variance}% allowed")
    print("=" * 80)
    print()
    
    for bench_file in benchmark_files:
        baseline_path = os.path.join(baseline_dir, bench_file)
        current_path = os.path.join(current_dir, bench_file)
        
        baseline = parse_benchmark_file(baseline_path)
        current = parse_benchmark_file(current_path)
        
        if not baseline or not current:
            print(f"⚠️  Skipping {bench_file} (missing baseline or current)")
            print()
            continue
        
        benchmark_name = baseline.get('benchmark', bench_file)
        print(f"📊 {benchmark_name.upper()}")
        print("-" * 80)
        
        # Find all numeric metrics ending in _ns or _bytes
        metrics_to_compare = []
        for key in current.keys():
            if key.endswith('_ns') or key.endswith('_bytes') or key.endswith('_percent'):
                if key in baseline:
                    metrics_to_compare.append(key)
        
        bench_regressions = []
        
        for metric in sorted(metrics_to_compare):
            try:
                baseline_val = float(baseline[metric])
                current_val = float(current[metric])
                
                is_regression, percent_change = compare_metric(baseline_val, current_val, threshold + variance)
                total_comparisons += 1
                
                # Format values based on metric type
                if metric.endswith('_ns'):
                    baseline_str = format_time_ns(int(baseline_val))
                    current_str = format_time_ns(int(current_val))
                elif metric.endswith('_bytes'):
                    baseline_str = f"{baseline_val/(1024*1024):.2f} MB"
                    current_str = f"{current_val/(1024*1024):.2f} MB"
                else:
                    baseline_str = f"{baseline_val:.2f}"
                    current_str = f"{current_val:.2f}"
                
                status = "❌ REGRESSION" if is_regression else "✅ OK"
                
                if is_regression:
                    bench_regressions.append((metric, percent_change, baseline_str, current_str))
                    regression_count += 1
                    regressions_found = True
                
                print(f"  {status:15} {metric:30} {baseline_str:15} → {current_str:15} ({percent_change:+.1f}%)")
                
            except (ValueError, KeyError) as e:
                print(f"  ⚠️  WARNING     {metric:30} (parse error: {e})")
        
        if bench_regressions:
            print()
            print(f"  ⚠️  {len(bench_regressions)} regression(s) detected in this benchmark")
        
        print()
    
    # Summary
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"Total comparisons: {total_comparisons}")
    print(f"Regressions found: {regression_count}")
    print()
    
    if regressions_found:
        print("❌ PERFORMANCE REGRESSION DETECTED")
        print()
        print("Action required:")
        print("  1. Review the regressions above")
        print("  2. Investigate the cause of performance degradation")
        print("  3. Fix the regression or update baseline if intentional")
        print()
        return 1
    else:
        print("✅ NO PERFORMANCE REGRESSIONS DETECTED")
        print()
        return 0

def main():
    parser = argparse.ArgumentParser(description='Compare benchmark results and detect regressions')
    parser.add_argument('--baseline', required=True, help='Directory containing baseline results')
    parser.add_argument('--current', required=True, help='Directory containing current results')
    parser.add_argument('--threshold', type=float, default=5.0, help='Regression threshold percentage (default: 5%%)')
    parser.add_argument('--variance', type=float, default=2.0, help='Allowed variance percentage (default: 2%%)')
    
    args = parser.parse_args()
    
    if not os.path.isdir(args.baseline):
        print(f"Error: Baseline directory not found: {args.baseline}", file=sys.stderr)
        return 1
    
    if not os.path.isdir(args.current):
        print(f"Error: Current directory not found: {args.current}", file=sys.stderr)
        return 1
    
    return compare_benchmarks(args.baseline, args.current, args.threshold, args.variance)

if __name__ == '__main__':
    sys.exit(main())
