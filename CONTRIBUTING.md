# Contributing to LGX Runtime Core

Thank you for your interest in contributing to LGX Runtime Core! This document provides guidelines for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Release Process](#release-process)

## Code of Conduct

This project adheres to a code of conduct. By participating, you are expected to uphold this code. Please report unacceptable behavior to team@lgx-platform.org.

## Getting Started

### Prerequisites

- GCC 9+ or Clang 10+
- CMake 3.16+
- Git
- Vulkan SDK (optional, for GPU features)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/dream1290/LGX.git
cd LGX

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run tests
cd build && ctest
```

## Development Workflow

### 1. Fork and Clone

```bash
# Fork the repository on GitHub
# Clone your fork
git clone https://github.com/YOUR_USERNAME/LGX.git
cd LGX

# Add upstream remote
git remote add upstream https://github.com/dream1290/LGX.git
```

### 2. Create a Branch

```bash
# Create a feature branch
git checkout -b feature/your-feature-name

# Or a bugfix branch
git checkout -b fix/issue-number-description
```

### 3. Make Changes

- Write clean, readable code
- Follow the coding standards (see below)
- Add tests for new features
- Update documentation as needed

### 4. Test Your Changes

```bash
# Build and run all tests
cmake --build build
cd build && ctest --output-on-failure

# Run specific tests
ctest -R test_name

# Run with AddressSanitizer
cmake -B build-asan -DCMAKE_C_FLAGS="-fsanitize=address"
cmake --build build-asan
cd build-asan && ctest
```

### 5. Commit Your Changes

```bash
# Stage your changes
git add .

# Commit with a descriptive message
git commit -m "feat: add new feature X

- Detailed description of changes
- Why the change was made
- Any breaking changes"
```

#### Commit Message Format

Follow conventional commits:

- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation changes
- `test:` Test additions or changes
- `refactor:` Code refactoring
- `perf:` Performance improvements
- `chore:` Build process or auxiliary tool changes

## Coding Standards

### C Code Style

- **Indentation:** 4 spaces (no tabs)
- **Line length:** 100 characters maximum
- **Naming:**
  - Functions: `lgx_snake_case()`
  - Types: `lgx_snake_case_t`
  - Macros: `LGX_UPPER_CASE`
  - Private functions: `static` with descriptive names

### Code Quality

- No compiler warnings with `-Wall -Wextra -Wpedantic`
- Pass static analysis (`-fanalyzer`)
- ISO C11 compliant
- No variable length arrays (VLAs)
- Proper error handling
- Memory safety (no leaks, no use-after-free)

### Documentation

- Document all public APIs with Doxygen comments
- Include usage examples
- Update README.md if adding new features
- Update CHANGELOG.md

## Testing

### Test Requirements

- All new features must have tests
- Bug fixes must include regression tests
- Maintain or improve code coverage
- Tests must pass on all supported platforms

### Test Types

1. **Unit Tests** - Test individual functions
2. **Integration Tests** - Test component interactions
3. **Performance Tests** - Verify performance targets
4. **ABI Tests** - Ensure ABI compatibility

### Running Tests

```bash
# All tests
ctest

# Specific test suite
ctest -R unit_

# With verbose output
ctest --output-on-failure --verbose

# Memory leak check
valgrind --leak-check=full ./build/tests/unit/test_name
```

## Submitting Changes

### Pull Request Process

1. **Update your branch**
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Push to your fork**
   ```bash
   git push origin feature/your-feature-name
   ```

3. **Create Pull Request**
   - Go to GitHub and create a PR
   - Fill out the PR template
   - Link related issues
   - Request review from maintainers

### PR Requirements

- All tests pass
- No compiler warnings
- Code follows style guidelines
- Documentation updated
- CHANGELOG.md updated
- Commits are clean and descriptive

### Review Process

- Maintainers will review your PR
- Address feedback and update PR
- Once approved, maintainers will merge

## Release Process

### Version Numbering

We follow [Semantic Versioning](https://semver.org/):

- **MAJOR:** Breaking changes
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes (backward compatible)

### Release Checklist

1. Update version in:
   - `CMakeLists.txt`
   - `include/lgx_version.h`
   - `packaging/debian/changelog`
   - `packaging/rpm/lgx-runtime.spec`
   - `packaging/arch/PKGBUILD`

2. Update `CHANGELOG.md`

3. Run full test suite

4. Create git tag:
   ```bash
   git tag -a v1.0.1 -m "Release v1.0.1"
   git push origin v1.0.1
   ```

5. Create GitHub Release with release notes

6. Build and upload packages

## What NOT to Commit

The following should NEVER be committed to the repository:

- Build directories (`build/`, `build-*/`)
- IDE configuration (`.vscode/`, `.idea/`, `.kiro/`)
- Build artifacts (`*.o`, `*.a`, `*.so`)
- Test results files
- Temporary files (`*.tmp`, `*.log`)
- Security audit results
- Package outputs (`*.deb`, `*.rpm`, `*.tar.gz`)
- Development notes and summaries

Always check `.gitignore` and use `git status` before committing.

## Getting Help

- **Documentation:** https://lgx-platform.org/docs
- **Issues:** https://github.com/dream1290/LGX/issues
- **Discussions:** https://github.com/dream1290/LGX/discussions
- **Email:** team@lgx-platform.org

## License

By contributing to LGX Runtime Core, you agree that your contributions will be licensed under the Apache License 2.0.

---

Thank you for contributing to LGX Runtime Core!
