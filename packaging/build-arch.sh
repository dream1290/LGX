#!/bin/bash
# Build Arch Linux package for LGX Runtime Core

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building LGX Runtime Arch Linux package${NC}"
echo "Project root: $PROJECT_ROOT"

# Check if we're in the right directory
if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found in $PROJECT_ROOT${NC}"
    exit 1
fi

# Check for required tools
for tool in makepkg cmake; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: $tool is not installed${NC}"
        echo "Install with: sudo pacman -S base-devel cmake"
        exit 1
    fi
done

# Get version from CMakeLists.txt
VERSION=$(grep "^project.*VERSION" "$PROJECT_ROOT/CMakeLists.txt" | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')
if [ -z "$VERSION" ]; then
    VERSION="1.0.0"
fi

echo "Building version: $VERSION"

# Create build directory
BUILD_DIR="$SCRIPT_DIR/arch-build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Copy PKGBUILD
cp "$SCRIPT_DIR/arch/PKGBUILD" "$BUILD_DIR/"

# Create source tarball
echo -e "${YELLOW}Creating source tarball...${NC}"
TARBALL_NAME="lgx-runtime-${VERSION}.tar.gz"
cd "$PROJECT_ROOT/.."
tar --exclude='.git' \
    --exclude='build*' \
    --exclude='packaging/output' \
    --exclude='packaging/arch-build' \
    --exclude='*.deb' \
    --exclude='*.rpm' \
    --exclude='*.pkg.tar.zst' \
    -czf "$BUILD_DIR/$TARBALL_NAME" \
    "$(basename "$PROJECT_ROOT")"

# Build package
echo -e "${YELLOW}Building package...${NC}"
cd "$BUILD_DIR"

# Update PKGBUILD to use local source
sed -i "s|source=(.*)|source=(\"${TARBALL_NAME}\")|" PKGBUILD

# Build
makepkg -sf --noconfirm

# Copy packages to output directory
echo -e "${YELLOW}Copying packages to packaging/output/...${NC}"
mkdir -p "$SCRIPT_DIR/output"
cp -v *.pkg.tar.zst "$SCRIPT_DIR/output/" 2>/dev/null || true

echo -e "${GREEN}✓ Package built successfully!${NC}"
echo -e "${GREEN}Packages available in: $SCRIPT_DIR/output/${NC}"
ls -lh "$SCRIPT_DIR/output/"/*.pkg.tar.zst 2>/dev/null || true

echo ""
echo -e "${YELLOW}To install:${NC}"
echo "  sudo pacman -U $SCRIPT_DIR/output/lgx-runtime-*.pkg.tar.zst"
echo ""
echo -e "${YELLOW}To test installation:${NC}"
echo "  pkg-config --modversion lgx_runtime"
echo "  pkg-config --cflags --libs lgx_runtime"
echo ""
echo -e "${YELLOW}To publish to AUR:${NC}"
echo "  1. Update .SRCINFO: cd packaging/arch && makepkg --printsrcinfo > .SRCINFO"
echo "  2. Clone AUR repo: git clone ssh://aur@aur.archlinux.org/lgx-runtime.git"
echo "  3. Copy PKGBUILD and .SRCINFO to AUR repo"
echo "  4. Commit and push: git add . && git commit -m 'Update to ${VERSION}' && git push"
