# Packaging Directory Organization

This document describes the organization of the packaging directory and its contents.

## Directory Structure

```
packaging/
├── debian/              # Debian/Ubuntu packaging files
│   ├── control          # Package metadata and dependencies
│   ├── rules            # Build rules
│   ├── changelog        # Version history
│   ├── copyright        # License information
│   └── *.install        # File installation lists
├── rpm/                 # Fedora/RHEL packaging files
│   └── lgx-runtime.spec # RPM spec file
├── arch/                # Arch Linux packaging files
│   ├── PKGBUILD         # Build script
│   └── .SRCINFO         # Package metadata
├── output/              # Built packages (gitignored)
├── build-deb.sh         # Build Debian packages
├── build-rpm.sh         # Build RPM packages
├── build-arch.sh        # Build Arch packages
├── build-all.sh         # Build all packages
├── install.sh           # Universal installer
├── uninstall.sh         # Universal uninstaller
├── README.md            # Main packaging documentation
├── QUICKSTART.md        # Quick start guide
├── PPA_SETUP.md         # Ubuntu PPA setup guide
├── COPR_SETUP.md        # Fedora Copr setup guide
├── AUR_SETUP.md         # Arch AUR setup guide
├── PUBLIC_REPOSITORIES.md # Public repository overview
└── ORGANIZATION.md      # This file
```

## File Categories

### Build Scripts
- `build-deb.sh` - Builds .deb packages for Ubuntu/Debian
- `build-rpm.sh` - Builds .rpm packages for Fedora/RHEL
- `build-arch.sh` - Builds .pkg.tar.zst packages for Arch Linux
- `build-all.sh` - Builds all package types

### Installation Scripts
- `install.sh` - Auto-detects distribution and installs appropriate package
- `uninstall.sh` - Auto-detects distribution and removes package

### Documentation
- `README.md` - Comprehensive packaging documentation
- `QUICKSTART.md` - Quick start guide for building and installing
- `PPA_SETUP.md` - Detailed guide for Ubuntu PPA setup
- `COPR_SETUP.md` - Detailed guide for Fedora Copr setup
- `AUR_SETUP.md` - Detailed guide for Arch AUR setup
- `PUBLIC_REPOSITORIES.md` - Overview of public repository distribution
- `ORGANIZATION.md` - This file

### Distribution-Specific Files
- `debian/` - Debian/Ubuntu packaging metadata
- `rpm/` - Fedora/RHEL packaging metadata
- `arch/` - Arch Linux packaging metadata

### Output Directory
- `output/` - Contains built packages (excluded from git)

## Gitignore Configuration

The following patterns are excluded from version control:

```
# Package artifacts
*.deb
*.rpm
*.pkg.tar.zst
*.tar.gz
*.zip
*.buildinfo
*.changes
*.ddeb

# Packaging output directory
/packaging/output/
```

## Workflow

### Building Packages

1. Build all packages:
   ```bash
   cd packaging
   ./build-all.sh
   ```

2. Build specific package:
   ```bash
   ./build-deb.sh    # Ubuntu/Debian
   ./build-rpm.sh    # Fedora/RHEL
   ./build-arch.sh   # Arch Linux
   ```

3. Packages are placed in `output/` directory

### Installing Locally

```bash
./install.sh
```

The installer auto-detects your distribution and installs the appropriate package.

### Publishing to Repositories

1. Ubuntu PPA - See `PPA_SETUP.md`
2. Fedora Copr - See `COPR_SETUP.md`
3. Arch AUR - See `AUR_SETUP.md`

## Maintenance

### Adding New Distribution Support

1. Create distribution-specific directory (e.g., `opensuse/`)
2. Add packaging metadata files
3. Create build script (e.g., `build-opensuse.sh`)
4. Update `build-all.sh` to include new distribution
5. Update `install.sh` and `uninstall.sh` with detection logic
6. Create setup guide (e.g., `OPENSUSE_SETUP.md`)
7. Update `README.md` and `PUBLIC_REPOSITORIES.md`

### Updating Version

When releasing a new version:

1. Update version in source files:
   - `CMakeLists.txt`
   - `include/lgx_version.h`
   - `CHANGELOG.md`

2. Update packaging files:
   - `debian/changelog`
   - `rpm/lgx-runtime.spec`
   - `arch/PKGBUILD`

3. Build and test packages locally

4. Publish to public repositories

### Cleaning Build Artifacts

```bash
# Clean output directory
rm -rf output/

# Clean distribution-specific build artifacts
cd debian && debclean
cd ../rpm && rm -rf BUILD/ BUILDROOT/ RPMS/ SRPMS/
cd ../arch && rm -rf src/ pkg/ *.pkg.tar.zst
```

## Best Practices

1. Always test builds locally before publishing
2. Keep packaging files in sync with upstream releases
3. Document all changes in distribution-specific changelogs
4. Respond to user feedback promptly
5. Monitor build failures and fix quickly
6. Keep dependencies minimal and well-documented
7. Follow distribution-specific packaging guidelines

## Support

For packaging issues:
- GitHub Issues: https://github.com/dream1290/LGX/issues
- Distribution-specific support channels (see setup guides)

For general LGX Runtime questions:
- Documentation: docs/
- GitHub Discussions: https://github.com/dream1290/LGX/discussions
