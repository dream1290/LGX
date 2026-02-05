#!/usr/bin/env python3
"""
Performance regression detection script for LGX Runtime Core.
Compares benchmark results between baseline and current builds.
"""

import argparse
import json
import sys
from typing import Dict, List, Tuple
import statistics

def parse_benchmark_results(filename: str) -> Dict[str, Dict[str, float]]:
    """Parse benchmark results from file."""
    results = {}
    
    try:
        with open(filename, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith('BENCHMARK:'):
                    # Format: BENCHMARK: test_name metric_name value unit
                    parts = line.split()
                    if len(parts) >= 5:
                        test_name = parts[1]
                        metric_name = parts[2]
                        value = float(parts[3])
                        unit = parts[4]
                        
                        if test_name not in results:
                            results[test_name] = {}
                        results[test_name][metric_name] = value
    except FileNotFoundError:
        print(f"Error: Could not find file {filename}")
        sys.exit(1)
    except Exception as e:
        print(f"Error parsing {filename}: {e}")
        sys.exit(1)
    
    return results

def calculate_regression(baseline: float, current: float) -> Tuple[float, bool]:
    """Calculate percentage regression and determine if significant."""
    if baseline == 0:
        return 0.0, False
    
    regression_pct = ((current - baseline) / baseline) * 100
    return regression_pct, abs(regression_pct) > 5.0  # 5% threshold

def generate_report(baseline_results: Dict, current_results: Dict, 
                   threshold: float) -> str:
    """Generate markdown performance report."""
    report = ["# Performance Regression Report\n"]
    
    regressions = []
    improvements = []
    stable = []
    
    all_tests = set(baseline_results.keys()) | set(current_results.keys())
    
    for test_name in sorted(all_tests):
        if test_name not in baseline_results:
            report.append(f"⚠️ **New test**: {test_name} (no baseline)")
            continue
        if test_name not in current_results:
            report.append(f"❌ **Missing test**: {test_name} (was in baseline)")
            continue
        
        baseline_metrics = baseline_results[test_name]
        current_metrics = current_results[test_name]
        
        for metric_name in baseline_metrics:
            if metric_name not in current_metrics:
                continue
                
            baseline_val = baseline_metrics[metric_name]
            current_val = current_metrics[metric_name]
            
            regression_pct, is_significant = calculate_regression(baseline_val, current_val)
            
            if is_significant:
                if regression_pct > 0:
                    regressions.append((test_name, metric_name, regression_pct, baseline_val, current_val))
                else:
                    improvements.append((test_name, metric_name, abs(regression_pct), baseline_val, current_val))
            else:
                stable.append((test_name, metric_name, regression_pct, baseline_val, current_val))
    
    # Report regressions
    if regressions:
        report.append("## 🔴 Performance Regressions\n")
        report.append("| Test | Metric | Regression | Baseline | Current |")
        report.append("|------|--------|------------|----------|---------|")
        for test, metric, pct, baseline, current in regressions:
            report.append(f"| {test} | {metric} | {pct:+.1f}% | {baseline:.3f} | {current:.3f} |")
        report.append("")
    
    # Report improvements
    if improvements:
        report.append("## 🟢 Performance Improvements\n")
        report.append("| Test | Metric | Improvement | Baseline | Current |")
        report.append("|------|--------|-------------|----------|---------|")
        for test, metric, pct, baseline, current in improvements:
            report.append(f"| {test} | {metric} | {pct:.1f}% | {baseline:.3f} | {current:.3f} |")
        report.append("")
    
    # Summary
    report.append("## Summary\n")
    report.append(f"- **Regressions**: {len(regressions)}")
    report.append(f"- **Improvements**: {len(improvements)}")
    report.append(f"- **Stable**: {len(stable)}")
    report.append(f"- **Threshold**: {threshold}%")
    
    if regressions:
        report.append("\n❌ **Performance regression detected!** Please investigate.")
        return "\n".join(report), False
    else:
        report.append("\n✅ **No significant performance regressions detected.**")
        return "\n".join(report), True

def main():
    parser = argparse.ArgumentParser(description='Compare benchmark performance results')
    parser.add_argument('baseline', help='Baseline benchmark results file')
    parser.add_argument('current', help='Current benchmark results file')
    parser.add_argument('--threshold', type=float, default=5.0, 
                       help='Regression threshold percentage (default: 5.0)')
    parser.add_argument('--output', help='Output markdown report file')
    
    args = parser.parse_args()
    
    baseline_results = parse_benchmark_results(args.baseline)
    current_results = parse_benchmark_results(args.current)
    
    report, passed = generate_report(baseline_results, current_results, args.threshold)
    
    if args.output:
        with open(args.output, 'w') as f:
            f.write(report)
    else:
        print(report)
    
    # Exit with error code if regressions detected
    sys.exit(0 if passed else 1)

if __name__ == '__main__':
    main()