#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Global state
static bool g_initialized = false;

// Runtime initialization
lgx_result_t lgx_runtime_init(void) {
    if (g_initialized) {
        fprintf(stderr, "LGX Runtime already initialized\n");
        return LGX_ERROR_INVALID_PARAM;
    }
    
    printf("LGX Runtime initialized (Phase 0)\n");
    g_initialized = true;
    return LGX_SUCCESS;
}

// Runtime shutdown
lgx_result_t lgx_runtime_shutdown(void) {
    if (!g_initialized) {
        fprintf(stderr, "LGX Runtime not initialized\n");
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    printf("LGX Runtime shutdown\n");
    g_initialized = false;
    return LGX_SUCCESS;
}

// Version query
lgx_version_t lgx_runtime_get_version(void) {
    lgx_version_t version = {
        .major = 1,
        .minor = 0,
        .patch = 0
    };
    return version;
}

// Memory allocation (Phase 0: just use malloc)
void* lgx_alloc(size_t size) {
    if (!g_initialized) {
        fprintf(stderr, "LGX Runtime not initialized\n");
        return NULL;
    }
    
    if (size == 0) {
        return NULL;
    }
    
    return malloc(size);
}

// Memory deallocation
void lgx_free(void* ptr) {
    if (ptr != NULL) {
        free(ptr);
    }
}
