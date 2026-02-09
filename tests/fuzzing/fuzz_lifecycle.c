/*
 * Lifecycle Fuzzing Harness
 * 
 * Fuzzes suspend/resume and initialization/shutdown sequences
 * to find state machine bugs and race conditions.
 */

#include "../../include/lgx_runtime.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// Lifecycle operations
typedef enum {
    OP_INIT = 0,
    OP_SHUTDOWN,
    OP_SUSPEND,
    OP_RESUME,
    OP_ALLOC,
    OP_FREE,
    OP_HEALTH_CHECK,
    OP_MAX
} lifecycle_op_t;

// Global state
static lgx_runtime_config_t* g_config = NULL;
static int g_initialized = 0;
static int g_suspended = 0;
static void* g_allocations[16] = {0};
static int g_alloc_count = 0;

// Reset state
static void reset_state(void) {
    // Free any outstanding allocations
    for (int i = 0; i < g_alloc_count; i++) {
        if (g_allocations[i]) {
            lgx_free(g_allocations[i]);
            g_allocations[i] = NULL;
        }
    }
    g_alloc_count = 0;
    
    // Shutdown if initialized
    if (g_initialized) {
        lgx_runtime_shutdown();
        g_initialized = 0;
    }
    
    // Destroy config
    if (g_config) {
        lgx_config_destroy(g_config);
        g_config = NULL;
    }
    
    g_suspended = 0;
}

// Execute a lifecycle operation
static void execute_op(lifecycle_op_t op, const uint8_t* data, size_t size) {
    lgx_result_t result;
    
    switch (op) {
        case OP_INIT:
            if (!g_initialized) {
                g_config = lgx_config_create();
                if (g_config) {
                    result = lgx_runtime_init(g_config);
                    if (result == LGX_SUCCESS) {
                        g_initialized = 1;
                    }
                }
            }
            break;
            
        case OP_SHUTDOWN:
            if (g_initialized && !g_suspended) {
                result = lgx_runtime_shutdown();
                if (result == LGX_SUCCESS) {
                    g_initialized = 0;
                }
            }
            break;
            
        case OP_SUSPEND:
            if (g_initialized && !g_suspended) {
                result = lgx_runtime_suspend();
                if (result == LGX_SUCCESS) {
                    g_suspended = 1;
                }
            }
            break;
            
        case OP_RESUME:
            if (g_initialized && g_suspended) {
                result = lgx_runtime_resume();
                if (result == LGX_SUCCESS) {
                    g_suspended = 0;
                }
            }
            break;
            
        case OP_ALLOC:
            if (g_initialized && !g_suspended && g_alloc_count < 16 && size > 0) {
                size_t alloc_size = (data[0] % 64) * 1024 + 64;  // 64B to 64KB
                void* ptr = lgx_alloc(alloc_size);
                if (ptr) {
                    g_allocations[g_alloc_count++] = ptr;
                }
            }
            break;
            
        case OP_FREE:
            if (g_initialized && !g_suspended && g_alloc_count > 0) {
                int idx = data[0] % g_alloc_count;
                if (g_allocations[idx]) {
                    lgx_free(g_allocations[idx]);
                    g_allocations[idx] = NULL;
                }
            }
            break;
            
        case OP_HEALTH_CHECK:
            if (g_initialized) {
                lgx_health_status_t status;
                lgx_runtime_health_check(&status);
            }
            break;
            
        default:
            break;
    }
}

// AFL fuzzing entry point
#ifdef __AFL_FUZZ_TESTCASE_LEN
__AFL_FUZZ_INIT();
#endif

int main(int argc, char** argv) {
#ifdef __AFL_FUZZ_TESTCASE_LEN
    // AFL persistent mode
    __AFL_INIT();
    
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    
    while (__AFL_LOOP(1000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        
        if (len < 1) continue;
        
        // Execute sequence of operations
        for (int i = 0; i < len && i < 100; i++) {
            lifecycle_op_t op = buf[i] % OP_MAX;
            execute_op(op, &buf[i], len - i);
        }
        
        // Reset state for next iteration
        reset_state();
    }
#else
    // Standalone mode - read from file
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(f);
        return 1;
    }
    
    fread(data, 1, size, f);
    fclose(f);
    
    // Execute sequence
    for (int i = 0; i < size && i < 100; i++) {
        lifecycle_op_t op = data[i] % OP_MAX;
        execute_op(op, &data[i], size - i);
    }
    
    reset_state();
    free(data);
#endif
    
    return 0;
}
