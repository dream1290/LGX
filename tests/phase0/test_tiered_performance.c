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
    
    // Do some allocations to generate performance data
    const int num_allocs = 100;
    void* ptrs[num_allocs];
    for (int i = 0; i < num_allocs; i++) {
        ptrs[i] = lgx_alloc(1024);
        assert(ptrs[i] != NULL);
    }
    
    // Test performance targets API
    printf("Performance Targets:\n");
    lgx_performance_targets_t targets = lgx_runtime_get_performance_targets();
    
    printf("Initialization Time Targets:\n");
    printf("  Tier 1 (MVP): %.2f ms\n", targets.init_time_tier1_ns / 1000000.0);
    printf("  Tier 2 (Competitive): %.2f ms\n", targets.init_time_tier2_ns / 1000000.0);
    printf("  Tier 3 (Best-in-class): %.2f ms\n", targets.init_time_tier3_ns / 1000000.0);
    
    printf("\nMemory Usage Targets:\n");
    printf("  Tier 1 (MVP): %.2f MB\n", targets.memory_tier1_bytes / (1024.0 * 1024.0));
    printf("  Tier 2 (Competitive): %.2f MB\n", targets.memory_tier2_bytes / (1024.0 * 1024.0));
    printf("  Tier 3 (Best-in-class): %.2f MB\n", targets.memory_tier3_bytes / (1024.0 * 1024.0));
    
    printf("\nAllocation Latency Targets:\n");
    printf("  Tier 1 (MVP): %.2f μs\n", targets.alloc_latency_tier1_ns / 1000.0);
    printf("  Tier 2 (Competitive): %.2f μs\n", targets.alloc_latency_tier2_ns / 1000.0);
    printf("  Tier 3 (Best-in-class): %.2f μs\n", targets.alloc_latency_tier3_ns / 1000.0);
    
    // Test performance assessment API
    printf("\n" "Performance Assessment:\n");
    lgx_performance_assessment_t assessment;
    result = lgx_runtime_assess_performance(&assessment);
    assert(result == LGX_SUCCESS);
    
    printf("Measured Performance:\n");
    printf("  Initialization Time: %.2f ms\n", assessment.measured_init_time_ns / 1000000.0);
    printf("  Memory Usage: %.2f MB\n", assessment.measured_memory_bytes / (1024.0 * 1024.0));
    printf("  Allocation Latency: %.2f μs\n", assessment.measured_alloc_latency_ns / 1000.0);
    
    printf("\nTier Assessment:\n");
    printf("  Initialization Time: %s (%.1f%% margin)\n", 
           lgx_performance_tier_to_string(assessment.init_time_tier),
           assessment.init_time_margin_percent);
    printf("  Memory Usage: %s (%.1f%% margin)\n", 
           lgx_performance_tier_to_string(assessment.memory_usage_tier),
           assessment.memory_margin_percent);
    printf("  Allocation Latency: %s (%.1f%% margin)\n", 
           lgx_performance_tier_to_string(assessment.alloc_latency_tier),
           assessment.alloc_latency_margin_percent);
    
    printf("\nOverall Assessment:\n");
    printf("  Overall Tier: %s\n", lgx_performance_tier_to_string(assessment.overall_tier));
    printf("  Bottleneck Component: %s\n", assessment.bottleneck_component);
    printf("  Improvement Suggestion: %s\n", assessment.improvement_suggestion);
    
    // Validate tier logic
    printf("\nTier Logic Validation:\n");
    
    // Check that overall tier is minimum of individual tiers
    lgx_performance_tier_t expected_overall = assessment.init_time_tier;
    if (assessment.memory_usage_tier < expected_overall) {
        expected_overall = assessment.memory_usage_tier;
    }
    if (assessment.alloc_latency_tier < expected_overall) {
        expected_overall = assessment.alloc_latency_tier;
    }
    
    if (assessment.overall_tier == expected_overall) {
        printf("✅ Overall tier correctly calculated as minimum of individual tiers\n");
    } else {
        printf("❌ Overall tier calculation error\n");
        return 1;
    }
    
    // Check margin calculations make sense
    bool margins_valid = true;
    if (assessment.init_time_tier == LGX_PERF_TIER_3 && assessment.init_time_margin_percent <= 0) {
        printf("❌ Tier 3 init time should have positive margin\n");
        margins_valid = false;
    }
    if (assessment.memory_usage_tier == LGX_PERF_TIER_3 && assessment.memory_margin_percent <= 0) {
        printf("❌ Tier 3 memory usage should have positive margin\n");
        margins_valid = false;
    }
    if (assessment.alloc_latency_tier == LGX_PERF_TIER_3 && assessment.alloc_latency_margin_percent <= 0) {
        printf("❌ Tier 3 allocation latency should have positive margin\n");
        margins_valid = false;
    }
    
    if (margins_valid) {
        printf("✅ Performance margins correctly calculated\n");
    }
    
    // Test decision criteria
    printf("\nDecision Criteria Test:\n");
    if (assessment.overall_tier == LGX_PERF_TIER_3) {
        printf("🎯 RECOMMENDATION: Ship as best-in-class product\n");
        printf("   All performance metrics exceed aspirational targets\n");
    } else if (assessment.overall_tier == LGX_PERF_TIER_2) {
        printf("🎯 RECOMMENDATION: Ship as competitive product\n");
        printf("   Performance meets market expectations\n");
        printf("   Consider optimizing: %s\n", assessment.bottleneck_component);
    } else {
        printf("🎯 RECOMMENDATION: Ship as MVP, continue optimization\n");
        printf("   Focus on: %s\n", assessment.bottleneck_component);
        printf("   Suggestion: %s\n", assessment.improvement_suggestion);
    }
    
    // Clean up
    for (int i = 0; i < num_allocs; i++) {
        lgx_free(ptrs[i]);
    }
    
    result = lgx_runtime_shutdown();
    assert(result == LGX_SUCCESS);
    
    lgx_config_destroy(config);
    
    printf("\n✅ Tiered performance framework test completed successfully!\n");
    return 0;
}