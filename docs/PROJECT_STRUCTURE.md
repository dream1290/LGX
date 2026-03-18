# LGX Runtime Platform - Project Structure

This document describes the complete project structure and organization.

## Root Directory

```
LGX/
├── src/                    # Source code
├── include/                # Public headers
├── tests/                  # Test suite (76 tests)
├── benchmarks/             # Performance benchmarks
├── docs/                   # Documentation
├── packaging/              # Distribution packages
├── build/                  # Main build directory (gitignored)
├── build-test/             # Test build directory (gitignored)
├── CMakeLists.txt          # Build configuration
├── README.md               # Project overview
├── CHANGELOG.md            # Version history
├── LICENSE                 # Apache 2.0 license
└── CONTRIBUTING.md         # Contribution guidelines
```

## Source Code (`src/`)

Core runtime implementation organized by module:
- Memory management (frame arena, GPU pool, persistent heap)
- Threading (job system, fibers, lock-free queues)
- Graphics (Vulkan wrapper)
- Input (gamepad, keyboard, mouse)
- Audio (3D spatialization, mixing)
- Profiling (frame profiler, zones)
- Networking (UDP, serialization)
- Asset pipeline (loading, hot reload)
- Tooling (performance analysis)

## Public Headers (`include/`)

API headers for application integration:
- `lgx_runtime.h` - Core runtime API
- `lgx_types.h` - Type definitions
- `lgx_version.h` - Version macros
- `lgx_integration.h` - Integration contracts
- Module-specific headers (threading, graphics, input, etc.)

## Tests (`tests/`)

Comprehensive test suite with 76 tests organized by category:
- Unit tests
- Integration tests
- Performance tests
- Security tests
- Hardware adaptation tests
- ABI stability tests

See `tests/README.md` for details.

## Benchmarks (`benchmarks/`)

Performance benchmarking suite:
- Allocation throughput
- Memory patterns
- Hardware adaptation
- Intent accuracy
- Telemetry overhead

See `benchmarks/README.md` for details.

## Documentation (`docs/`)

Organized into 10 numbered directories:

### 01. Getting Started
Quick start guides and tutorials

### 02. API Reference
Complete API documentation for all 9 modules:
- Runtime, Threading, Graphics, Input, Audio
- Profiling, Networking, Asset, Tooling

### 03. Architecture
System architecture and design documents

### 04. Integration
Integration guides and examples

### 05. Performance
Performance benchmarks, optimization reports, and targets

### 06. Testing
Testing strategy, chaos testing, fuzzing, failure injection

### 07. Security
Security model, threat analysis, audit preparation

### 08. Operations
Deployment, monitoring, hardware compatibility

### 09. Packaging
Distribution packaging for Ubuntu, Fedora, Arch Linux

### 10. Development
Development process, project history, strategic planning

See `docs/README.md` for complete documentation index.

## Packaging (`packaging/`)

Distribution packaging infrastructure:

```
packaging/
├── debian/              # Ubuntu/Debian .deb packages
├── rpm/                 # Fedora/RHEL .rpm packages
├── arch/                # Arch Linux .pkg.tar.zst packages
├── output/              # Built packages (gitignored)
├── build-*.sh           # Build scripts
├── install.sh           # Universal installer
├── uninstall.sh         # Universal uninstaller
└── *.md                 # Documentation
```

Supports:
- Ubuntu 22.04+, Debian 11+
- Fedora 38+, RHEL 8+, Rocky/Alma Linux
- Arch Linux, Manjaro

See `packaging/README.md` for packaging documentation.

## Build Directories (Gitignored)

### `build/`
Main CMake build directory containing:
- Compiled libraries (`.so` files)
- Test executables
- Benchmark executables
- CMake cache and configuration
- Build artifacts

Created by: `cmake -B build`

### `build-test/`
Separate build directory for testing:
- Test-specific builds
- Test executables
- Test artifacts

Created by: `cmake -B build-test`

### Build Directory Management

Build directories are gitignored via `.gitignore`:
```
/build/
/build-*/
```

To clean build artifacts:
```bash
# Remove all build directories
rm -rf build/ build-*/

# Rebuild
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Configuration Files

### `.gitignore`
Excludes:
- Build artifacts (`/build/`, `/build-*/`)
- Compiled binaries (`.o`, `.so`, `.a`)
- Package artifacts (`.deb`, `.rpm`, `.pkg.tar.zst`)
- Test results
- IDE files
- Security audits (kept private)

### `CMakeLists.txt`
CMake build configuration:
- Project metadata
- Module definitions
- Dependency management
- Install rules
- Test configuration

## Version Information

Current version: **v2.2** (Platform)
- Runtime Core: v1.0.1
- Individual modules: v1.0 - v2.2

See `CHANGELOG.md` for version history.

## License

Apache License 2.0 - See `LICENSE` file.

## Getting Started

1. **Clone repository:**
   ```bash
   git clone https://github.com/dream1290/LGX.git
   cd LGX
   ```

2. **Build:**
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

3. **Test:**
   ```bash
   cd build
   ctest --output-on-failure
   ```

4. **Install:**
   ```bash
   sudo cmake --install build
   ```

Or use pre-built packages from `packaging/` directory.

## Contributing

See `CONTRIBUTING.md` for contribution guidelines.

## Support

- Issues: https://github.com/dream1290/LGX/issues
- Documentation: `docs/`
- Discussions: https://github.com/dream1290/LGX/discussions
