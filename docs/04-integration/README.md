# Integration Guide

## Overview

This guide covers integrating the LGX Runtime Core into your project's build system and application code.

## Contents

### Integration Guides
- [lgx_runtime_integration_guide.md](lgx_runtime_integration_guide.md) - Complete integration guide
- [CMake Integration](#cmake-integration)
- [Makefile Integration](#makefile-integration)
- [Meson Integration](#meson-integration)
- [Manual Compilation](#manual-compilation)
- [Runtime Configuration](#runtime-configuration)
- [Best Practices](#best-practices)

### Real-World Integration Examples
- [INTEGRATION_SUMMARY.md](INTEGRATION_SUMMARY.md) - SuperTuxKart integration summary
- [LGX_STK_INTEGRATION_PLAN.md](LGX_STK_INTEGRATION_PLAN.md) - Integration planning document
- [STK_LGX_INTEGRATION_COMPLETE.md](STK_LGX_INTEGRATION_COMPLETE.md) - Complete implementation details
- [STK_LGX_PROGRESS.md](STK_LGX_PROGRESS.md) - Integration progress tracking

## CMake Integration

### Using find_package

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyGame)

# Find LGX Runtime
find_package(lgx_runtime REQUIRED)

# Add your executable
add_executable(mygame src/main.c)

# Link against LGX Runtime
target_link_libraries(mygame PRIVATE lgx_runtime::lgx_runtime)
```

### Using pkg-config

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LGX_RUNTIME REQUIRED lgx_runtime)

add_executable(mygame src/main.c)
target_include_directories(mygame PRIVATE ${LGX_RUNTIME_INCLUDE_DIRS})
target_link_libraries(mygame PRIVATE ${LGX_RUNTIME_LIBRARIES})
target_link_directories(mygame PRIVATE ${LGX_RUNTIME_LIBRARY_DIRS})
```

### FetchContent (Build from Source)

```cmake
include(FetchContent)

FetchContent_Declare(
  lgx_runtime
  GIT_REPOSITORY https://github.com/dream1290/LGX.git
  GIT_TAG        v1.0.1
)

FetchContent_MakeAvailable(lgx_runtime)

add_executable(mygame src/main.c)
target_link_libraries(mygame PRIVATE lgx_runtime)
```

## Makefile Integration

### Using pkg-config

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2
CFLAGS += $(shell pkg-config --cflags lgx_runtime)
LDFLAGS += $(shell pkg-config --libs lgx_runtime)

mygame: src/main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f mygame
```

### Manual Paths

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I/usr/local/include
LDFLAGS = -L/usr/local/lib -llgx_runtime -lpthread

mygame: src/main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

## Meson Integration

### Using pkg-config

```meson
project('mygame', 'c', version: '1.0.0')

lgx_runtime = dependency('lgx_runtime')

executable('mygame',
  'src/main.c',
  dependencies: lgx_runtime
)
```

### Subproject

```meson
lgx_runtime_proj = subproject('lgx_runtime')
lgx_runtime = lgx_runtime_proj.get_variable('lgx_runtime_dep')

executable('mygame',
  'src/main.c',
  dependencies: lgx_runtime
)
```

## Manual Compilation

### Compile and Link

```bash
# Compile
gcc -c -o main.o src/main.c \
    -I/usr/local/include \
    -Wall -Wextra -O2

# Link
gcc -o mygame main.o \
    -L/usr/local/lib \
    -llgx_runtime \
    -lpthread

# Run
LD_LIBRARY_PATH=/usr/local/lib ./mygame
```

### Static Linking

```bash
gcc -o mygame src/main.c \
    -I/usr/local/include \
    -L/usr/local/lib \
    -static \
    -llgx_runtime \
    -lpthread \
    -lm
```

## Runtime Configuration

### Basic Initialization

```c
#include <lgx_runtime.h>

int main(void) {
    // Create configuration
    lgx_runtime_config_t* config = lgx_config_create();
    
    // Configure runtime
    lgx_config_set_memory_pool_size(config, 256 * 1024 * 1024); // 256MB
    lgx_config_set_log_path(config, "/var/log/mygame.log");
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    
    // Initialize
    lgx_result_t result = lgx_runtime_init(config);
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %s\n", 
                lgx_result_to_string(result));
        lgx_config_destroy(config);
        return 1;
    }
    
    // Your game code here
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return 0;
}
```

### Advanced Configuration

```c
lgx_runtime_config_t* config = lgx_config_create();

// Memory configuration
lgx_config_set_memory_pool_size(config, 512 * 1024 * 1024);
lgx_config_set_frame_arena_size(config, 64 * 1024 * 1024);
lgx_config_set_persistent_heap_size(config, 128 * 1024 * 1024);

// Performance tuning
lgx_config_set_thread_count(config, 8);
lgx_config_set_allocation_alignment(config, 64);

// Logging
lgx_config_set_log_path(config, "/var/log/mygame.log");
lgx_config_set_log_level(config, LGX_LOG_INFO);

// Features
uint32_t flags = LGX_CONFIG_ENABLE_TELEMETRY |
                 LGX_CONFIG_ENABLE_GPU_POOL |
                 LGX_CONFIG_ENABLE_HUGE_PAGES;
lgx_config_set_flags(config, flags);

// Resource limits
lgx_config_set_max_allocations(config, 1000000);
lgx_config_set_max_memory(config, 4ULL * 1024 * 1024 * 1024); // 4GB

lgx_result_t result = lgx_runtime_init(config);
```

## Best Practices

### 1. Error Handling

Always check return values:

```c
lgx_result_t result = lgx_runtime_init(config);
if (result != LGX_SUCCESS) {
    // Handle error
    const char* error_msg = lgx_result_to_string(result);
    log_error("Runtime init failed: %s", error_msg);
    return -1;
}
```

### 2. Memory Management

Use intent-based allocation for optimal performance:

```c
// Frame-scoped data (automatically freed)
void* frame_data = lgx_alloc_frame(1024);

// Persistent data (manual free required)
void* persistent_data = lgx_alloc_persistent(4096);
lgx_free(persistent_data);

// Level-scoped data (freed on level unload)
void* level_data = lgx_alloc_level(8192);
```

### 3. Lifecycle Management

Properly manage runtime lifecycle:

```c
// Initialize once at startup
lgx_runtime_init(config);

// Suspend when backgrounded
lgx_runtime_suspend();

// Resume when foregrounded
lgx_runtime_resume();

// Shutdown at exit
lgx_runtime_shutdown();
```

### 4. Performance Monitoring

Monitor runtime health:

```c
lgx_health_status_t health;
lgx_runtime_health_check(&health);

if (health.status != LGX_HEALTH_OK) {
    log_warning("Runtime health: %s", health.message);
}

// Get performance metrics
lgx_memory_stats_t stats;
lgx_memory_stats(&stats);
log_info("Memory usage: %zu / %zu bytes", 
         stats.used_bytes, stats.total_bytes);
```

### 5. Thread Safety

Understand thread safety guarantees:

```c
// Thread-safe operations
void* ptr1 = lgx_alloc(1024);  // Safe from any thread
void* ptr2 = lgx_alloc(2048);  // Safe from any thread
lgx_free(ptr1);                // Safe from any thread

// Thread-local operations
void* frame_ptr = lgx_alloc_frame(512);  // Per-thread arena
lgx_frame_reset();                       // Resets current thread's arena
```

## Troubleshooting

### Library Not Found

```bash
# Check installation
pkg-config --modversion lgx_runtime

# Set library path
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Or use rpath during compilation
gcc -o mygame src/main.c -llgx_runtime -Wl,-rpath,/usr/local/lib
```

### Header Not Found

```bash
# Check header installation
ls /usr/local/include/lgx_runtime.h

# Add include path
gcc -I/usr/local/include -c src/main.c
```

### Version Mismatch

```c
// Check runtime version
lgx_version_t version = lgx_runtime_get_version();
printf("Runtime version: %d.%d.%d\n", 
       version.major, version.minor, version.patch);

// Check compatibility
lgx_version_t required = {1, 0, 0};
lgx_result_t compat = lgx_runtime_check_compatibility(&required);
if (compat != LGX_SUCCESS) {
    fprintf(stderr, "Incompatible runtime version\n");
}
```

## Platform-Specific Notes

### Linux

- Requires kernel 5.10+
- pthread library required
- Vulkan optional (for GPU features)

### Distribution Packages

- **Debian/Ubuntu**: `sudo apt install liblgx-runtime-dev`
- **Fedora/RHEL**: `sudo dnf install lgx-runtime-devel`
- **Arch Linux**: `sudo pacman -S lgx-runtime`

## Examples

See [examples directory](../02-api-reference/examples/) for complete integration examples:

- [Basic Integration](../02-api-reference/examples/basic.c)
- [CMake Project](../02-api-reference/examples/cmake-project/)
- [Makefile Project](../02-api-reference/examples/makefile-project/)
- [Advanced Configuration](../02-api-reference/examples/advanced-config.c)

## Related Documentation

- [API Reference](../02-api-reference/README.md)
- [Getting Started](../01-getting-started/README.md)
- [Architecture](../03-architecture/README.md)
