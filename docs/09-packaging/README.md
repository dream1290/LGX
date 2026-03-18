# Packaging and Distribution Documentation

This directory contains documentation for LGX Runtime Core packaging and distribution.

## Packaging Directory

The actual packaging files are located in `../../packaging/` at the project root.

### Key Files
- [Packaging README](../../packaging/README.md) - Comprehensive packaging guide
- [Quick Start](../../packaging/QUICKSTART.md) - Quick build and install guide
- [Organization](../../packaging/ORGANIZATION.md) - Directory structure and workflow
- [PPA Setup](../../packaging/PPA_SETUP.md) - Ubuntu PPA configuration
- [Copr Setup](../../packaging/COPR_SETUP.md) - Fedora Copr configuration
- [AUR Setup](../../packaging/AUR_SETUP.md) - Arch AUR configuration
- [Public Repositories](../../packaging/PUBLIC_REPOSITORIES.md) - Multi-platform distribution

## Documentation in This Directory

### [release-process.md](release-process.md)
Release workflow and automation procedures.

### [versioning-and-releases-complete.md](versioning-and-releases-complete.md)
Version management and release guidelines.

### [RELEASE_QUICKSTART.md](RELEASE_QUICKSTART.md)
Quick reference for creating releases.

### [packaging-complete.md](packaging-complete.md)
Detailed completion report for packaging implementation.

### [PACKAGING_SUMMARY.md](PACKAGING_SUMMARY.md)
Quick overview of packaging infrastructure.

## Quick Start

```bash
# Navigate to packaging directory
cd ../../packaging

# Build all packages
./build-all.sh

# Install (auto-detects distribution)
./install.sh

# Verify
pkg-config --modversion lgx_runtime

# Uninstall
./uninstall.sh
```

## Supported Distributions

- **Debian/Ubuntu**: .deb packages
  - Ubuntu 22.04 LTS, 24.04 LTS
  - Debian 11, 12

- **Fedora/RHEL**: .rpm packages
  - Fedora 38, 39, 40
  - RHEL 8, 9
  - Rocky Linux 8, 9
  - AlmaLinux 8, 9

- **Arch Linux**: .pkg.tar.zst packages
  - Arch Linux
  - Manjaro

## Package Contents

### Runtime Package
- Shared library: `liblgx_runtime.so.*`

### Development Package
- Headers: `lgx_runtime.h`, `lgx_types.h`, `lgx_version.h`, `lgx_integration.h`
- Internal headers: `lgx/lgx_runtime_internal.h`
- pkg-config file: `lgx_runtime.pc`

## Publishing to Public Repositories

### Ubuntu PPA
See [PPA_SETUP.md](../../packaging/PPA_SETUP.md) for detailed instructions on publishing to Ubuntu PPA.

### Fedora Copr
See [COPR_SETUP.md](../../packaging/COPR_SETUP.md) for detailed instructions on publishing to Fedora Copr.

### Arch AUR
See [AUR_SETUP.md](../../packaging/AUR_SETUP.md) for detailed instructions on publishing to Arch AUR.

## Status

Complete - All packaging tasks finished

- 13.1.1 - Debian/Ubuntu packages
- 13.1.2 - Fedora/RHEL packages
- 13.1.3 - Arch Linux packages
- 13.1.4 - Installation scripts
- 13.1.5 - Public repository setup guides
