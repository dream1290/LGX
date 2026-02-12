/**
 * @file benchmark_hardware_adaptation.c
 * @brief Benchmark hardware adaptation and tier detection overhead
 */

#include "benchmark_framework.h"
#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>

static void bench_get_hardware_status(void* ctx) {
    (void)ctx;
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    (void)status;
}

static void bench_has_capability(void* ctx) {
    (void)ctx;
    bool has_gpu = lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION);
    bool has_numa = lgx_runtime_has_capability(LGX_CAP_NUMA_AWARENESS);
    (void)has_gpu;
    (void)has_numa;
}

static void bench_get_version(void* ctx) {
    (void)ctx;
    lgx_version_t version = lgx_runtime_get_version();
    (void)version;
}

static void bench_check_compatibility(void* ctx) {
    (void)ctx;
    lgx_version_t required = {.major = 1, .minor = 0, .patch = 0};
    lgx_result_t result = lgx_runtime_check_compatibility(&required);
    (void)result;
}

int main(void) {
    printf("LGX Runtime Core - Hardware Adaptation Benchmark\n");
    printf("=================================================\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    if (lgx_runtime_init(config) != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        lgx_config_destroy(config);
        return 1;
    }
    
    benchmark_init();
    
    benchmark_result_t results[10];
    size_t result_count = 0;
    
    // Benchmark hardware status query
    results[result_count++] = benchmark_run("get_hardware_status", 
                                           bench_get_hardware_status, NULL, 100000, 10000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Benchmark capability check
    results[result_count++] = benchmark_run("has_capability", 
                                           bench_has_capability, NULL, 100000, 10000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Benchmark version query
    results[result_count++] = benchmark_run("get_version", 
                                           bench_get_version, NULL, 100000, 10000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Benchmark compatibility check
    results[result_count++] = benchmark_run("check_compatibility", 
                                           bench_check_compatibility, NULL, 100000, 10000);
    benchmark_print_result(&results[result_count - 1]);
    
    // Print hardware information
    printf("\nHardware Information:\n");
    printf("=====================\n");
    lgx_hardware_status_t hw_status = lgx_runtime_get_hardware_status();
    printf("Hardware Tier: %d\n", hw_status.achieved_tier);
    printf("GPU Available: %s\n", lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION) ? "Yes" : "No");
    printf("NUMA Available: %s\n", lgx_runtime_has_capability(LGX_CAP_NUMA_AWARENESS) ? "Yes" : "No");
    printf("Huge Pages: %s\n", lgx_runtime_has_capability(LGX_CAP_HUGE_PAGES) ? "Yes" : "No");
    
    lgx_version_t version = lgx_runtime_get_version();
    printf("Runtime Version: %d.%d.%d\n", version.major, version.minor, version.patch);
    
    // Save results
    benchmark_save_results("benchmark_hardware_adaptation.csv", results, result_count);
    
    benchmark_cleanup();
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\nResults saved to: benchmark_hardware_adaptation.csv\n");
    
    return 0;
}
