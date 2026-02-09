#!/bin/bash
# Universal installation script for LGX Runtime Core
# Detects distribution and installs appropriate package

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         LGX Runtime Core - Installation Script            ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Detect distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_VERSION=$VERSION_ID
        DISTRO_NAME=$NAME
    elif [ -f /etc/lsb-release ]; then
        . /etc/lsb-release
        DISTRO=$DISTRIB_ID
        DISTRO_VERSION=$DISTRIB_RELEASE
        DISTRO_NAME=$DISTRIB_DESCRIPTION
    else
        echo -e "${RED}Error: Cannot detect distribution${NC}"
        exit 1
    fi
}

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}Note: This script requires root privileges for installation${NC}"
        echo -e "${YELLOW}You may be prompted for your password${NC}"
        echo ""
    fi
}

# Install dependencies
install_dependencies() {
    echo -e "${YELLOW}Installing dependencies...${NC}"
    
    case $DISTRO in
        ubuntu|debian)
            sudo apt-get update
            sudo apt-get install -y libvulkan1 libjemalloc2
            ;;
        fedora|rhel|centos|rocky|almalinux)
            sudo dnf install -y vulkan-loader jemalloc
            ;;
        arch|manjaro)
            sudo pacman -S --noconfirm vulkan-icd-loader jemalloc
            ;;
        *)
            echo -e "${YELLOW}Warning: Unknown distribution, skipping dependency installation${NC}"
            ;;
    esac
}

# Install from pre-built package
install_from_package() {
    echo -e "${YELLOW}Installing from pre-built package...${NC}"
    
    case $DISTRO in
        ubuntu|debian)
            if [ -f "$SCRIPT_DIR/output/lgx-runtime_"*.deb ]; then
                sudo dpkg -i "$SCRIPT_DIR/output/lgx-runtime_"*.deb
                sudo dpkg -i "$SCRIPT_DIR/output/lgx-runtime-dev_"*.deb 2>/dev/null || true
                sudo apt-get install -f -y  # Fix any dependency issues
            else
                echo -e "${RED}Error: .deb package not found in $SCRIPT_DIR/output/${NC}"
                echo -e "${YELLOW}Build packages first with: ./build-deb.sh${NC}"
                exit 1
            fi
            ;;
        fedora|rhel|centos|rocky|almalinux)
            if [ -f "$SCRIPT_DIR/output/lgx-runtime-"*.rpm ]; then
                sudo dnf install -y "$SCRIPT_DIR/output/lgx-runtime-"*.rpm
                sudo dnf install -y "$SCRIPT_DIR/output/lgx-runtime-devel-"*.rpm 2>/dev/null || true
            else
                echo -e "${RED}Error: .rpm package not found in $SCRIPT_DIR/output/${NC}"
                echo -e "${YELLOW}Build packages first with: ./build-rpm.sh${NC}"
                exit 1
            fi
            ;;
        arch|manjaro)
            if [ -f "$SCRIPT_DIR/output/lgx-runtime-"*.pkg.tar.zst ]; then
                sudo pacman -U --noconfirm "$SCRIPT_DIR/output/lgx-runtime-"*.pkg.tar.zst
            else
                echo -e "${RED}Error: .pkg.tar.zst package not found in $SCRIPT_DIR/output/${NC}"
                echo -e "${YELLOW}Build packages first with: ./build-arch.sh${NC}"
                exit 1
            fi
            ;;
        *)
            echo -e "${RED}Error: Unsupported distribution: $DISTRO${NC}"
            echo -e "${YELLOW}Supported distributions:${NC}"
            echo "  - Ubuntu/Debian"
            echo "  - Fedora/RHEL/Rocky/AlmaLinux"
            echo "  - Arch Linux/Manjaro"
            exit 1
            ;;
    esac
}

# Verify installation
verify_installation() {
    echo ""
    echo -e "${YELLOW}Verifying installation...${NC}"
    
    if pkg-config --exists lgx_runtime; then
        VERSION=$(pkg-config --modversion lgx_runtime)
        echo -e "${GREEN}✓ LGX Runtime Core ${VERSION} installed successfully!${NC}"
        echo ""
        echo -e "${BLUE}Installation details:${NC}"
        echo "  Version: $VERSION"
        echo "  Prefix: $(pkg-config --variable=prefix lgx_runtime)"
        echo "  Library: $(pkg-config --variable=libdir lgx_runtime)/liblgx_runtime.so"
        echo "  Headers: $(pkg-config --variable=includedir lgx_runtime)"
        echo ""
        echo -e "${BLUE}Compiler flags:${NC}"
        echo "  CFLAGS:  $(pkg-config --cflags lgx_runtime)"
        echo "  LDFLAGS: $(pkg-config --libs lgx_runtime)"
    else
        echo -e "${RED}✗ Installation verification failed${NC}"
        echo -e "${YELLOW}pkg-config cannot find lgx_runtime${NC}"
        exit 1
    fi
}

# Print usage information
print_usage() {
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                    Next Steps                              ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${GREEN}To use LGX Runtime in your project:${NC}"
    echo ""
    echo "1. Include the header in your C code:"
    echo "   #include <lgx_runtime.h>"
    echo ""
    echo "2. Compile with pkg-config:"
    echo "   gcc myapp.c \$(pkg-config --cflags --libs lgx_runtime) -o myapp"
    echo ""
    echo "3. Or use CMake:"
    echo "   find_package(PkgConfig REQUIRED)"
    echo "   pkg_check_modules(LGX REQUIRED lgx_runtime)"
    echo "   target_link_libraries(myapp \${LGX_LIBRARIES})"
    echo ""
    echo -e "${GREEN}Documentation:${NC}"
    echo "  - API Reference: docs/02-api-reference/README.md"
    echo "  - Integration Guide: docs/03-integration-guide/README.md"
    echo "  - Examples: docs/02-api-reference/examples/"
    echo ""
}

# Main installation flow
main() {
    detect_distro
    check_root
    
    echo -e "${BLUE}Detected distribution:${NC} $DISTRO_NAME"
    echo ""
    
    install_dependencies
    install_from_package
    verify_installation
    print_usage
    
    echo -e "${GREEN}Installation complete!${NC}"
}

# Run main function
main
