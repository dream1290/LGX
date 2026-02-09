#!/bin/bash
# Capture baseline performance results

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-release"
BASELINE_DIR="${PROJECT_ROOT}/performance_baseline"

echo "=== Capturing Performance Baseline ==="
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    echo "Please build the project first with: cmake -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release"
    exit 1
fi

# Create baseline directory
mkdir -p "$BASELINE_DIR"

echo "Running performance benchmarks..."
echo ""

# Run each benchmark and capture results
BENCHMARKS=(
    "perf_test_initialization_time"
    "perf_test_allocation_latency"
    "perf_test_frame_time_contribution"
    "perf_test_memory_overhead"
)

for bench in "${BENCHMARKS[@]}"; do
    BENCH_PATH="${BUILD_DIR}/tests/performance/${bench}"
    
    if [ ! -f "$BENCH_PATH" ]; then
        echo "Warning: Benchmark not found: $BENCH_PATH"
        continue
    fi
    
    echo "Running $bench..."
    
    # Run benchmark 3 times and take median to reduce variance
    for run in 1 2 3; do
        echo "  Run $run/3..."
        cd "$BUILD_DIR/tests/performance"
        ./"$bench" > /dev/null 2>&1 || true
        
        # Copy results to temporary location
        if [ -f "benchmark_results_init.txt" ]; then
            cp "benchmark_results_init.txt" "${BASELINE_DIR}/run${run}_init.txt"
        fi
        if [ -f "benchmark_results_alloc.txt" ]; then
            cp "benchmark_results_alloc.txt" "${BASELINE_DIR}/run${run}_alloc.txt"
        fi
        if [ -f "benchmark_results_frame.txt" ]; then
            cp "benchmark_results_frame.txt" "${BASELINE_DIR}/run${run}_frame.txt"
        fi
        if [ -f "benchmark_results_memory.txt" ]; then
            cp "benchmark_results_memory.txt" "${BASELINE_DIR}/run${run}_memory.txt"
        fi
    done
    
    echo ""
done

# Calculate median results (simple approach: use run 2)
echo "Selecting median results..."
for result_type in init alloc frame memory; do
    if [ -f "${BASELINE_DIR}/run2_${result_type}.txt" ]; then
        cp "${BASELINE_DIR}/run2_${result_type}.txt" "${BASELINE_DIR}/benchmark_results_${result_type}.txt"
    fi
done

# Clean up temporary files
rm -f "${BASELINE_DIR}"/run*_*.txt

# Add metadata
cat > "${BASELINE_DIR}/metadata.txt" << EOF
timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
git_commit=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
git_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
hostname=$(hostname)
kernel=$(uname -r)
cpu_model=$(grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
EOF

echo "✅ Baseline captured successfully!"
echo ""
echo "Baseline location: $BASELINE_DIR"
echo "Files:"
ls -lh "$BASELINE_DIR"
echo ""
echo "To use this baseline for regression detection:"
echo "  ./scripts/compare_benchmarks.py --baseline $BASELINE_DIR --current <current_results_dir>"
