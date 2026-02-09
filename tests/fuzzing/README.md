# LGX Runtime Fuzzing

This directory contains fuzzing harnesses for security testing of the LGX Runtime.

## Quick Start

### AFL Fuzzing

```bash
./build_afl.sh
afl-fuzz -i testcases -o findings ./fuzz_api_inputs
```

### libFuzzer

```bash
./build_libfuzzer.sh
./fuzz_allocation_patterns -max_total_time=300
```

## Files

- `fuzz_api_inputs.c` - AFL harness for API input fuzzing
- `fuzz_allocation_patterns.cpp` - libFuzzer harness for allocation patterns
- `build_afl.sh` - Build script for AFL
- `build_libfuzzer.sh` - Build script for libFuzzer
- `testcases/` - Initial test cases for AFL
- `findings/` - AFL output directory (crashes, hangs, queue)
- `corpus/` - libFuzzer corpus directory

## Requirements

### AFL
```bash
sudo apt-get install afl  # Ubuntu/Debian
sudo dnf install afl      # Fedora
```

### libFuzzer
```bash
sudo apt-get install clang  # Ubuntu/Debian
sudo dnf install clang      # Fedora
```

## Documentation

See `docs/SECURITY_TESTING.md` for comprehensive documentation.
