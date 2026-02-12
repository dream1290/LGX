#include "../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
    printf("LGX Runtime Tiered Performance Framework Test\n");
    printf("==============================================\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Do some allocations to generate performance data
    #define NUM_TIERED_ALLOCS 100
    void* ptrs[NUM_TIERED_ALLOCS];
    for (int i = 0; i < NUM_TIERED_ALLOCS; i++) {
        ptrs[i] = lgx_alloc(1024);
        assert(ptrs[i] != NULL);
    }
    
    // Test performance targets API
    printf("Performance Targets:\n");
    lgx_performance_targets_t targets = lgx_runtime_get_performance_targets();
    
    printf("Initialization Time Targets:\n");
    printf("  Tier 1 (MVP): %u ms\n", targets.tier1_init_time_ms);
    printf("  Tier 2 (Competitive): %u ms\n", targets.tier2_init_time_ms);
    printf("  Tier 3 (Best-in-class): %u ms\n", targets.tier3_init_time_ms);
    
    printf("\nMemory Usage Targets:\n");
    printf("  Tier 1 (MVP): %zu MB\n", targets.tier1_memory_overhead_mb);
    printf("  Tier 2 (Competitive): %zu MB\n", targets.tier2_memory_overhead_mb);
    printf("  Tier 3 (Best-in-class): %zu MB\n", targets.tier3_memory_overhead_mb);
    
    printf("\nAllocation Latency Targets:\n");
    printf("  Tier 1 (MVP): %.2f μs\n", targets.tier1_alloc_latency_ns / 1000.0);
    printf("  Tier 2 (Competitive): %.2f μs\n", targets.tier2_alloc_latency_ns / 1000.0);
    printf("  Tier 3 (Best-in-class): %.2f μs\n", targets.tier3_alloc_latency_ns / 1000.0);
    
    // Test performance assessment API
    printf("\nPerformance Assessment:\n");
    lgx_performance_assessment_t assessment;
    result = lgx_runtime_assess_performance(&assessment);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    printf("Measured Performance:\n");
    printf("  Initialization Time: %u ms\n", assessment.measured_init_time_ms);
    printf("  Memory Usage: %zu MB\n", assessment.measured_memory_overhead_mb);
    printf("  Allocation Latency: %.2f μs\n", assessment.measured_alloc_latency_ns / 1000.0);
    
    printf("\nTier Assessment:\n");
    printf("  Achieved Tier: %s\n", lgx_performance_tier_to_string(assessment.achieved_tier));
    printf("  Meets Tier 1: %s\n", assessment.meets_tier1 ? "Yes" : "No");
    printf("  Meets Tier 2: %s\n", assessment.meets_tier2 ? "Yes" : "No");
    printf("  Meets Tier 3: %s\n", assessment.meets_tier3 ? "Yes" : "No");
    
    printf("\nBottleneck Analysis:\n");
    printf("  Bottleneck: %s\n", assessment.bottleneck_description ? assessment.bottleneck_description : "None");
    printf("  Improvement Suggestions: %s\n", assessment.improvement_suggestions ? assessment.improvement_suggestions : "None");
    
    printf("\n✅ All tiered performance tests passed!\n");
    
    // Cleanup
    for (int i = 0; i < NUM_TIERED_ALLOCS; i++) {
        lgx_free(ptrs[i]);
    }
    #undef NUM_TIERED_ALLOCS
    
    lgx_config_destroy(config);
    lgx_runtime_shutdown();
    
    return 0;
}
