/**
 * AFL Fuzzing Harness for LGX Runtime API Inputs
 * 
 * This fuzzer tests the robustness of the LGX Runtime API by feeding
 * random/malformed inputs to various API functions.
 * 
 * Build with AFL:
 *   afl-gcc -o fuzz_api_inputs fuzz_api_inputs.c -I../../include -L../../build -llgx_runtime
 * 
 * Run with AFL:
 *   afl-fuzz -i testcases -o findings ./fuzz_api_inputs
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

// Fuzzing input structure
typedef struct {
    uint8_t api_function;
    uint8_t param_count;
    uint64_t params[8];
    uint8_t string_data[256];
} fuzz_input_t;

// Initialize runtime once (reused across fuzzing iterations)
static int runtime_initialized = 0;
static lgx_runtime_config_t* global_config = NULL;

static void init_runtime_once(void) {
    if (!runtime_initialized) {
        global_config = lgx_config_create();
        if (global_config) {
            lgx_runtime_init(global_config);
            runtime_initialized = 1;
        }
    }
}

// Fuzz allocation functions
static void fuzz_allocation(fuzz_input_t* input) {
    size_t size = input->params[0];
    size_t alignment = input->params[1];
    
    // Test lgx_alloc with various sizes
    void* ptr1 = lgx_alloc(size);
    if (ptr1) {
        lgx_free(ptr1);
    }
    
    // Test lgx_alloc_aligned with various alignments
    void* ptr2 = lgx_alloc_aligned(size, alignment);
    if (ptr2) {
        lgx_free(ptr2);
    }
}

// Fuzz configuration functions
static void fuzz_configuration(fuzz_input_t* input) {
    lgx_runtime_config_t* config = lgx_config_create();
    if (!config) return;
    
    // Fuzz various config setters with random values
    // Note: These are placeholder calls - actual setters depend on API
    
    lgx_config_destroy(config);
}

// Fuzz capability queries
static void fuzz_capability_queries(fuzz_input_t* input) {
    uint32_t capability = (uint32_t)input->params[0];
    uint32_t capabilities = 0;
    
    // Test capability checks with random values
    lgx_runtime_has_capability((lgx_capability_t)capability);
    lgx_runtime_query_capabilities(&capabilities);
}

// Fuzz health check API
static void fuzz_health_check(fuzz_input_t* input) {
    lgx_health_status_t health_status;
    health_status.struct_size = sizeof(lgx_health_status_t);
    
    lgx_runtime_health_check(&health_status);
}

// Fuzz string inputs (paths, etc.)
static void fuzz_string_inputs(fuzz_input_t* input) {
    // Ensure null termination
    input->string_data[255] = '\0';
    
    // Test filesystem operations with fuzzed paths
    lgx_file_handle_t* file = lgx_fs_open((const char*)input->string_data, "r");
    if (file) {
        lgx_fs_close(file);
    }
}

// Fuzz frame arena operations
static void fuzz_frame_arena(fuzz_input_t* input) {
    size_t size = input->params[0];
    
    // Test frame allocation with various sizes
    void* ptr = lgx_frame_alloc(size);
    (void)ptr; // Frame allocations don't need explicit free
    
    // Test frame reset
    if (input->params[1] & 1) {
        lgx_frame_reset();
    }
}

// Fuzz GPU operations (if available)
static void fuzz_gpu_operations(fuzz_input_t* input) {
    if (!lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
        return;
    }
    
    size_t size = input->params[0];
    size_t alignment = input->params[1];
    uint32_t memory_type = (uint32_t)input->params[2];
    
    // Test GPU allocation with various parameters
    lgx_gpu_allocation_t* alloc = lgx_gpu_alloc(size, alignment, memory_type);
    if (alloc) {
        lgx_gpu_free(alloc, memory_type);
    }
}

// Fuzz telemetry operations
static void fuzz_telemetry(fuzz_input_t* input) {
    // Test telemetry configuration with random values
    lgx_telemetry_set_enabled(input->params[0] & 1);
}

// Fuzz input validation functions
static void fuzz_input_validation(fuzz_input_t* input) {
    void* ptr = (void*)input->params[0];
    size_t size = input->params[1];
    size_t alignment = input->params[2];
    
    // Test validation functions with random inputs
    lgx_validate_pointer(ptr);
    lgx_validate_size(size);
    lgx_validate_allocation_size(size);
    lgx_validate_alignment(alignment);
    lgx_validate_string((const char*)input->string_data, 256);
}

// Main fuzzing entry point
int main(int argc, char** argv) {
    fuzz_input_t input;
    
    // Initialize runtime once
    init_runtime_once();
    
    // Read fuzzing input from stdin (AFL standard)
    ssize_t bytes_read = read(STDIN_FILENO, &input, sizeof(input));
    if (bytes_read < (ssize_t)sizeof(input)) {
        // Not enough data, use partial input
        memset(&input, 0, sizeof(input));
        if (bytes_read > 0) {
            memcpy(&input, &input, bytes_read);
        }
    }
    
    // Dispatch to different fuzzing targets based on api_function
    switch (input.api_function % 8) {
        case 0:
            fuzz_allocation(&input);
            break;
        case 1:
            fuzz_configuration(&input);
            break;
        case 2:
            fuzz_capability_queries(&input);
            break;
        case 3:
            fuzz_health_check(&input);
            break;
        case 4:
            fuzz_string_inputs(&input);
            break;
        case 5:
            fuzz_frame_arena(&input);
            break;
        case 6:
            fuzz_gpu_operations(&input);
            break;
        case 7:
            fuzz_telemetry(&input);
            break;
    }
    
    // Additional validation fuzzing
    fuzz_input_validation(&input);
    
    return 0;
}
