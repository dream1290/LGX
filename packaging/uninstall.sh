#!/bin/bash
# Universal uninstallation script for LGX Runtime Core

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║       LGX Runtime Core - Uninstallation Script            ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Detect distribution
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_NAME=$NAME
    elif [ -f /etc/lsb-release ]; then
        . /etc/lsb-release
        DISTRO=$DISTRIB_ID
        DISTRO_NAME=$DISTRIB_DESCRIPTION
    else
        echo -e "${RED}Error: Cannot detect distribution${NC}"
        exit 1
    fi
}

# Check if package is installed
check_installed() {
    case $DISTRO in
        ubuntu|debian)
            if ! dpkg -l | grep -q lgx-runtime; then
                echo -e "${YELLOW}LGX Runtime Core is not installed${NC}"
                exit 0
            fi
            ;;
        fedora|rhel|centos|rocky|almalinux)
            if ! rpm -qa | grep -q lgx-runtime; then
                echo -e "${YELLOW}LGX Runtime Core is not installed${NC}"
                exit 0
            fi
            ;;
        arch|manjaro)
            if ! pacman -Q lgx-runtime &>/dev/null; then
                echo -e "${YELLOW}LGX Runtime Core is not installed${NC}"
                exit 0
            fi
            ;;
        *)
            echo -e "${YELLOW}Warning: Unknown distribution${NC}"
            ;;
    esac
}

# Uninstall package
uninstall_package() {
    echo -e "${YELLOW}Uninstalling LGX Runtime Core...${NC}"
    
    case $DISTRO in
        ubuntu|debian)
            sudo apt-get remove -y lgx-runtime lgx-runtime-dev
            sudo apt-get autoremove -y
            ;;
        fedora|rhel|centos|rocky|almalinux)
            sudo dnf remove -y lgx-runtime lgx-runtime-devel
            ;;
        arch|manjaro)
            sudo pacman -R --noconfirm lgx-runtime
            ;;
        *)
            echo -e "${RED}Error: Unsupported distribution: $DISTRO${NC}"
            exit 1
            ;;
    esac
}

# Verify uninstallation
verify_uninstallation() {
    echo ""
    echo -e "${YELLOW}Verifying uninstallation...${NC}"
    
    if pkg-config --exists lgx_runtime 2>/dev/null; then
        echo -e "${RED}✗ Uninstallation verification failed${NC}"
        echo -e "${YELLOW}pkg-config still finds lgx_runtime${NC}"
        exit 1
    else
        echo -e "${GREEN}✓ LGX Runtime Core uninstalled successfully!${NC}"
    fi
}

# Clean up any remaining files (optional)
cleanup_files() {
    echo ""
    read -p "Remove configuration and cache files? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Cleaning up configuration files...${NC}"
        
        # Remove any user-specific config (if exists)
        rm -rf ~/.config/lgx-runtime 2>/dev/null || true
        rm -rf ~/.cache/lgx-runtime 2>/dev/null || true
        
        # Remove system-wide config (if exists)
        sudo rm -rf /etc/lgx-runtime 2>/dev/null || true
        
        echo -e "${GREEN}✓ Configuration files removed${NC}"
    fi
}

# Main uninstallation flow
main() {
    detect_distro
    
    echo -e "${BLUE}Detected distribution:${NC} $DISTRO_NAME"
    echo ""
    
    check_installed
    uninstall_package
    verify_uninstallation
    cleanup_files
    
    echo ""
    echo -e "${GREEN}Uninstallation complete!${NC}"
}

# Run main function
main
