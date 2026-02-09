# Release Quick Start Guide

## TL;DR - Create a Release

```bash
# 1. Bump version
./scripts/version.sh bump minor "Add new features"

# 2. Update changelog
./scripts/changelog.sh update

# 3. Commit
git add -A && git commit -m "chore: release $(./scripts/version.sh show)"

# 4. Create release
./scripts/release.sh

# 5. Push
git push && git push --tags

# 6. Upload to GitHub
# Go to https://github.com/lgx-platform/LGX/releases/new
# Upload files from release/ directory
```

## Version Management

```bash
# Show version
./scripts/version.sh show

# Bump version
./scripts/version.sh bump patch    # 1.0.0 -> 1.0.1
./scripts/version.sh bump minor    # 1.0.1 -> 1.1.0
./scripts/version.sh bump major    # 1.1.0 -> 2.0.0

# Set specific version
./scripts/version.sh set 1.2.3 "Release message"
```

## Changelog

```bash
# Update changelog
./scripts/changelog.sh update

# Show changelog for version
./scripts/changelog.sh show 1.2.3

# Regenerate full changelog
./scripts/changelog.sh full
```

## Release

```bash
# Create release for current version
./scripts/release.sh

# Create release for specific version
./scripts/release.sh 1.2.3
```

## Hotfix

```bash
# 1. Create hotfix branch
git checkout -b hotfix/v1.0.1 v1.0.0

# 2. Fix bug and commit
git commit -m "fix: critical bug"

# 3. Bump patch version
./scripts/version.sh bump patch "Hotfix: critical bug"

# 4. Create release
./scripts/release.sh

# 5. Merge back
git checkout main && git merge hotfix/v1.0.1 && git push --tags
```

## Checklist

See `RELEASE_CHECKLIST.md` for complete validation checklist.

**Critical checks**:
- [ ] All tests passing
- [ ] No compiler warnings
- [ ] Performance targets met
- [ ] Documentation updated
- [ ] Packages build successfully

## More Information

- **Full Process**: `docs/13-packaging/release-process.md`
- **Checklist**: `RELEASE_CHECKLIST.md`
- **Changelog**: `CHANGELOG.md`
