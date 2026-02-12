# LGX Runtime Core - Release Process Documentation

## Overview

This document describes the complete release process for LGX Runtime Core, including versioning, changelog management, package building, and distribution.

## Semantic Versioning

LGX Runtime Core follows [Semantic Versioning 2.0.0](https://semver.org/):

**Format**: `MAJOR.MINOR.PATCH`

- **MAJOR**: Incompatible API changes
- **MINOR**: Backwards-compatible functionality additions
- **PATCH**: Backwards-compatible bug fixes

### Version Management

Use the `version.sh` script to manage versions:

```bash
# Show current version
./scripts/version.sh show

# Set specific version
./scripts/version.sh set 1.2.3 "Release message"

# Bump version
./scripts/version.sh bump major "Breaking API changes"
./scripts/version.sh bump minor "New features"
./scripts/version.sh bump patch "Bug fixes"
```

The script automatically updates:
- `CMakeLists.txt`
- `include/lgx_version.h`
- `packaging/debian/changelog`
- `packaging/rpm/lgx-runtime.spec`
- `packaging/arch/PKGBUILD`
- `packaging/arch/.SRCINFO`

## Changelog Management

### Conventional Commits

Follow [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `perf`: Performance improvement
- `docs`: Documentation changes
- `style`: Code style changes
- `refactor`: Code refactoring
- `test`: Test changes
- `build`: Build system changes
- `ci`: CI/CD changes
- `chore`: Maintenance tasks

**Breaking Changes**:
Add `BREAKING CHANGE:` in commit body or use `!` after type/scope.

### Changelog Generation

Use the `changelog.sh` script:

```bash
# Generate changelog for current version
./scripts/changelog.sh generate

# Update CHANGELOG.md
./scripts/changelog.sh update

# Regenerate full changelog from git history
./scripts/changelog.sh full

# Show changelog for specific version
./scripts/changelog.sh show 1.2.3
```

## Release Process

### 1. Pre-Release Preparation

**a. Complete all tasks for the release**
- Ensure all planned features are implemented
- All tests passing
- Documentation updated

**b. Run pre-release checklist**
```bash
# See RELEASE_CHECKLIST.md for complete checklist
```

**c. Update version**
```bash
./scripts/version.sh bump minor "Release 1.2.0 with new features"
```

**d. Update changelog**
```bash
./scripts/changelog.sh update
```

**e. Commit changes**
```bash
git add -A
git commit -m "chore: bump version to $(./scripts/version.sh show)"
```

### 2. Create Release

**Run the release script**:
```bash
./scripts/release.sh
```

This script will:
1. Check prerequisites (git, cmake, tar, etc.)
2. Create source tarball
3. Build all distribution packages (.deb, .rpm, .pkg.tar.zst)
4. Generate SHA256 checksums
5. Sign release with GPG (if available)
6. Create release notes
7. Create git tag

**Release artifacts** will be in `release/` directory:
- `lgx-runtime-1.2.0.tar.gz` - Source tarball
- `lgx-runtime_1.2.0-1_amd64.deb` - Debian runtime package
- `lgx-runtime-dev_1.2.0-1_amd64.deb` - Debian dev package
- `lgx-runtime-1.2.0-1.x86_64.rpm` - RPM runtime package
- `lgx-runtime-devel-1.2.0-1.x86_64.rpm` - RPM dev package
- `lgx-runtime-1.2.0-1-x86_64.pkg.tar.zst` - Arch package
- `SHA256SUMS` - Checksums file
- `SHA256SUMS.asc` - GPG signature (if available)
- `RELEASE_NOTES.md` - Release notes

### 3. Publish Release

**a. Push git tag**:
```bash
git push origin v1.2.0
```

**b. Create GitHub release**:
1. Go to https://github.com/lgx-platform/LGX/releases/new
2. Select tag: `v1.2.0`
3. Title: `LGX Runtime Core 1.2.0`
4. Description: Copy from `release/RELEASE_NOTES.md`
5. Upload all files from `release/` directory
6. Publish release

**c. Publish to distribution repositories**:

**Ubuntu PPA**:
```bash
cd packaging
debuild -S -sa
dput ppa:lgx-platform/stable ../lgx-runtime_*.changes
```

**Fedora COPR**:
```bash
copr-cli build lgx-runtime packaging/rpm/lgx-runtime.spec
```

**Arch User Repository (AUR)**:
```bash
cd /tmp
git clone ssh://aur@aur.archlinux.org/lgx-runtime.git
cp packaging/arch/PKGBUILD lgx-runtime/
cp packaging/arch/.SRCINFO lgx-runtime/
cd lgx-runtime
git add PKGBUILD .SRCINFO
git commit -m "Update to 1.2.0"
git push
```

### 4. Post-Release

**a. Announce release**:
- Project website
- Mailing list
- Social media (Twitter, Reddit, etc.)
- Gaming forums
- Linux distribution forums

**b. Monitor for issues**:
- GitHub issues
- Distribution bug trackers
- User feedback

**c. Update documentation website**

**d. Create milestone for next release**

## Hotfix Process

For critical bugs found after release:

### 1. Create Hotfix Branch
```bash
git checkout -b hotfix/v1.2.1 v1.2.0
```

### 2. Fix the Bug
Make necessary changes and commit:
```bash
git commit -m "fix: critical bug in allocation path"
```

### 3. Update Version
```bash
./scripts/version.sh bump patch "Hotfix: critical bug fix"
```

### 4. Run Critical Tests
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && ctest -R "unit_|integration_"
```

### 5. Create Hotfix Release
```bash
./scripts/release.sh
```

### 6. Merge Back to Main
```bash
git checkout main
git merge hotfix/v1.2.1
git push
git push --tags
```

### 7. Publish Hotfix
Follow the same publishing process as regular releases.

## Release Schedule

### Regular Releases

- **Major releases**: As needed for breaking changes
- **Minor releases**: Every 2-3 months
- **Patch releases**: As needed for bug fixes

### Release Candidates

For major and minor releases:

1. **RC1**: 2 weeks before release
2. **RC2**: 1 week before release (if needed)
3. **Final**: After RC testing complete

Tag format: `v1.2.0-rc1`, `v1.2.0-rc2`, `v1.2.0`

## Version Compatibility

### API Compatibility

- **Major version**: Breaking changes allowed
- **Minor version**: Backwards-compatible additions only
- **Patch version**: No API changes

### ABI Compatibility

- **Major version**: ABI breaks allowed
- **Minor version**: ABI compatible (symbol versioning)
- **Patch version**: ABI compatible

### Testing Compatibility

Run ABI compatibility tests:
```bash
./scripts/lgx-abi-test-matrix.sh
```

## Distribution-Specific Notes

### Ubuntu/Debian

- Packages must be lintian-clean
- Follow Debian Policy Manual
- Test on both Ubuntu LTS and Debian stable

### Fedora/RHEL

- Packages must be rpmlint-clean
- Follow Fedora Packaging Guidelines
- Test on both Fedora and RHEL

### Arch Linux

- Packages must be namcap-clean
- Follow Arch Packaging Standards
- Test on Arch Linux and Manjaro

## Rollback Process

If a release needs to be rolled back:

### 1. Delete GitHub Release
Remove the release from GitHub releases page.

### 2. Delete Git Tag
```bash
git tag -d v1.2.0
git push origin :refs/tags/v1.2.0
```

### 3. Remove Packages
Remove packages from distribution repositories:
- Ubuntu PPA: Delete package from PPA
- Fedora COPR: Delete build from COPR
- AUR: Revert commit in AUR repository

### 4. Announce Rollback
Notify users through all channels.

### 5. Fix and Re-Release
- Fix the issues
- Create new release with incremented version
- Follow normal release process

## Tools and Scripts

### Version Management
- `scripts/version.sh` - Version management
- `include/lgx_version.h` - Version header (auto-generated)

### Changelog
- `scripts/changelog.sh` - Changelog generation
- `CHANGELOG.md` - Changelog file

### Release
- `scripts/release.sh` - Release automation
- `RELEASE_CHECKLIST.md` - Release checklist

### Packaging
- `packaging/build-all.sh` - Build all packages
- `packaging/build-deb.sh` - Build Debian packages
- `packaging/build-rpm.sh` - Build RPM packages
- `packaging/build-arch.sh` - Build Arch packages

## Best Practices

### Before Release

1. **Test thoroughly**: Run full test suite on all supported distributions
2. **Review changes**: Ensure all changes are documented
3. **Check dependencies**: Verify all dependencies are available
4. **Update documentation**: Keep docs in sync with code

### During Release

1. **Follow checklist**: Complete all items in RELEASE_CHECKLIST.md
2. **Verify artifacts**: Check all packages build and install correctly
3. **Test installation**: Install packages on clean systems
4. **Sign releases**: Use GPG to sign release artifacts

### After Release

1. **Monitor feedback**: Watch for bug reports and issues
2. **Respond quickly**: Address critical issues with hotfixes
3. **Document issues**: Keep track of known issues
4. **Plan next release**: Create roadmap for next version

## Support

- **Issues**: https://github.com/lgx-platform/LGX/issues
- **Email**: team@lgx-platform.org
- **Documentation**: https://github.com/lgx-platform/LGX/tree/main/docs

## References

- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [Debian Policy Manual](https://www.debian.org/doc/debian-policy/)
- [Fedora Packaging Guidelines](https://docs.fedoraproject.org/en-US/packaging-guidelines/)
- [Arch Packaging Standards](https://wiki.archlinux.org/title/Arch_package_guidelines)
