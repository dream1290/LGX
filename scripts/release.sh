#!/bin/bash
# Release automation script for LGX Runtime Core
# Creates a complete release with packages, checksums, and GitHub release

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
RELEASE_DIR="$PROJECT_ROOT/release"
GITHUB_REPO="lgx-platform/LGX"

# Get version
get_version() {
    "$SCRIPT_DIR/version.sh" show
}

# Check prerequisites
check_prerequisites() {
    echo -e "${YELLOW}Checking prerequisites...${NC}"
    
    local missing=()
    
    # Check for required tools
    for tool in git cmake tar sha256sum gpg; do
        if ! command -v $tool &> /dev/null; then
            missing+=("$tool")
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo -e "${RED}Error: Missing required tools: ${missing[*]}${NC}"
        echo "Install with:"
        echo "  Ubuntu/Debian: sudo apt-get install git cmake tar coreutils gnupg"
        echo "  Fedora/RHEL: sudo dnf install git cmake tar coreutils gnupg2"
        echo "  Arch: sudo pacman -S git cmake tar coreutils gnupg"
        exit 1
    fi
    
    # Check if we're in a git repository
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        echo -e "${RED}Error: Not in a git repository${NC}"
        exit 1
    fi
    
    # Check for uncommitted changes
    if ! git diff-index --quiet HEAD --; then
        echo -e "${RED}Error: Uncommitted changes detected${NC}"
        echo "Commit or stash changes before creating a release"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Prerequisites OK${NC}"
}

# Create source tarball
create_source_tarball() {
    local version=$1
    local tarball_name="lgx-runtime-${version}.tar.gz"
    
    echo -e "${YELLOW}Creating source tarball...${NC}"
    
    cd "$PROJECT_ROOT/.."
    tar --exclude='.git' \
        --exclude='build*' \
        --exclude='release' \
        --exclude='packaging/output' \
        --exclude='packaging/arch-build' \
        --exclude='*.deb' \
        --exclude='*.rpm' \
        --exclude='*.pkg.tar.zst' \
        --transform "s,^$(basename "$PROJECT_ROOT"),lgx-runtime-${version}," \
        -czf "$RELEASE_DIR/$tarball_name" \
        "$(basename "$PROJECT_ROOT")"
    
    echo -e "${GREEN}✓ Created $tarball_name${NC}"
}

# Build all packages
build_packages() {
    echo -e "${YELLOW}Building distribution packages...${NC}"
    
    cd "$PROJECT_ROOT/packaging"
    
    # Clean output directory
    rm -rf output
    mkdir -p output
    
    # Build all packages
    if ! ./build-all.sh; then
        echo -e "${RED}Error: Package build failed${NC}"
        exit 1
    fi
    
    # Copy packages to release directory
    cp output/* "$RELEASE_DIR/" 2>/dev/null || true
    
    echo -e "${GREEN}✓ Packages built${NC}"
}

# Generate checksums
generate_checksums() {
    echo -e "${YELLOW}Generating checksums...${NC}"
    
    cd "$RELEASE_DIR"
    
    # Generate SHA256 checksums
    sha256sum *.tar.gz *.deb *.rpm *.pkg.tar.zst 2>/dev/null > SHA256SUMS || true
    
    echo -e "${GREEN}✓ Checksums generated${NC}"
}

# Sign release (optional)
sign_release() {
    echo -e "${YELLOW}Signing release...${NC}"
    
    cd "$RELEASE_DIR"
    
    # Check if GPG key is available
    if ! gpg --list-secret-keys > /dev/null 2>&1; then
        echo -e "${YELLOW}Warning: No GPG key found, skipping signing${NC}"
        return
    fi
    
    # Sign checksums file
    if [ -f SHA256SUMS ]; then
        gpg --detach-sign --armor SHA256SUMS
        echo -e "${GREEN}✓ Release signed${NC}"
    fi
}

# Create release notes
create_release_notes() {
    local version=$1
    local notes_file="$RELEASE_DIR/RELEASE_NOTES.md"
    
    echo -e "${YELLOW}Creating release notes...${NC}"
    
    # Extract changelog for this version
    local changelog=""
    if [ -f "$PROJECT_ROOT/CHANGELOG.md" ]; then
        # Extract section for this version
        changelog=$(sed -n "/## \[${version}\]/,/## \[/p" "$PROJECT_ROOT/CHANGELOG.md" | sed '$d')
    fi
    
    cat > "$notes_file" << EOF
# LGX Runtime Core ${version}

Release date: $(date +%Y-%m-%d)

## Changes

${changelog:-No changelog available. See git log for details.}

## Downloads

### Source Code
- \`lgx-runtime-${version}.tar.gz\` - Source tarball

### Binary Packages

#### Debian/Ubuntu
- \`lgx-runtime_${version}-1_amd64.deb\` - Runtime library
- \`lgx-runtime-dev_${version}-1_amd64.deb\` - Development files

#### Fedora/RHEL
- \`lgx-runtime-${version}-1.x86_64.rpm\` - Runtime library
- \`lgx-runtime-devel-${version}-1.x86_64.rpm\` - Development files

#### Arch Linux
- \`lgx-runtime-${version}-1-x86_64.pkg.tar.zst\` - Combined package

## Installation

### Ubuntu/Debian
\`\`\`bash
wget https://github.com/${GITHUB_REPO}/releases/download/v${version}/lgx-runtime_${version}-1_amd64.deb
wget https://github.com/${GITHUB_REPO}/releases/download/v${version}/lgx-runtime-dev_${version}-1_amd64.deb
sudo dpkg -i lgx-runtime_${version}-1_amd64.deb lgx-runtime-dev_${version}-1_amd64.deb
\`\`\`

### Fedora/RHEL
\`\`\`bash
wget https://github.com/${GITHUB_REPO}/releases/download/v${version}/lgx-runtime-${version}-1.x86_64.rpm
wget https://github.com/${GITHUB_REPO}/releases/download/v${version}/lgx-runtime-devel-${version}-1.x86_64.rpm
sudo dnf install lgx-runtime-${version}-1.x86_64.rpm lgx-runtime-devel-${version}-1.x86_64.rpm
\`\`\`

### Arch Linux
\`\`\`bash
wget https://github.com/${GITHUB_REPO}/releases/download/v${version}/lgx-runtime-${version}-1-x86_64.pkg.tar.zst
sudo pacman -U lgx-runtime-${version}-1-x86_64.pkg.tar.zst
\`\`\`

## Verification

Verify checksums:
\`\`\`bash
wget https://github.com/${GITHUB_REPO}/releases/download/v${version}/SHA256SUMS
sha256sum -c SHA256SUMS
\`\`\`

Verify GPG signature (if available):
\`\`\`bash
wget https://github.com/${GITHUB_REPO}/releases/download/v${version}/SHA256SUMS.asc
gpg --verify SHA256SUMS.asc SHA256SUMS
\`\`\`

## Documentation

- [API Reference](https://github.com/${GITHUB_REPO}/tree/v${version}/docs/02-api-reference)
- [Integration Guide](https://github.com/${GITHUB_REPO}/tree/v${version}/docs/03-integration-guide)
- [Architecture](https://github.com/${GITHUB_REPO}/tree/v${version}/docs/04-architecture)

## Support

- Issues: https://github.com/${GITHUB_REPO}/issues
- Email: team@lgx-platform.org
EOF
    
    echo -e "${GREEN}✓ Release notes created${NC}"
}

# Create git tag
create_git_tag() {
    local version=$1
    local tag="v${version}"
    
    echo -e "${YELLOW}Creating git tag...${NC}"
    
    # Check if tag already exists
    if git rev-parse "$tag" >/dev/null 2>&1; then
        echo -e "${YELLOW}Warning: Tag $tag already exists${NC}"
        read -p "Delete and recreate? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git tag -d "$tag"
        else
            echo "Skipping tag creation"
            return
        fi
    fi
    
    # Create annotated tag
    git tag -a "$tag" -m "Release ${version}"
    
    echo -e "${GREEN}✓ Tag $tag created${NC}"
}

# Print release summary
print_summary() {
    local version=$1
    
    echo ""
    echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║              Release ${version} Complete                      ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${GREEN}Release artifacts:${NC}"
    ls -lh "$RELEASE_DIR"
    echo ""
    echo -e "${YELLOW}Next steps:${NC}"
    echo "  1. Review release artifacts in: $RELEASE_DIR"
    echo "  2. Push tag: git push origin v${version}"
    echo "  3. Create GitHub release:"
    echo "     - Go to: https://github.com/${GITHUB_REPO}/releases/new"
    echo "     - Tag: v${version}"
    echo "     - Title: LGX Runtime Core ${version}"
    echo "     - Description: Copy from $RELEASE_DIR/RELEASE_NOTES.md"
    echo "     - Upload files from: $RELEASE_DIR"
    echo "  4. Announce release on:"
    echo "     - Project website"
    echo "     - Mailing list"
    echo "     - Social media"
    echo ""
}

# Main release workflow
main() {
    local version=${1:-$(get_version)}
    
    echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║         LGX Runtime Core - Release Automation             ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${YELLOW}Version:${NC} $version"
    echo ""
    
    # Confirm
    read -p "Create release for version $version? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 0
    fi
    
    # Create release directory
    rm -rf "$RELEASE_DIR"
    mkdir -p "$RELEASE_DIR"
    
    # Run release steps
    check_prerequisites
    create_source_tarball "$version"
    build_packages
    generate_checksums
    sign_release
    create_release_notes "$version"
    create_git_tag "$version"
    print_summary "$version"
    
    echo -e "${GREEN}✓ Release complete!${NC}"
}

# Show usage
usage() {
    cat << EOF
Usage: $0 [version]

Create a complete release for LGX Runtime Core.

Arguments:
  version    Version to release (default: current version from CMakeLists.txt)

Examples:
  $0           # Release current version
  $0 1.2.3     # Release specific version

This script will:
  1. Check prerequisites (git, cmake, tar, etc.)
  2. Create source tarball
  3. Build all distribution packages (.deb, .rpm, .pkg.tar.zst)
  4. Generate SHA256 checksums
  5. Sign release with GPG (if available)
  6. Create release notes
  7. Create git tag

After running this script:
  - Push the tag: git push origin v<version>
  - Create GitHub release and upload artifacts
  - Announce the release

EOF
}

# Handle arguments
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    usage
    exit 0
fi

main "$@"
