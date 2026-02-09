#!/bin/bash
# Run Coverity Scan on LGX Runtime
#
# Note: Coverity Scan requires registration and download of the Coverity Build Tool
# from https://scan.coverity.com/

set -e

echo "=== Running Coverity Scan ==="

# Check if Coverity is installed
if ! command -v cov-build &> /dev/null; then
    echo "Error: Coverity Build Tool not found."
    echo ""
    echo "To use Coverity Scan:"
    echo "1. Register at https://scan.coverity.com/"
    echo "2. Download the Coverity Build Tool"
    echo "3. Extract and add to PATH:"
    echo "   export PATH=\$PATH:/path/to/cov-analysis/bin"
    echo ""
    echo "Alternatively, use the GitHub Coverity Scan Action:"
    echo "  See: .github/workflows/coverity.yml"
    exit 1
fi

# Configuration
PROJECT_NAME="lgx-runtime"
BUILD_DIR="build_coverity"
COVERITY_DIR="cov-int"

# Clean previous build
rm -rf "$BUILD_DIR" "$COVERITY_DIR" "${PROJECT_NAME}.tgz"

# Create build directory
mkdir -p "$BUILD_DIR"

echo "Configuring project..."
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Debug

echo "Running Coverity Build..."
cov-build --dir "$COVERITY_DIR" cmake --build "$BUILD_DIR" -j$(nproc)

echo "Analyzing results..."
cov-analyze --dir "$COVERITY_DIR" \
    --all \
    --enable-constraint-fpp \
    --enable-fnptr \
    --enable-virtual \
    --security \
    --concurrency \
    --rule \
    --webapp-security

echo "Generating report..."
cov-format-errors --dir "$COVERITY_DIR" \
    --html-output coverity_report

echo ""
echo "=== Coverity Scan Complete ==="
echo ""
echo "Local report generated in: coverity_report/"
echo "Open coverity_report/index.html in a browser to view"
echo ""

# Create tarball for upload to Coverity Scan service
echo "Creating tarball for Coverity Scan upload..."
tar czf "${PROJECT_NAME}.tgz" "$COVERITY_DIR"

echo ""
echo "To upload to Coverity Scan service:"
echo "  curl --form token=YOUR_TOKEN \\"
echo "    --form email=YOUR_EMAIL \\"
echo "    --form file=@${PROJECT_NAME}.tgz \\"
echo "    --form version=\"\$(git describe --tags)\" \\"
echo "    --form description=\"LGX Runtime Security Scan\" \\"
echo "    https://scan.coverity.com/builds?project=YOUR_PROJECT"
echo ""

# Count defects
if [ -d "coverity_report" ]; then
    DEFECT_COUNT=$(find coverity_report -name "*.html" | wc -l)
    echo "Found $DEFECT_COUNT potential defects"
    echo "Please review the HTML report for details"
fi
