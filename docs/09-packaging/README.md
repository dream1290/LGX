# Packaging and Distribution Documentation

This directory contains documentation for LGX Runtime Core packaging and distribution.

## Documents

### [PACKAGING_SUMMARY.md](PACKAGING_SUMMARY.md)
Quick overview of the packaging infrastructure with key features and quick start guide.

### [packaging-complete.md](packaging-complete.md)
Detailed completion report for Task 13.1 with full implementation details, testing procedures, and verification steps.

## Quick Links

### Packaging Files
- **Main Directory**: `../../packaging/`
- **README**: `../../packaging/README.md`
- **Quick Start**: `../../packaging/QUICKSTART.md`

### Build Scripts
- Build all: `../../packaging/build-all.sh`
- Debian/Ubuntu: `../../packaging/build-deb.sh`
- Fedora/RHEL: `../../packaging/build-rpm.sh`
- Arch Linux: `../../packaging/build-arch.sh`

### Installation
- Install: `../../packaging/install.sh`
- Uninstall: `../../packaging/uninstall.sh`

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

## Status

✅ **Complete** - All packaging tasks finished

- ✅ 13.1.1 - Debian/Ubuntu packages
- ✅ 13.1.2 - Fedora/RHEL packages
- ✅ 13.1.3 - Arch Linux packages
- ✅ 13.1.4 - Installation scripts

## Next Steps

- Task 13.2 - Set up versioning and releases
- Task 14 - Production hardening
- Task 11 - Documentation
