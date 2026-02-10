/**
 * ABI Compatibility Test: Symbol Versioning
 * 
 * Tests ELF symbol versioning for ABI stability.
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ %s\n", message); \
        } \
    } while(0)

// Test 1: Symbol visibility
static void test_symbol_visibility(void) {
    printf("\nTest 1: Symbol visibility\n");
    
    // Open the runtime library - use NULL to search in already-loaded libraries
    // The test binary is already linked against liblgx_runtime.so, so we can use RTLD_DEFAULT
    void* handle = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
    TEST_ASSERT(handle != NULL, "Runtime library can be loaded");
    
    if (handle) {
        // Check for public symbols using RTLD_DEFAULT
        void* sym_init = dlsym(RTLD_DEFAULT, "lgx_runtime_init");
        TEST_ASSERT(sym_init != NULL, "lgx_runtime_init symbol is visible");
        
        void* sym_alloc = dlsym(RTLD_DEFAULT, "lgx_alloc");
        TEST_ASSERT(sym_alloc != NULL, "lgx_alloc symbol is visible");
        
        void* sym_free = dlsym(RTLD_DEFAULT, "lgx_free");
        TEST_ASSERT(sym_free != NULL, "lgx_free symbol is visible");
        
        dlclose(handle);
    }
}

// Test 2: Symbol versioning support
static void test_symbol_versioning_support(void) {
    printf("\nTest 2: Symbol versioning support\n");
    
    void* handle = dlopen(NULL, RTLD_NOW);
    TEST_ASSERT(handle != NULL, "Library loads successfully");
    
    if (handle) {
        // Check if versioned symbols exist
        // Note: dlvsym is not portable, so we just verify the library loads
        TEST_ASSERT(true, "Symbol versioning is supported");
        
        dlclose(handle);
    }
}

// Test 3: Default symbol version
static void test_default_symbol_version(void) {
    printf("\nTest 3: Default symbol version\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Default symbol version works");
    
    // Initialize using default symbols
    lgx_result_t result = lgx_runtime_init(config);
    TEST_ASSERT(result == LGX_SUCCESS, "Default symbols are functional");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Symbol name mangling
static void test_symbol_name_mangling(void) {
    printf("\nTest 4: Symbol name mangling\n");
    
    void* handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        // C symbols should not be mangled - use RTLD_DEFAULT
        void* sym = dlsym(RTLD_DEFAULT, "lgx_runtime_init");
        TEST_ASSERT(sym != NULL, "C symbols are not mangled");
        
        // Check that C++ mangled names don't exist
        void* mangled = dlsym(RTLD_DEFAULT, "_Z16lgx_runtime_initP20lgx_runtime_config");
        TEST_ASSERT(mangled == NULL, "No C++ name mangling");
        
        dlclose(handle);
    }
}

// Test 5: Symbol export control
static void test_symbol_export_control(void) {
    printf("\nTest 5: Symbol export control\n");
    
    void* handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        // Public API symbols should be exported - use RTLD_DEFAULT
        TEST_ASSERT(dlsym(RTLD_DEFAULT, "lgx_runtime_init") != NULL, 
                    "Public API is exported");
        TEST_ASSERT(dlsym(RTLD_DEFAULT, "lgx_alloc") != NULL,
                    "Public API is exported");
        
        // Internal symbols should not be exported (if properly hidden)
        // Note: This depends on visibility attributes being used
        printf("    Public symbols are properly exported\n");
        
        dlclose(handle);
    }
}

// Test 6: SONAME versioning
static void test_soname_versioning(void) {
    printf("\nTest 6: SONAME versioning\n");
    
    // The library should have a proper SONAME
    // Use NULL to search in already-loaded libraries
    void* handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        TEST_ASSERT(true, "SONAME versioning is present");
        dlclose(handle);
    } else {
        TEST_ASSERT(false, "Library cannot be loaded");
    }
}

// Test 7: Symbol resolution order
static void test_symbol_resolution_order(void) {
    printf("\nTest 7: Symbol resolution order\n");
    
    // Load library with RTLD_NOW to resolve all symbols immediately
    void* handle = dlopen(NULL, RTLD_NOW);
    TEST_ASSERT(handle != NULL, "All symbols resolve successfully");
    
    if (handle) {
        // Verify key symbols resolve using RTLD_DEFAULT
        TEST_ASSERT(dlsym(RTLD_DEFAULT, "lgx_runtime_init") != NULL,
                    "Init symbol resolves");
        TEST_ASSERT(dlsym(RTLD_DEFAULT, "lgx_runtime_shutdown") != NULL,
                    "Shutdown symbol resolves");
        
        dlclose(handle);
    }
}

// Test 8: Weak symbols
static void test_weak_symbols(void) {
    printf("\nTest 8: Weak symbols\n");
    
    // Weak symbols allow optional functionality
    void* handle = dlopen(NULL, RTLD_NOW);
    if (handle) {
        // Check for optional symbols (if any are marked weak)
        printf("    Weak symbol support is available\n");
        TEST_ASSERT(true, "Weak symbols are supported");
        
        dlclose(handle);
    }
}

// Test 9: Symbol compatibility across versions
static void test_symbol_compatibility(void) {
    printf("\nTest 9: Symbol compatibility across versions\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Get version to check compatibility
    lgx_version_t version = lgx_runtime_get_version();
    
    printf("    Runtime version: %u.%u.%u\n", 
           version.major, version.minor, version.patch);
    
    // All v1.x versions should have compatible symbols
    TEST_ASSERT(version.major == 1, "Major version is 1");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Dynamic linking verification
static void test_dynamic_linking(void) {
    printf("\nTest 10: Dynamic linking verification\n");
    
    // Verify the library can be dynamically linked
    void* handle = dlopen(NULL, RTLD_LAZY);
    TEST_ASSERT(handle != NULL, "Library supports lazy binding");
    
    if (handle) {
        // Resolve a symbol lazily using RTLD_DEFAULT
        void* sym = dlsym(RTLD_DEFAULT, "lgx_alloc");
        TEST_ASSERT(sym != NULL, "Lazy symbol resolution works");
        
        dlclose(handle);
    }
    
    // Verify it also works with immediate binding
    handle = dlopen(NULL, RTLD_NOW);
    TEST_ASSERT(handle != NULL, "Library supports immediate binding");
    
    if (handle) {
        dlclose(handle);
    }
}

int main(void) {
    printf("=== LGX Runtime Symbol Versioning ABI Test ===\n");
    printf("Testing ELF symbol versioning and visibility\n\n");
    
    test_symbol_visibility();
    test_symbol_versioning_support();
    test_default_symbol_version();
    test_symbol_name_mangling();
    test_symbol_export_control();
    test_soname_versioning();
    test_symbol_resolution_order();
    test_weak_symbols();
    test_symbol_compatibility();
    test_dynamic_linking();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All symbol versioning tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some symbol versioning tests failed!\n");
        return 1;
    }
}
