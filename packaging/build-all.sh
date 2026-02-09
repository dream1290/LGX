#!/bin/bash
# Build packages for all supported distributions

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     LGX Runtime Core - Build All Packages                 ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Clean output directory
echo -e "${YELLOW}Cleaning output directory...${NC}"
rm -rf "$SCRIPT_DIR/output"
mkdir -p "$SCRIPT_DIR/output"

# Track build status
BUILD_SUCCESS=()
BUILD_FAILED=()

# Build Debian package
echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Building Debian/Ubuntu package (.deb)${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
if "$SCRIPT_DIR/build-deb.sh"; then
    BUILD_SUCCESS+=("Debian/Ubuntu (.deb)")
else
    BUILD_FAILED+=("Debian/Ubuntu (.deb)")
    echo -e "${RED}✗ Debian package build failed${NC}"
fi

# Build RPM package
echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Building Fedora/RHEL package (.rpm)${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
if "$SCRIPT_DIR/build-rpm.sh"; then
    BUILD_SUCCESS+=("Fedora/RHEL (.rpm)")
else
    BUILD_FAILED+=("Fedora/RHEL (.rpm)")
    echo -e "${RED}✗ RPM package build failed${NC}"
fi

# Build Arch package
echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Building Arch Linux package (.pkg.tar.zst)${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
if "$SCRIPT_DIR/build-arch.sh"; then
    BUILD_SUCCESS+=("Arch Linux (.pkg.tar.zst)")
else
    BUILD_FAILED+=("Arch Linux (.pkg.tar.zst)")
    echo -e "${RED}✗ Arch package build failed${NC}"
fi

# Print summary
echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                    Build Summary                           ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [ ${#BUILD_SUCCESS[@]} -gt 0 ]; then
    echo -e "${GREEN}✓ Successfully built:${NC}"
    for pkg in "${BUILD_SUCCESS[@]}"; do
        echo -e "  ${GREEN}✓${NC} $pkg"
    done
fi

if [ ${#BUILD_FAILED[@]} -gt 0 ]; then
    echo ""
    echo -e "${RED}✗ Failed to build:${NC}"
    for pkg in "${BUILD_FAILED[@]}"; do
        echo -e "  ${RED}✗${NC} $pkg"
    done
fi

echo ""
echo -e "${BLUE}All packages available in:${NC} $SCRIPT_DIR/output/"
ls -lh "$SCRIPT_DIR/output/" 2>/dev/null || true

# Exit with error if any builds failed
if [ ${#BUILD_FAILED[@]} -gt 0 ]; then
    exit 1
fi

echo ""
echo -e "${GREEN}✓ All packages built successfully!${NC}"
