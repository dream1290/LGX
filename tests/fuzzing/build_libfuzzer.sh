#!/bin/bash
# Build script for libFuzzer

set -e

echo "=== Building LGX Runtime with libFuzzer ==="

# Check if clang is installed
if ! command -v clang++ &> /dev/null; then
    echo "Error: clang++ not found. Please install clang:"
    echo "  Ubuntu/Debian: sudo apt-get install clang"
    echo "  Fedora: sudo dnf install clang"
    exit 1
fi

# Build directory
BUILD_DIR="../../build_libfuzzer"
mkdir -p "$BUILD_DIR"

# Build LGX Runtime with AddressSanitizer (compatible with libFuzzer)
echo "Building LGX Runtime with AddressSanitizer..."
cd ../..
CC=clang CXX=clang++ cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-g -O1 -fsanitize=address" \
    -DCMAKE_CXX_FLAGS="-g -O1 -fsanitize=address"

cmake --build "$BUILD_DIR" -j$(nproc)

# Build libFuzzer harness
echo "Building libFuzzer harness..."
cd tests/fuzzing
clang++ -fsanitize=fuzzer,address -g -O1 \
    -I../../include \
    -I../../include/lgx \
    -L"$BUILD_DIR" \
    -llgx_runtime \
    -lpthread \
    -lm \
    -ldl \
    -Wl,-rpath,"$BUILD_DIR" \
    fuzz_allocation_patterns.cpp \
    -o fuzz_allocation_patterns

echo "=== libFuzzer Build Complete ==="
echo ""
echo "To run libFuzzer:"
echo "  cd tests/fuzzing"
echo "  ./fuzz_allocation_patterns -max_total_time=300"
echo ""
echo "To run with corpus:"
echo "  mkdir -p corpus"
echo "  ./fuzz_allocation_patterns corpus/ -max_total_time=300"
echo ""
echo "To minimize corpus:"
echo "  ./fuzz_allocation_patterns -merge=1 corpus_min/ corpus/"
