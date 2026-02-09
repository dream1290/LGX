# LGX Runtime Core - Release Validation Checklist

This checklist ensures that all release requirements are met before publishing a new version.

## Pre-Release Checklist

### 1. Code Quality

- [ ] All tests passing
  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  cd build && ctest --output-on-failure
  ```

- [ ] No compiler warnings
  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-Werror"
  cmake --build build
  ```

- [ ] Static analysis clean
  ```bash
  ./scripts/run_static_analysis.sh
  ```

- [ ] No memory leaks (Valgrind)
  ```bash
  valgrind --leak-check=full --show-leak-kinds=all ./build/tests/unit/unit_test_*
  ```

- [ ] Address sanitizer clean
  ```bash
  cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug
  cmake --build build-asan
  cd build-asan && ctest
  ```

### 2. Performance Validation

- [ ] Allocation latency meets targets
  - Frame arena: P99 < 100ns ✅
  - General allocations: P99 < 5μs (Tier 1) ✅
  ```bash
  ./build/tests/performance/perf_test_allocation_latency
  ```

- [ ] Memory overhead meets targets
  - Runtime overhead < 200MB (Tier 2) ✅
  ```bash
  ./build/tests/performance/test_memory_footprint
  ```

- [ ] Initialization time meets targets
  - Init time < 500ms (Tier 2) ✅
  ```bash
  ./build/tests/performance/test_initialization_time
  ```

- [ ] No performance regressions
  ```bash
  ./scripts/compare_performance.py baseline.json current.json
  ```

### 3. ABI Compatibility

- [ ] ABI compatibility maintained
  ```bash
  ./scripts/lgx-abi-test-matrix.sh
  ```

- [ ] Symbol versioning correct
  ```bash
  nm -D build/liblgx_runtime.so | grep lgx_
  ```

- [ ] No unintended symbol exports
  ```bash
  nm -D build/liblgx_runtime.so | grep -v lgx_ | grep -v " U "
  ```

### 4. Security

- [ ] Fuzzing tests pass (no crashes)
  ```bash
  cd tests/fuzzing
  ./build_libfuzzer.sh
  # Run for at least 1 hour
  ```

- [ ] No security vulnerabilities (Coverity)
  ```bash
  ./scripts/run_coverity_scan.sh
  ```

- [ ] Input validation complete
  ```bash
  ./build/tests/phase0/test_input_validation
  ```

- [ ] Memory safety features enabled
  ```bash
  ./build/tests/phase0/test_memory_safety
  ```

### 5. Documentation

- [ ] CHANGELOG.md updated
  ```bash
  ./scripts/changelog.sh update $(./scripts/version.sh show)
  ```

- [ ] README.md up to date
- [ ] API documentation complete
- [ ] Integration guide reviewed
- [ ] All code examples tested

### 6. Version Management

- [ ] Version bumped correctly
  ```bash
  ./scripts/version.sh show
  ```

- [ ] Version consistent across files:
  - [ ] CMakeLists.txt
  - [ ] include/lgx_version.h
  - [ ] packaging/debian/changelog
  - [ ] packaging/rpm/lgx-runtime.spec
  - [ ] packaging/arch/PKGBUILD
  - [ ] packaging/arch/.SRCINFO

- [ ] Git working directory clean
  ```bash
  git status
  ```

### 7. Packaging

- [ ] All packages build successfully
  ```bash
  cd packaging
  ./build-all.sh
  ```

- [ ] Debian package quality
  ```bash
  lintian packaging/output/lgx-runtime_*.deb
  ```

- [ ] RPM package quality
  ```bash
  rpmlint packaging/output/lgx-runtime-*.rpm
  ```

- [ ] Arch package quality
  ```bash
  namcap packaging/output/lgx-runtime-*.pkg.tar.zst
  ```

- [ ] Package installation works
  ```bash
  # Test on clean VM/container for each distribution
  ./packaging/install.sh
  pkg-config --modversion lgx_runtime
  ```

### 8. Integration Testing

- [ ] Test with sample application
  ```bash
  cat > test_app.c << 'EOF'
  #include <lgx_runtime.h>
  #include <stdio.h>
  
  int main() {
      lgx_config_t* config = lgx_config_create();
      lgx_result_t result = lgx_runtime_init(config);
      if (result == LGX_SUCCESS) {
          printf("LGX Runtime initialized successfully\n");
          lgx_runtime_shutdown();
      }
      lgx_config_destroy(config);
      return 0;
  }
  EOF
  
  gcc test_app.c $(pkg-config --cflags --libs lgx_runtime) -o test_app
  ./test_app
  ```

- [ ] Test with CMake integration
  ```bash
  cat > CMakeLists.txt << 'EOF'
  cmake_minimum_required(VERSION 3.16)
  project(test_app)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LGX REQUIRED lgx_runtime)
  add_executable(test_app test_app.c)
  target_link_libraries(test_app ${LGX_LIBRARIES})
  EOF
  
  cmake -B build && cmake --build build
  ./build/test_app
  ```

### 9. Cross-Distribution Testing

- [ ] Ubuntu 22.04 LTS
  - [ ] Build succeeds
  - [ ] Tests pass
  - [ ] Package installs
  - [ ] Sample app works

- [ ] Ubuntu 24.04 LTS
  - [ ] Build succeeds
  - [ ] Tests pass
  - [ ] Package installs
  - [ ] Sample app works

- [ ] Fedora 40
  - [ ] Build succeeds
  - [ ] Tests pass
  - [ ] Package installs
  - [ ] Sample app works

- [ ] Arch Linux
  - [ ] Build succeeds
  - [ ] Tests pass
  - [ ] Package installs
  - [ ] Sample app works

### 10. Hardware Testing

- [ ] Test on different CPU architectures
  - [ ] x86_64 (Intel)
  - [ ] x86_64 (AMD)

- [ ] Test with different GPU vendors
  - [ ] NVIDIA (if available)
  - [ ] AMD (if available)
  - [ ] Intel (if available)

- [ ] Test graceful degradation
  - [ ] Without huge pages
  - [ ] Without Vulkan
  - [ ] Without jemalloc

## Release Process

### 1. Prepare Release

- [ ] Run pre-release checklist (above)
- [ ] Update version
  ```bash
  ./scripts/version.sh bump <major|minor|patch> "Release message"
  ```

- [ ] Update changelog
  ```bash
  ./scripts/changelog.sh update
  ```

- [ ] Commit version bump
  ```bash
  git add -A
  git commit -m "chore: bump version to $(./scripts/version.sh show)"
  ```

### 2. Create Release

- [ ] Run release script
  ```bash
  ./scripts/release.sh
  ```

- [ ] Verify release artifacts
  ```bash
  ls -lh release/
  sha256sum -c release/SHA256SUMS
  ```

- [ ] Push tag
  ```bash
  git push origin v$(./scripts/version.sh show)
  ```

### 3. Publish Release

- [ ] Create GitHub release
  - Go to: https://github.com/lgx-platform/LGX/releases/new
  - Tag: v$(./scripts/version.sh show)
  - Title: LGX Runtime Core $(./scripts/version.sh show)
  - Description: Copy from release/RELEASE_NOTES.md
  - Upload files from release/

- [ ] Publish to distribution repositories
  - [ ] Ubuntu PPA
    ```bash
    dput ppa:lgx-platform/stable ../lgx-runtime_*.changes
    ```
  
  - [ ] Fedora COPR
    ```bash
    copr-cli build lgx-runtime packaging/rpm/lgx-runtime.spec
    ```
  
  - [ ] Arch User Repository (AUR)
    ```bash
    cd /tmp
    git clone ssh://aur@aur.archlinux.org/lgx-runtime.git
    cp packaging/arch/PKGBUILD lgx-runtime/
    cp packaging/arch/.SRCINFO lgx-runtime/
    cd lgx-runtime
    git add PKGBUILD .SRCINFO
    git commit -m "Update to $(./scripts/version.sh show)"
    git push
    ```

### 4. Post-Release

- [ ] Announce release
  - [ ] Project website
  - [ ] Mailing list
  - [ ] Social media (Twitter, Reddit, etc.)
  - [ ] Gaming forums

- [ ] Update documentation website
- [ ] Monitor for issues
  - [ ] GitHub issues
  - [ ] Distribution bug trackers
  - [ ] User feedback

- [ ] Create milestone for next release

## Emergency Hotfix Process

If a critical bug is found after release:

1. [ ] Create hotfix branch from release tag
   ```bash
   git checkout -b hotfix/v1.0.1 v1.0.0
   ```

2. [ ] Fix the bug
3. [ ] Update version (patch bump)
   ```bash
   ./scripts/version.sh bump patch "Hotfix: <description>"
   ```

4. [ ] Run critical tests
   - [ ] Unit tests
   - [ ] Integration tests
   - [ ] Affected functionality

5. [ ] Create hotfix release
   ```bash
   ./scripts/release.sh
   ```

6. [ ] Merge back to main
   ```bash
   git checkout main
   git merge hotfix/v1.0.1
   git push
   ```

## Rollback Process

If a release needs to be rolled back:

1. [ ] Delete GitHub release
2. [ ] Delete git tag
   ```bash
   git tag -d v1.0.0
   git push origin :refs/tags/v1.0.0
   ```

3. [ ] Remove packages from repositories
4. [ ] Announce rollback
5. [ ] Investigate and fix issues
6. [ ] Create new release with fixes

## Notes

- This checklist should be reviewed and updated with each release
- All checkboxes must be completed before publishing a release
- Document any deviations from this checklist
- Keep a copy of completed checklists for each release

## Release Approval

- [ ] Technical Lead approval
- [ ] QA approval
- [ ] Security review approval
- [ ] Documentation review approval

**Release Manager:** ___________________  
**Date:** ___________________  
**Version:** ___________________  
**Approved:** [ ] Yes [ ] No
