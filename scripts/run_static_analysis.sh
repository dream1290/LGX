#!/bin/bash
# Run Clang Static Analyzer on LGX Runtime

set -e

echo "=== Running Clang Static Analyzer ==="

# Check if scan-build is installed
if ! command -v scan-build &> /dev/null; then
    echo "Error: scan-build not found. Please install clang-tools:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-tools"
    echo "  Fedora: sudo dnf install clang-analyzer"
    exit 1
fi

# Create analysis output directory
ANALYSIS_DIR="build_analysis"
REPORT_DIR="static_analysis_reports"
mkdir -p "$REPORT_DIR"

# Clean previous build
rm -rf "$ANALYSIS_DIR"

echo "Running static analysis..."
echo "This may take several minutes..."

# Run scan-build with comprehensive checks
scan-build \
    --use-cc=clang \
    --use-c++=clang++ \
    -o "$REPORT_DIR" \
    -enable-checker alpha.core.BoolAssignment \
    -enable-checker alpha.core.CastSize \
    -enable-checker alpha.core.CastToStruct \
    -enable-checker alpha.core.FixedAddr \
    -enable-checker alpha.core.PointerArithm \
    -enable-checker alpha.core.PointerSub \
    -enable-checker alpha.core.SizeofPtr \
    -enable-checker alpha.core.TestAfterDivZero \
    -enable-checker alpha.deadcode.UnreachableCode \
    -enable-checker alpha.security.ArrayBound \
    -enable-checker alpha.security.ArrayBoundV2 \
    -enable-checker alpha.security.MallocOverflow \
    -enable-checker alpha.security.ReturnPtrRange \
    -enable-checker alpha.unix.cstring.BufferOverlap \
    -enable-checker alpha.unix.cstring.NotNullTerminated \
    -enable-checker alpha.unix.cstring.OutOfBounds \
    -enable-checker security.insecureAPI.strcpy \
    -enable-checker security.insecureAPI.DeprecatedOrUnsafeBufferHandling \
    cmake -B "$ANALYSIS_DIR" -S . -DCMAKE_BUILD_TYPE=Debug

# Build with analysis
scan-build \
    --use-cc=clang \
    --use-c++=clang++ \
    -o "$REPORT_DIR" \
    -enable-checker alpha.core.BoolAssignment \
    -enable-checker alpha.core.CastSize \
    -enable-checker alpha.core.CastToStruct \
    -enable-checker alpha.core.FixedAddr \
    -enable-checker alpha.core.PointerArithm \
    -enable-checker alpha.core.PointerSub \
    -enable-checker alpha.core.SizeofPtr \
    -enable-checker alpha.core.TestAfterDivZero \
    -enable-checker alpha.deadcode.UnreachableCode \
    -enable-checker alpha.security.ArrayBound \
    -enable-checker alpha.security.ArrayBoundV2 \
    -enable-checker alpha.security.MallocOverflow \
    -enable-checker alpha.security.ReturnPtrRange \
    -enable-checker alpha.unix.cstring.BufferOverlap \
    -enable-checker alpha.unix.cstring.NotNullTerminated \
    -enable-checker alpha.unix.cstring.OutOfBounds \
    -enable-checker security.insecureAPI.strcpy \
    -enable-checker security.insecureAPI.DeprecatedOrUnsafeBufferHandling \
    cmake --build "$ANALYSIS_DIR" -j$(nproc)

echo ""
echo "=== Static Analysis Complete ==="
echo ""

# Check for reports
LATEST_REPORT=$(ls -td "$REPORT_DIR"/*/ 2>/dev/null | head -1)

if [ -n "$LATEST_REPORT" ]; then
    echo "Analysis reports generated in: $LATEST_REPORT"
    echo ""
    echo "To view the report:"
    echo "  scan-view $LATEST_REPORT"
    echo ""
    
    # Count issues
    ISSUE_COUNT=$(find "$LATEST_REPORT" -name "*.html" | wc -l)
    if [ "$ISSUE_COUNT" -gt 0 ]; then
        echo "Found $ISSUE_COUNT potential issues"
        echo "Please review the HTML reports"
    else
        echo "No issues found!"
    fi
else
    echo "No issues found - clean analysis!"
fi

echo ""
echo "Analysis artifacts saved in: $REPORT_DIR"
