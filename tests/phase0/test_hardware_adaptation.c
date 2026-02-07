#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/lgx_runtime.h"

static const char* tier_to_string(lgx_hardware_tier_t tier) {
    switch (tier) {
        case LGX_HW_TIER_OPTIMAL: return "OPTIMAL";
        case LGX_HW_TIER_COMPATIBLE: return "COMPATIBLE";
        case LGX_HW_TIER_DEGRADED: return "DEGRADED";
        default: return "UNKNOWN";
    }
}

static void test_hardware_detection(void) {
    printf("Testing hardware detection and classification...\n");
    
    // Get hardware status
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    printf("  Hardware Status:\n");
    printf("    Achieved tier: %s\n", tier_to_string(status.achieved_tier));
    printf("    Huge pages available: %s\n", status.huge_pages_available ? "Yes" : "No");
    printf("    NUMA topology detected: %s\n", status.numa_topology_detected ? "Yes" : "No");
    printf("    NUMA node count: %u\n", status.numa_node_count);
    printf("    GPU acceleration available: %s\n", status.gpu_acceleration_available ? "Yes" : "No");
    printf("    GPU vendor: %s\n", status.gpu_vendor ? status.gpu_vendor : "Unknown");
    printf("    CPU features: %s\n", status.cpu_features ? status.cpu_features : "Unknown");
    printf("    Missing capabilities: 0x%08X\n", status.missing_capabilities);
    printf("    Degradation reason: %s\n", status.degradation_reason ? status.degradation_reason : "None");
    printf("    Performance impact: %s\n", status.performance_impact_estimate ? status.performance_impact_estimate : "None");
    printf("    Remediation steps: %s\n", status.remediation_steps ? status.remediation_steps : "None");
    
    printf("  ✓ Hardware detection completed\n\n");
}

static void test_capability_queries(void) {
    printf("Testing capability queries...\n");
    
    // Test individual capability checks
    bool has_huge_pages = lgx_runtime_has_capability(LGX_CAP_HUGE_PAGES);
    bool has_numa = lgx_runtime_has_capability(LGX_CAP_NUMA_AWARENESS);
    bool has_gpu = lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION);
    bool has_fast_alloc = lgx_runtime_has_capability(LGX_CAP_FAST_ALLOCATOR);
    bool has_telemetry = lgx_runtime_has_capability(LGX_CAP_TELEMETRY);
    
    printf("  Individual capability checks:\n");
    printf("    Huge pages: %s\n", has_huge_pages ? "Available" : "Not available");
    printf("    NUMA awareness: %s\n", has_numa ? "Available" : "Not available");
    printf("    GPU acceleration: %s\n", has_gpu ? "Available" : "Not available");
    printf("    Fast allocator: %s\n", has_fast_alloc ? "Available" : "Not available");
    printf("    Telemetry: %s\n", has_telemetry ? "Available" : "Not available");
    
    // Test bulk capability query
    uint32_t all_capabilities = 0;
    lgx_result_t result = lgx_runtime_query_capabilities(&all_capabilities);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    printf("  Bulk capability query: 0x%08X\n", all_capabilities);
    
    // Verify consistency
    assert(has_huge_pages == ((all_capabilities & LGX_CAP_HUGE_PAGES) != 0));
    assert(has_numa == ((all_capabilities & LGX_CAP_NUMA_AWARENESS) != 0));
    assert(has_gpu == ((all_capabilities & LGX_CAP_GPU_ACCELERATION) != 0));
    assert(has_fast_alloc == ((all_capabilities & LGX_CAP_FAST_ALLOCATOR) != 0));
    assert(has_telemetry == ((all_capabilities & LGX_CAP_TELEMETRY) != 0));
    
    printf("  ✓ Capability queries consistent\n\n");
}

static void test_health_check_api(void) {
    printf("Testing health check API...\n");
    
    lgx_hardware_status_t health_status;
    lgx_result_t result = lgx_runtime_health_check(&health_status);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    printf("  Health check results:\n");
    printf("    Overall tier: %s\n", tier_to_string(health_status.achieved_tier));
    printf("    System health: %s\n", 
           (health_status.achieved_tier >= LGX_HW_TIER_COMPATIBLE) ? "Good" : "Degraded");
    
    // Verify health status matches hardware status
    lgx_hardware_status_t hw_status = lgx_runtime_get_hardware_status();
    (void)hw_status;  // Suppress unused warning
    assert(health_status.achieved_tier == hw_status.achieved_tier);
    assert(health_status.missing_capabilities == hw_status.missing_capabilities);
    
    printf("  ✓ Health check API working correctly\n\n");
}

static void test_graceful_degradation_scenarios(void) {
    printf("Testing graceful degradation scenarios...\n");
    
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    // Test different degradation scenarios
    switch (status.achieved_tier) {
        case LGX_HW_TIER_OPTIMAL:
            printf("  Scenario: OPTIMAL hardware configuration\n");
            printf("    All features available - no degradation needed\n");
            printf("    Expected performance: 100%% baseline\n");
            break;
            
        case LGX_HW_TIER_COMPATIBLE:
            printf("  Scenario: COMPATIBLE hardware configuration\n");
            printf("    Some features missing but core functionality available\n");
            printf("    Performance impact: %s\n", 
                   status.performance_impact_estimate ? status.performance_impact_estimate : "Unknown");
            printf("    Recommended action: %s\n", 
                   status.remediation_steps ? status.remediation_steps : "None");
            
            // Test that we can still allocate memory
            void* test_ptr = lgx_alloc(1024);
            assert(test_ptr != NULL);
            lgx_free(test_ptr);
            printf("    ✓ Memory allocation still works in compatible mode\n");
            break;
            
        case LGX_HW_TIER_DEGRADED:
            printf("  Scenario: DEGRADED hardware configuration\n");
            printf("    Multiple features missing - significant performance impact\n");
            printf("    Performance impact: %s\n", 
                   status.performance_impact_estimate ? status.performance_impact_estimate : "Unknown");
            printf("    Recommended action: %s\n", 
                   status.remediation_steps ? status.remediation_steps : "None");
            
            // Test that basic functionality still works
            void* test_ptr2 = lgx_alloc(512);
            assert(test_ptr2 != NULL);
            lgx_free(test_ptr2);
            printf("    ✓ Basic functionality still works in degraded mode\n");
            break;
    }
    
    printf("  ✓ Graceful degradation scenarios tested\n\n");
}

static void test_hardware_adaptation_recommendations(void) {
    printf("Testing hardware adaptation recommendations...\n");
    
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    
    // Analyze missing capabilities and provide specific recommendations
    if (status.missing_capabilities & LGX_CAP_HUGE_PAGES) {
        printf("  Missing: Huge Pages\n");
        printf("    Impact: Increased TLB pressure, 5-10%% performance penalty\n");
        printf("    Recommendation: %s\n", status.remediation_steps);
        printf("    Command: echo 128 > /proc/sys/vm/nr_hugepages (as root)\n");
    }
    
    if (status.missing_capabilities & LGX_CAP_GPU_ACCELERATION) {
        printf("  Missing: GPU Acceleration\n");
        printf("    Impact: Software fallback for GPU operations\n");
        printf("    Recommendation: Install appropriate GPU drivers\n");
        printf("    NVIDIA: Install nvidia-driver package\n");
        printf("    AMD: Install mesa-vulkan-drivers package\n");
        printf("    Intel: Install intel-media-va-driver package\n");
    }
    
    if (status.missing_capabilities & LGX_CAP_NUMA_AWARENESS) {
        printf("  Missing: NUMA Awareness\n");
        printf("    Impact: Suboptimal memory placement on multi-socket systems\n");
        printf("    Recommendation: This is typically a hardware limitation\n");
        printf("    Note: Single-socket systems don't need NUMA awareness\n");
    }
    
    if (status.missing_capabilities == 0) {
        printf("  All capabilities available - no recommendations needed\n");
    }
    
    printf("  ✓ Hardware adaptation recommendations provided\n\n");
}

static void test_error_conditions(void) {
    printf("Testing error conditions...\n");
    
    // Test invalid parameters
    lgx_result_t result = lgx_runtime_query_capabilities(NULL);
    assert(result == LGX_ERROR_INVALID_PARAM);
    (void)result;  // Suppress unused warning
    printf("  ✓ NULL parameter correctly rejected\n");
    
    result = lgx_runtime_health_check(NULL);
    assert(result == LGX_ERROR_INVALID_PARAM);
    (void)result;  // Suppress unused warning
    printf("  ✓ NULL health check parameter correctly rejected\n");
    
    // Test capability check with invalid capability (should return false)
    bool has_invalid = lgx_runtime_has_capability((lgx_capability_t)0x80000000);
    assert(has_invalid == false);
    (void)has_invalid;  // Suppress unused warning
    printf("  ✓ Invalid capability correctly returns false\n");
    
    printf("  ✓ Error conditions handled correctly\n\n");
}

static void print_system_summary(void) {
    printf("=== System Hardware Summary ===\n");
    
    lgx_hardware_status_t status = lgx_runtime_get_hardware_status();
    uint32_t capabilities = 0;
    lgx_runtime_query_capabilities(&capabilities);
    
    printf("Hardware Tier: %s\n", tier_to_string(status.achieved_tier));
    printf("Available Capabilities:\n");
    
    if (capabilities & LGX_CAP_HUGE_PAGES) printf("  ✓ Huge Pages\n");
    else printf("  ✗ Huge Pages\n");
    
    if (capabilities & LGX_CAP_NUMA_AWARENESS) printf("  ✓ NUMA Awareness\n");
    else printf("  ✗ NUMA Awareness\n");
    
    if (capabilities & LGX_CAP_GPU_ACCELERATION) printf("  ✓ GPU Acceleration\n");
    else printf("  ✗ GPU Acceleration\n");
    
    if (capabilities & LGX_CAP_FAST_ALLOCATOR) printf("  ✓ Fast Allocator\n");
    else printf("  ✗ Fast Allocator\n");
    
    if (capabilities & LGX_CAP_TELEMETRY) printf("  ✓ Telemetry\n");
    else printf("  ✗ Telemetry\n");
    
    if (status.achieved_tier != LGX_HW_TIER_OPTIMAL) {
        printf("\nPerformance Impact: %s\n", 
               status.performance_impact_estimate ? status.performance_impact_estimate : "Unknown");
        printf("Remediation: %s\n", 
               status.remediation_steps ? status.remediation_steps : "None available");
    }
    
    printf("\n");
}

int main(void) {
    printf("=== LGX Runtime Hardware Adaptation Framework Test ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    printf("✓ Runtime initialized successfully\n\n");
    
    // Run hardware adaptation tests
    test_hardware_detection();
    test_capability_queries();
    test_health_check_api();
    test_graceful_degradation_scenarios();
    test_hardware_adaptation_recommendations();
    test_error_conditions();
    
    // Print system summary
    print_system_summary();
    
    // Cleanup
    lgx_config_destroy(config);
    result = lgx_runtime_shutdown();
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    printf("=== All Hardware Adaptation Tests Passed! ===\n");
    return 0;
}