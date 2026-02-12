# Deployment Guide - LGX Runtime Core

This document describes the professional deployment process for LGX Runtime Core releases.

## Table of Contents

- [Pre-Deployment Checklist](#pre-deployment-checklist)
- [Version Update Process](#version-update-process)
- [Repository Cleanup](#repository-cleanup)
- [Building Release Artifacts](#building-release-artifacts)
- [Git Tagging and Pushing](#git-tagging-and-pushing)
- [GitHub Release](#github-release)
- [Package Distribution](#package-distribution)
- [Post-Deployment](#post-deployment)

## Pre-Deployment Checklist

Before starting the deployment process, ensure:

- [ ] All tests pass (`ctest`)
- [ ] No compiler warnings
- [ ] Static analysis passes (`-fanalyzer`)
- [ ] Memory leak check passes (Valgrind)
- [ ] AddressSanitizer clean
- [ ] Documentation is up-to-date
- [ ] CHANGELOG.md is updated
- [ ] All PRs for this release are merged

## Version Update Process

### 1. Determine Version Number

Follow [Semantic Versioning](https://semver.org/):

- **MAJOR.MINOR.PATCH** (e.g., 1.0.1)
- **MAJOR:** Breaking API changes
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes (backward compatible)

### 2. Update Version Files

Update version in the following files:

#### CMakeLists.txt
```cmake
project(lgx_runtime VERSION 1.0.1 LANGUAGES C)
```

#### include/lgx_version.h
```c
#define LGX_VERSION_MAJOR 1
#define LGX_VERSION_MINOR 0
#define LGX_VERSION_PATCH 1
#define LGX_VERSION_STRING "1.0.1"
```

#### packaging/debian/changelog
Add new entry at the top:
```
lgx-runtime (1.0.1-1) unstable; urgency=medium

  * Bug fixes and improvements
  * [List changes here]

 -- LGX Platform Team <team@lgx-platform.org>  [Date]
```

#### packaging/rpm/lgx-runtime.spec
```spec
Version:        1.0.1
Release:        1%{?dist}
```

Add changelog entry at the bottom.

#### packaging/arch/PKGBUILD
```bash
pkgver=1.0.1
pkgrel=1
```

#### packaging/arch/.SRCINFO
```
pkgver = 1.0.1
```

### 3. Update CHANGELOG.md

Add release notes following Keep a Changelog format:

```markdown
## [1.0.1] - 2026-02-12

### Fixed
- Bug fix descriptions

### Added
- New feature descriptions

### Changed
- Change descriptions
```

## Repository Cleanup

### 1. Verify .gitignore

Ensure `.gitignore` excludes:

```gitignore
# Build artifacts
build/
build-*/
*.o
*.a
*.so

# IDE configuration
.vscode/
.idea/
.kiro/

# Temporary files
*.tmp
*.log
*_SUMMARY.md
*_STATUS.md

# Test results
test_results.txt

# Security audits
security_audit_*/

# Package outputs
packaging/output/
*.tar.gz
```

### 2. Check for Unwanted Files

```bash
# Check what's tracked
git ls-files | grep -E "^\.|_SUMMARY|_STATUS|_FIXES"

# Remove unwanted files
git rm --cached [file]
```

### 3. Verify Repository State

```bash
# Check status
git status

# Verify no large files
git ls-files | xargs ls -lh | sort -k5 -h | tail -20

# Check repository size
du -sh .git
```

## Building Release Artifacts

### 1. Clean Build

```bash
# Remove old build directories
rm -rf build build-release

# Create fresh release build
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# Run tests
cd build-release && ctest --output-on-failure
```

### 2. Create Source Tarball

```bash
# Create output directory
mkdir -p packaging/output

# Create source tarball (exclude build artifacts)
tar --exclude='.git' \
    --exclude='build*' \
    --exclude='security_audit_*' \
    --exclude='packaging/output' \
    --exclude='*.o' \
    --exclude='*.a' \
    --exclude='.kiro' \
    -czf packaging/output/lgx-runtime-1.0.1.tar.gz \
    .
```

### 3. Build Distribution Packages (Optional)

```bash
# Debian/Ubuntu
cd packaging && ./build-deb.sh

# Fedora/RHEL
cd packaging && ./build-rpm.sh

# Arch Linux
cd packaging && ./build-arch.sh

# All packages
cd packaging && ./build-all.sh
```

## Git Tagging and Pushing

### 1. Commit Version Changes

```bash
# Stage version files
git add CMakeLists.txt \
        include/lgx_version.h \
        packaging/debian/changelog \
        packaging/rpm/lgx-runtime.spec \
        packaging/arch/PKGBUILD \
        packaging/arch/.SRCINFO \
        CHANGELOG.md \
        .gitignore

# Commit
git commit -m "chore: bump version to 1.0.1

- Updated version in all packaging files
- Updated CHANGELOG.md with release notes
- Cleaned up repository (removed development artifacts)"
```

### 2. Create Git Tag

```bash
# Create annotated tag
git tag -a v1.0.1 -m "Release v1.0.1

Bug fixes and improvements:
- [List key changes]

See CHANGELOG.md for full details."

# Verify tag
git tag -l -n9 v1.0.1
```

### 3. Push to GitHub

```bash
# Push commits
git push origin main

# Push tag
git push origin v1.0.1
```

### 4. Handle Push Failures

If push fails due to large files:

```bash
# Remove large files from history
git filter-branch --force --index-filter \
  'git rm --cached --ignore-unmatch [large-file]' \
  --prune-empty --tag-name-filter cat -- --all

# Force push (CAUTION: rewrites history)
git push --force origin main
git push --force origin v1.0.1
```

## GitHub Release

### 1. Create Release on GitHub

1. Go to: https://github.com/dream1290/LGX/releases/new
2. Select tag: `v1.0.1`
3. Release title: `LGX Runtime Core v1.0.1`
4. Description: Copy from CHANGELOG.md or create release notes

### 2. Upload Release Assets

Upload the following files:

- `packaging/output/lgx-runtime-1.0.1.tar.gz` - Source tarball
- `packaging/output/*.deb` - Debian packages (if built)
- `packaging/output/*.rpm` - RPM packages (if built)
- `packaging/output/*.pkg.tar.zst` - Arch packages (if built)

### 3. Publish Release

- Check "Set as the latest release" if appropriate
- Click "Publish release"

## Package Distribution

### Debian/Ubuntu (APT Repository)

```bash
# Sign packages
debsign lgx-runtime_1.0.1-1_amd64.deb

# Upload to repository
dput ppa:lgx-platform/stable lgx-runtime_1.0.1-1_amd64.changes
```

### Fedora/RHEL (Copr)

```bash
# Upload to Copr
copr-cli build lgx-runtime packaging/output/lgx-runtime-1.0.1-1.src.rpm
```

### Arch Linux (AUR)

```bash
# Update AUR repository
cd aur-lgx-runtime
git pull
# Update PKGBUILD and .SRCINFO
makepkg --printsrcinfo > .SRCINFO
git add PKGBUILD .SRCINFO
git commit -m "Update to 1.0.1"
git push
```

## Post-Deployment

### 1. Verify Release

```bash
# Clone fresh copy
git clone https://github.com/dream1290/LGX.git lgx-test
cd lgx-test

# Checkout tag
git checkout v1.0.1

# Build and test
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && ctest
```

### 2. Update Documentation

- Update website with new release
- Update installation instructions
- Announce release on social media/forums

### 3. Monitor Issues

- Watch for bug reports
- Respond to user feedback
- Plan next release if needed

## Rollback Procedure

If critical issues are found:

### 1. Delete Tag

```bash
# Delete local tag
git tag -d v1.0.1

# Delete remote tag
git push --delete origin v1.0.1
```

### 2. Delete GitHub Release

- Go to GitHub Releases
- Delete the release

### 3. Fix Issues

- Fix the critical bugs
- Increment patch version (e.g., 1.0.2)
- Repeat deployment process

## Best Practices

### DO:
- ✅ Test thoroughly before release
- ✅ Update all version files consistently
- ✅ Write clear release notes
- ✅ Use semantic versioning
- ✅ Tag releases properly
- ✅ Keep repository clean

### DON'T:
- ❌ Commit build artifacts
- ❌ Commit IDE configuration
- ❌ Commit temporary files
- ❌ Push without testing
- ❌ Rewrite published history (unless critical)
- ❌ Skip version updates in any file

## Troubleshooting

### Large Files Rejected

```bash
# Find large files
git ls-files | xargs ls -lh | sort -k5 -h | tail -20

# Remove from history
git filter-branch --force --index-filter \
  'git rm --cached --ignore-unmatch [file]' \
  --prune-empty --tag-name-filter cat -- --all

# Force push
git push --force
```

### Version Mismatch

Ensure version is updated in ALL files:
- CMakeLists.txt
- include/lgx_version.h
- All packaging files
- CHANGELOG.md

### Build Failures

```bash
# Clean everything
rm -rf build build-release

# Fresh build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Support

For deployment issues:
- Email: team@lgx-platform.org
- GitHub Issues: https://github.com/dream1290/LGX/issues

---

**Remember:** A professional deployment process ensures quality and reliability for all users.
