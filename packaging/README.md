# LGX Runtime Core - Packaging

This directory contains packaging files and build scripts for distributing LGX Runtime Core across major Linux distributions.

## Supported Distributions

- **Ubuntu/Debian** (.deb packages)
  - Ubuntu 22.04 LTS (Jammy Jellyfish)
  - Ubuntu 24.04 LTS (Noble Numbat)
  - Debian 11 (Bullseye)
  - Debian 12 (Bookworm)

- **Fedora/RHEL** (.rpm packages)
  - Fedora 38, 39, 40
  - RHEL 8, 9
  - Rocky Linux 8, 9
  - AlmaLinux 8, 9

- **Arch Linux** (.pkg.tar.zst packages)
  - Arch Linux
  - Manjaro

## Quick Start

### Building Packages

```bash
# For Ubuntu/Debian
./build-deb.sh

# For Fedora/RHEL
./build-rpm.sh

# For Arch Linux
./build-arch.sh
```

Built packages will be placed in `packaging/output/`.

### Installing

```bash
# Universal installer (auto-detects distribution)
./install.sh

# Or install manually:
# Ubuntu/Debian
sudo dpkg -i output/lgx-runtime_*.deb
sudo dpkg -i output/lgx-runtime-dev_*.deb

# Fedora/RHEL
sudo dnf install output/lgx-runtime-*.rpm
sudo dnf install output/lgx-runtime-devel-*.rpm

# Arch Linux
sudo pacman -U output/lgx-runtime-*.pkg.tar.zst
```

### Uninstalling

```bash
# Universal uninstaller
./uninstall.sh

# Or uninstall manually:
# Ubuntu/Debian
sudo apt-get remove lgx-runtime lgx-runtime-dev

# Fedora/RHEL
sudo dnf remove lgx-runtime lgx-runtime-devel

# Arch Linux
sudo pacman -R lgx-runtime
```

## Directory Structure

```
packaging/
├── debian/              # Debian/Ubuntu packaging files
│   ├── control          # Package metadata and dependencies
│   ├── rules            # Build rules
│   ├── changelog        # Version history
│   ├── copyright        # License information
│   ├── compat           # Debhelper compatibility level
│   ├── *.install        # File installation lists
│   └── *.lintian-overrides
├── rpm/                 # Fedora/RHEL packaging files
│   └── lgx-runtime.spec # RPM spec file
├── arch/                # Arch Linux packaging files
│   ├── PKGBUILD         # Build script
│   └── .SRCINFO         # Package metadata
├── build-deb.sh         # Debian package build script
├── build-rpm.sh         # RPM package build script
├── build-arch.sh        # Arch package build script
├── install.sh           # Universal installation script
├── uninstall.sh         # Universal uninstallation script
├── output/              # Built packages (created during build)
└── README.md            # This file
```

## Package Contents

### Runtime Package (`lgx-runtime`)

Contains the shared library:
- `/usr/lib/liblgx_runtime.so.*` - Shared library

### Development Package (`lgx-runtime-dev` / `lgx-runtime-devel`)

Contains headers and development files:
- `/usr/include/lgx_runtime.h` - Main API header
- `/usr/include/lgx_types.h` - Type definitions
- `/usr/include/lgx_version.h` - Version macros
- `/usr/include/lgx_integration.h` - Integration contracts
- `/usr/include/lgx/lgx_runtime_internal.h` - Internal API
- `/usr/lib/liblgx_runtime.so` - Development symlink
- `/usr/lib/pkgconfig/lgx_runtime.pc` - pkg-config file

## Dependencies

### Build Dependencies

- cmake >= 3.16
- gcc or clang
- pkg-config
- libvulkan-dev (optional, for GPU features)
- libjemalloc-dev (optional, for fallback allocator)

### Runtime Dependencies

- glibc
- libvulkan1 (recommended, for GPU features)
- libjemalloc2 (recommended, for fallback allocator)

## Building from Source

If you prefer to build and install manually without creating packages:

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

# Build
cmake --build build

# Test (optional)
cd build && ctest --output-on-failure

# Install
sudo cmake --install build
```

## Package Verification

After installation, verify the package:

```bash
# Check version
pkg-config --modversion lgx_runtime

# Check compiler flags
pkg-config --cflags --libs lgx_runtime

# Check library location
pkg-config --variable=libdir lgx_runtime

# Test with a simple program
cat > test.c << 'EOF'
#include <lgx_runtime.h>
#include <stdio.h>

int main() {
    lgx_version_info_t version;
    version.struct_size = sizeof(version);
    lgx_runtime_get_version(&version);
    printf("LGX Runtime v%d.%d.%d\n", 
           version.major, version.minor, version.patch);
    return 0;
}
EOF

gcc test.c $(pkg-config --cflags --libs lgx_runtime) -o test
./test
```

## Publishing Packages

### Ubuntu PPA

```bash
# Build source package
cd packaging
debuild -S -sa

# Upload to PPA
dput ppa:lgx-platform/stable ../lgx-runtime_*.changes
```

### Fedora COPR

```bash
# Create COPR project at https://copr.fedorainfracloud.org/

# Build from spec file
copr-cli build lgx-runtime rpm/lgx-runtime.spec
```

### Arch User Repository (AUR)

```bash
# Clone AUR repository
git clone ssh://aur@aur.archlinux.org/lgx-runtime.git

# Copy packaging files
cp arch/PKGBUILD lgx-runtime/
cp arch/.SRCINFO lgx-runtime/

# Commit and push
cd lgx-runtime
git add PKGBUILD .SRCINFO
git commit -m "Update to version 1.0.0"
git push
```

## Troubleshooting

### Build Failures

**Missing dependencies:**
```bash
# Ubuntu/Debian
sudo apt-get install build-essential debhelper cmake pkg-config libvulkan-dev libjemalloc-dev

# Fedora/RHEL
sudo dnf install rpm-build rpmdevtools cmake gcc vulkan-headers vulkan-loader-devel jemalloc-devel

# Arch Linux
sudo pacman -S base-devel cmake vulkan-headers vulkan-icd-loader jemalloc
```

**CMake version too old:**
```bash
# Install newer CMake from Kitware repository
# See: https://apt.kitware.com/
```

### Installation Issues

**Dependency conflicts:**
```bash
# Ubuntu/Debian
sudo apt-get install -f

# Fedora/RHEL
sudo dnf install --allowerasing

# Arch Linux
sudo pacman -Syu
```

**Library not found:**
```bash
# Update library cache
sudo ldconfig

# Check library path
ldconfig -p | grep lgx_runtime
```

## Performance Validation

After installation, run performance tests:

```bash
# Clone repository
git clone https://github.com/lgx-platform/LGX.git
cd LGX

# Build tests
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run performance tests
cd build
ctest -R perf_ --verbose
```

Expected results:
- Frame arena allocation: P99 < 100ns
- Memory overhead: < 2 MB
- Initialization time: < 5 ms

## Support

- **Issues:** https://github.com/lgx-platform/LGX/issues
- **Documentation:** https://github.com/lgx-platform/LGX/tree/main/docs
- **Email:** team@lgx-platform.org

## License

MIT License - See LICENSE file for details.
