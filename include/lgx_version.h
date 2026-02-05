/**
 * LGX Runtime Version - Version Macros and Constants
 * 
 * This header defines version information and compatibility macros.
 */

#ifndef LGX_VERSION_H
#define LGX_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

// Version constants
#define LGX_VERSION_MAJOR 1
#define LGX_VERSION_MINOR 0
#define LGX_VERSION_PATCH 0

// Version string
#define LGX_VERSION_STRING "1.0.0"

// Version as single integer (for comparisons)
#define LGX_VERSION_INT ((LGX_VERSION_MAJOR << 16) | (LGX_VERSION_MINOR << 8) | LGX_VERSION_PATCH)

// Compatibility macros
#define LGX_VERSION_AT_LEAST(major, minor, patch) \
    (LGX_VERSION_INT >= (((major) << 16) | ((minor) << 8) | (patch)))

#define LGX_VERSION_EXACTLY(major, minor, patch) \
    (LGX_VERSION_MAJOR == (major) && LGX_VERSION_MINOR == (minor) && LGX_VERSION_PATCH == (patch))

// ABI version (incremented when ABI breaks)
#define LGX_ABI_VERSION 1

// Build information (set by CMake)
#ifndef LGX_BUILD_TYPE
#define LGX_BUILD_TYPE "Unknown"
#endif

#ifndef LGX_BUILD_TIMESTAMP
#define LGX_BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

// Feature availability macros (compile-time)
#ifdef HAVE_JEMALLOC
#define LGX_HAS_JEMALLOC 1
#else
#define LGX_HAS_JEMALLOC 0
#endif

// Platform detection
#ifdef __linux__
#define LGX_PLATFORM_LINUX 1
#else
#define LGX_PLATFORM_LINUX 0
#endif

#ifdef __x86_64__
#define LGX_ARCH_X86_64 1
#else
#define LGX_ARCH_X86_64 0
#endif

#ifdef __aarch64__
#define LGX_ARCH_ARM64 1
#else
#define LGX_ARCH_ARM64 0
#endif

// Compiler detection
#ifdef __GNUC__
#define LGX_COMPILER_GCC 1
#define LGX_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#define LGX_COMPILER_GCC 0
#endif

#ifdef __clang__
#define LGX_COMPILER_CLANG 1
#else
#define LGX_COMPILER_CLANG 0
#endif

// API versioning support
#define LGX_API_VERSION_1_0 1

// Deprecation warnings
#if defined(__GNUC__) || defined(__clang__)
#define LGX_DEPRECATED __attribute__((deprecated))
#define LGX_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#else
#define LGX_DEPRECATED
#define LGX_DEPRECATED_MSG(msg)
#endif

// Symbol visibility
#if defined(__GNUC__) || defined(__clang__)
#define LGX_PUBLIC __attribute__((visibility("default")))
#define LGX_PRIVATE __attribute__((visibility("hidden")))
#else
#define LGX_PUBLIC
#define LGX_PRIVATE
#endif

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define LGX_LIKELY(x) __builtin_expect(!!(x), 1)
#define LGX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LGX_LIKELY(x) (x)
#define LGX_UNLIKELY(x) (x)
#endif

// Memory barriers and atomic operations
#if defined(__GNUC__) || defined(__clang__)
#define LGX_MEMORY_BARRIER() __sync_synchronize()
#define LGX_COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#define LGX_MEMORY_BARRIER()
#define LGX_COMPILER_BARRIER()
#endif

// Cache line size (architecture-specific)
#if defined(__x86_64__) || defined(__i386__)
#define LGX_CACHE_LINE_SIZE 64
#elif defined(__aarch64__)
#define LGX_CACHE_LINE_SIZE 64
#else
#define LGX_CACHE_LINE_SIZE 64  // Safe default
#endif

// Alignment macros
#define LGX_ALIGN(n) __attribute__((aligned(n)))
#define LGX_CACHE_ALIGNED LGX_ALIGN(LGX_CACHE_LINE_SIZE)

// Pack structures
#define LGX_PACKED __attribute__((packed))

#ifdef __cplusplus
}
#endif

#endif // LGX_VERSION_H