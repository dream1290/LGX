#!/bin/bash
# Build script for AFL fuzzing

set -e

echo "=== Building LGX Runtime with AFL ==="

# Check if AFL is installed
if ! command -v afl-gcc &> /dev/null; then
    echo "Error: AFL (afl-gcc) not found. Please install AFL:"
    echo "  Ubuntu/Debian: sudo apt-get install afl"
    echo "  Fedora: sudo dnf install afl"
    echo "  From source: http://lcamtuf.coredump.cx/afl/"
    exit 1
fi

# Build directory
BUILD_DIR="../../build_afl"
mkdir -p "$BUILD_DIR"

# Build LGX Runtime with AFL instrumentation
echo "Building LGX Runtime with AFL instrumentation..."
cd ../..
CC=afl-gcc CXX=afl-g++ cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-g -O0"

cmake --build "$BUILD_DIR" -j$(nproc)

# Build fuzzing harness
echo "Building AFL fuzzing harnesses..."
cd tests/fuzzing

# API inputs fuzzer
afl-gcc -o fuzz_api_inputs fuzz_api_inputs.c \
    -I../../include \
    -I../../include/lgx \
    -L"$BUILD_DIR/src" \
    -llgx_runtime \
    -lpthread \
    -lm \
    -ldl \
    -Wl,-rpath,"$BUILD_DIR/src"

# Lifecycle fuzzer
afl-gcc -o fuzz_lifecycle fuzz_lifecycle.c \
    -I../../include \
    -I../../include/lgx \
    -L"$BUILD_DIR/src" \
    -llgx_runtime \
    -lpthread \
    -lm \
    -ldl \
    -Wl,-rpath,"$BUILD_DIR/src"

echo "=== AFL Build Complete ==="
echo ""
echo "To run AFL fuzzing:"
echo "  cd tests/fuzzing"
echo "  afl-fuzz -i testcases -o findings ./fuzz_api_inputs"
echo "  afl-fuzz -i testcases -o findings ./fuzz_lifecycle"
echo ""
echo "To view findings:"
echo "  ls -la findings/crashes/"
echo "  ls -la findings/hangs/"
