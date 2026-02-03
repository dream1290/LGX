# LGX Runtime - Phase 0 Prototype

## Quick Start

### Build and Run

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
make

# Run test
./test_game
```

### Expected Output

```
=== LGX Runtime Phase 0 Test ===

LGX Runtime Version: 1.0.0
LGX Runtime initialized (Phase 0)
Allocated 1024 bytes: 0x...
Filled memory with pattern 0xAB
Freed memory
LGX Runtime shutdown

=== All tests passed! ===
```

## Phase 0 Goals

- ✅ Minimal runtime with init/shutdown
- ✅ Simple allocator (malloc wrapper)
- ✅ Version check
- ✅ Test game that links against runtime

## What's Next

After Phase 0 validation:
1. Measure initialization time
2. Test on real hardware
3. Write validation report
4. Proceed to Phase 1 (lock-free allocator)
