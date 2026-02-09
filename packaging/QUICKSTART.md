# LGX Runtime Core - Packaging Quick Start

## TL;DR

```bash
# Build all packages
cd packaging
./build-all.sh

# Install (auto-detects your distribution)
./install.sh

# Verify
pkg-config --modversion lgx_runtime

# Uninstall
./uninstall.sh
```

## Build Individual Packages

```bash
# Ubuntu/Debian
./build-deb.sh

# Fedora/RHEL
./build-rpm.sh

# Arch Linux
./build-arch.sh
```

## Manual Installation

### Ubuntu/Debian
```bash
sudo dpkg -i output/lgx-runtime_*.deb
sudo dpkg -i output/lgx-runtime-dev_*.deb
```

### Fedora/RHEL
```bash
sudo dnf install output/lgx-runtime-*.rpm
sudo dnf install output/lgx-runtime-devel-*.rpm
```

### Arch Linux
```bash
sudo pacman -U output/lgx-runtime-*.pkg.tar.zst
```

## Test Installation

```bash
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

Expected: `LGX Runtime v1.0.0`

## Troubleshooting

### Missing Build Tools

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential debhelper cmake pkg-config
```

**Fedora/RHEL:**
```bash
sudo dnf install rpm-build rpmdevtools cmake gcc
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake
```

### Missing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install libvulkan-dev libjemalloc-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install vulkan-headers vulkan-loader-devel jemalloc-devel
```

**Arch Linux:**
```bash
sudo pacman -S vulkan-headers vulkan-icd-loader jemalloc
```

## More Information

See [README.md](README.md) for complete documentation.
