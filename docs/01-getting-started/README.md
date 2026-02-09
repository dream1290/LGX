# Getting Started with LGX Runtime Core

Welcome! This guide will help you get started with the LGX Runtime Core in under 5 minutes.

---

## 📋 Prerequisites

- Linux system (Ubuntu 22.04+, Fedora 38+, or Arch Linux)
- CMake 3.20+
- GCC 11+ or Clang 14+
- (Optional) Vulkan SDK for GPU features

---

## 🚀 Quick Start

### 1. Installation

See [installation.md](installation.md) for detailed installation instructions.

**Quick install (Ubuntu/Debian)**:
```bash
# Install from package
sudo apt install lgx-runtime

# Or build from source
git clone https://github.com/your-org/lgx-runtime.git
cd lgx-runtime
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### 2. Basic Usage

See [quick-start.md](quick-start.md) for a complete tutorial.

**Minimal example**:
```c
#include <lgx_runtime.h>

int main(void) {
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Allocate memory
    void* ptr = lgx_alloc(1024);
    
    // Use memory...
    
    // Free memory
    lgx_free(ptr);
    
    // Shutdown
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    return 0;
}
```

### 3. Integration

See [Integration Guide](../03-integration-guide/README.md) for CMake/Bazel integration.

---

## 📚 What's Next?

- **Learn the API**: [API Reference](../02-api-reference/README.md)
- **Integrate into your project**: [Integration Guide](../03-integration-guide/README.md)
- **Understand the architecture**: [Architecture](../04-architecture/README.md)
- **Optimize performance**: [Performance Guide](../05-performance/README.md)

---

## 🆘 Need Help?

- [FAQ](faq.md) - Frequently asked questions
- [Troubleshooting](../03-integration-guide/troubleshooting.md) - Common issues
- [GitHub Issues](https://github.com/your-org/lgx-runtime/issues) - Report bugs

---

## 📄 Documentation in This Section

- [installation.md](installation.md) - Detailed installation instructions
- [quick-start.md](quick-start.md) - 5-minute tutorial
- [faq.md](faq.md) - Frequently asked questions

---

**Next**: [Installation Guide](installation.md) →
