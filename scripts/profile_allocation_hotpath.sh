#!/bin/bash
# Profile Allocation Hot Path with perf
# Task 12.1.1: Profile allocation fast path with perf

set -e

echo "=========================================="
echo "Allocation Hot Path Profiling"
echo "=========================================="
echo ""

# Check if perf is available
if ! command -v perf &> /dev/null; then
    echo "Error: perf not found. Install with:"
    echo "  sudo apt install linux-tools-common linux-tools-generic"
    exit 1
fi

# Check if we have permissions
if ! perf stat ls &> /dev/null; then
    echo "Error: perf requires elevated permissions. Run with:"
    echo "  sudo sysctl -w kernel.perf_event_paranoid=-1"
    echo "Or run this script with sudo"
    exit 1
fi

# Build in release mode with debug symbols
echo "Step 1: Building with debug symbols..."
echo "----------------------------------------"
cmake -B build-profile \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS="-O2 -g -fno-omit-frame-pointer" \
    -DCMAKE_CXX_FLAGS="-O2 -g -fno-omit-frame-pointer"
cmake --build build-profile

echo ""
echo "Step 2: Running allocation latency benchmark..."
echo "------------------------------------------------"
cd build-profile/tests/performance

# Run benchmark to generate workload
./perf_test_allocation_latency

echo ""
echo "Step 3: Profiling with perf record..."
echo "--------------------------------------"
# Record performance data
perf record -F 99 -g --call-graph dwarf \
    ./perf_test_allocation_latency

echo ""
echo "Step 4: Analyzing hot paths..."
echo "-------------------------------"
# Generate report
perf report --stdio > ../../../perf_hotpath_report.txt

# Show top functions
echo "Top 20 hottest functions:"
perf report --stdio --no-children | head -n 50

echo ""
echo "Step 5: Analyzing cache misses..."
echo "----------------------------------"
perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
    ./perf_test_allocation_latency

echo ""
echo "Step 6: Analyzing branch mispredictions..."
echo "-------------------------------------------"
perf stat -e branches,branch-misses,branch-instructions,branch-load-misses \
    ./perf_test_allocation_latency

echo ""
echo "Step 7: Analyzing TLB misses..."
echo "--------------------------------"
perf stat -e dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses \
    ./perf_test_allocation_latency

echo ""
echo "Step 8: Generating flamegraph..."
echo "---------------------------------"
if command -v flamegraph.pl &> /dev/null; then
    perf script | flamegraph.pl > ../../../allocation_flamegraph.svg
    echo "Flamegraph saved to: allocation_flamegraph.svg"
else
    echo "Flamegraph tools not found. Install with:"
    echo "  git clone https://github.com/brendangregg/FlameGraph"
    echo "  export PATH=\$PATH:\$PWD/FlameGraph"
fi

echo ""
echo "=========================================="
echo "Profiling Complete!"
echo "=========================================="
echo ""
echo "Results:"
echo "  - Full report: perf_hotpath_report.txt"
echo "  - Flamegraph: allocation_flamegraph.svg (if available)"
echo "  - perf.data: build-profile/tests/performance/perf.data"
echo ""
echo "Next steps:"
echo "  1. Review perf_hotpath_report.txt for hot functions"
echo "  2. Identify optimization opportunities"
echo "  3. Implement optimizations (cache alignment, branch hints, prefetching)"
echo "  4. Re-profile to validate improvements"
echo ""
