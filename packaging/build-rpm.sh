#!/bin/bash
# Build RPM package for LGX Runtime Core (Fedora/RHEL)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building LGX Runtime RPM package${NC}"
echo "Project root: $PROJECT_ROOT"

# Check if we're in the right directory
if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found in $PROJECT_ROOT${NC}"
    exit 1
fi

# Check for required tools
for tool in rpmbuild cmake; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: $tool is not installed${NC}"
        echo "Install with: sudo dnf install rpm-build rpmdevtools cmake gcc"
        exit 1
    fi
done

# Get version from CMakeLists.txt
VERSION=$(grep "^project.*VERSION" "$PROJECT_ROOT/CMakeLists.txt" | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')
if [ -z "$VERSION" ]; then
    VERSION="1.0.0"
fi

echo "Building version: $VERSION"

# Set up RPM build environment
echo -e "${YELLOW}Setting up RPM build environment...${NC}"
RPMBUILD_DIR="$HOME/rpmbuild"
mkdir -p "$RPMBUILD_DIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Create source tarball
echo -e "${YELLOW}Creating source tarball...${NC}"
TARBALL_NAME="lgx-runtime-${VERSION}.tar.gz"
cd "$PROJECT_ROOT/.."
tar --exclude='.git' \
    --exclude='build*' \
    --exclude='packaging/output' \
    --exclude='*.deb' \
    --exclude='*.rpm' \
    -czf "$RPMBUILD_DIR/SOURCES/$TARBALL_NAME" \
    "$(basename "$PROJECT_ROOT")"

# Copy spec file
echo -e "${YELLOW}Copying spec file...${NC}"
cp "$SCRIPT_DIR/rpm/lgx-runtime.spec" "$RPMBUILD_DIR/SPECS/"

# Build RPM
echo -e "${YELLOW}Building RPM package...${NC}"
cd "$RPMBUILD_DIR"
rpmbuild -ba SPECS/lgx-runtime.spec

# Copy packages to output directory
echo -e "${YELLOW}Copying packages to packaging/output/...${NC}"
mkdir -p "$SCRIPT_DIR/output"
cp -v "$RPMBUILD_DIR/RPMS"/*/*.rpm "$SCRIPT_DIR/output/" 2>/dev/null || true
cp -v "$RPMBUILD_DIR/SRPMS"/*.rpm "$SCRIPT_DIR/output/" 2>/dev/null || true

echo -e "${GREEN}✓ Package built successfully!${NC}"
echo -e "${GREEN}Packages available in: $SCRIPT_DIR/output/${NC}"
ls -lh "$SCRIPT_DIR/output/"/*.rpm 2>/dev/null || true

echo ""
echo -e "${YELLOW}To install:${NC}"
echo "  sudo dnf install $SCRIPT_DIR/output/lgx-runtime-*.rpm"
echo "  sudo dnf install $SCRIPT_DIR/output/lgx-runtime-devel-*.rpm"
echo ""
echo -e "${YELLOW}To test installation:${NC}"
echo "  pkg-config --modversion lgx_runtime"
echo "  pkg-config --cflags --libs lgx_runtime"
