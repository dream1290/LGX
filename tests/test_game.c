#include "lgx_runtime.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== LGX Runtime Phase 0 Test ===\n\n");
    
    // Test 1: Get version
    lgx_version_t version = lgx_runtime_get_version();
    printf("LGX Runtime Version: %u.%u.%u\n", 
           version.major, version.minor, version.patch);
    
    // Test 2: Initialize runtime
    lgx_result_t result = lgx_runtime_init();
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        return 1;
    }
    
    // Test 3: Allocate memory
    void* ptr = lgx_alloc(1024);
    if (ptr == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }
    printf("Allocated 1024 bytes: %p\n", ptr);
    
    // Test 4: Use the memory
    memset(ptr, 0xAB, 1024);
    printf("Filled memory with pattern 0xAB\n");
    
    // Test 5: Free memory
    lgx_free(ptr);
    printf("Freed memory\n");
    
    // Test 6: Shutdown runtime
    result = lgx_runtime_shutdown();
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to shutdown runtime\n");
        return 1;
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
