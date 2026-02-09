#!/bin/bash
# Build Debian/Ubuntu package for LGX Runtime Core

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building LGX Runtime Debian package${NC}"
echo "Project root: $PROJECT_ROOT"

# Check if we're in the right directory
if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found in $PROJECT_ROOT${NC}"
    exit 1
fi

# Check for required tools
for tool in dpkg-buildpackage debhelper cmake; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: $tool is not installed${NC}"
        echo "Install with: sudo apt-get install build-essential debhelper cmake pkg-config"
        exit 1
    fi
done

# Create debian directory in project root
echo -e "${YELLOW}Setting up debian directory...${NC}"
rm -rf "$PROJECT_ROOT/debian"
cp -r "$SCRIPT_DIR/debian" "$PROJECT_ROOT/debian"

# Make rules executable
chmod +x "$PROJECT_ROOT/debian/rules"

# Build the package
echo -e "${YELLOW}Building package...${NC}"
cd "$PROJECT_ROOT"

# Clean previous builds
rm -rf build-deb
mkdir -p build-deb

# Build package
dpkg-buildpackage -us -uc -b

# Move packages to packaging directory
echo -e "${YELLOW}Moving packages to packaging/output/...${NC}"
mkdir -p "$SCRIPT_DIR/output"
mv ../*.deb "$SCRIPT_DIR/output/" 2>/dev/null || true
mv ../*.ddeb "$SCRIPT_DIR/output/" 2>/dev/null || true
mv ../*.buildinfo "$SCRIPT_DIR/output/" 2>/dev/null || true
mv ../*.changes "$SCRIPT_DIR/output/" 2>/dev/null || true

# Clean up debian directory
rm -rf "$PROJECT_ROOT/debian"

echo -e "${GREEN}✓ Package built successfully!${NC}"
echo -e "${GREEN}Packages available in: $SCRIPT_DIR/output/${NC}"
ls -lh "$SCRIPT_DIR/output/"

echo ""
echo -e "${YELLOW}To install:${NC}"
echo "  sudo dpkg -i $SCRIPT_DIR/output/lgx-runtime_*.deb"
echo "  sudo dpkg -i $SCRIPT_DIR/output/lgx-runtime-dev_*.deb"
echo ""
echo -e "${YELLOW}To test installation:${NC}"
echo "  pkg-config --modversion lgx_runtime"
echo "  pkg-config --cflags --libs lgx_runtime"
