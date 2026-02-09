#!/bin/bash
# Run clang-tidy on LGX Runtime

set -e

echo "=== Running clang-tidy ==="

# Check if clang-tidy is installed
if ! command -v clang-tidy &> /dev/null; then
    echo "Error: clang-tidy not found. Please install clang-tools:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-tidy"
    echo "  Fedora: sudo dnf install clang-tools-extra"
    exit 1
fi

# Create build directory with compile_commands.json
BUILD_DIR="build_tidy"
mkdir -p "$BUILD_DIR"

echo "Generating compile_commands.json..."
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy on all source files
echo "Running clang-tidy on source files..."

# Find all C source files
SOURCE_FILES=$(find src -name "*.c" -type f)

# clang-tidy checks to enable
CHECKS="-*,\
bugprone-*,\
cert-*,\
clang-analyzer-*,\
concurrency-*,\
cppcoreguidelines-*,\
misc-*,\
modernize-*,\
performance-*,\
portability-*,\
readability-*,\
security-*"

# Run clang-tidy on each file
ISSUE_COUNT=0
for file in $SOURCE_FILES; do
    echo "Analyzing: $file"
    clang-tidy \
        -p="$BUILD_DIR" \
        --checks="$CHECKS" \
        --warnings-as-errors="" \
        "$file" 2>&1 | tee -a clang_tidy_report.txt
    
    # Count warnings
    WARNINGS=$(grep -c "warning:" clang_tidy_report.txt || true)
    ISSUE_COUNT=$((ISSUE_COUNT + WARNINGS))
done

echo ""
echo "=== clang-tidy Complete ==="
echo ""
echo "Total issues found: $ISSUE_COUNT"
echo "Full report saved to: clang_tidy_report.txt"
echo ""

if [ "$ISSUE_COUNT" -eq 0 ]; then
    echo "No issues found - clean analysis!"
else
    echo "Please review clang_tidy_report.txt for details"
fi
