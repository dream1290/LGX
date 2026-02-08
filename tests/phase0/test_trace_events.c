/**
 * Test: Trace Event System
 * 
 * Tests the trace event system for performance profiling.
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

void test_trace_basic(void) {
    printf("\n=== Test: Basic Trace Events ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Enable tracing
    result = lgx_trace_enable(true);
    assert(result == LGX_SUCCESS);
    assert(lgx_trace_is_enabled() == true);
    
    // Record some trace events
    lgx_trace_begin("test_operation", 1234);
    usleep(1000);  // 1ms
    lgx_trace_end("test_operation");
    
    lgx_trace_instant("test_instant", 5678);
    
    // Get statistics
    lgx_trace_stats_t stats;
    result = lgx_trace_get_stats(&stats);
    assert(result == LGX_SUCCESS);
    
    printf("  Total events: %lu\n", stats.total_events);
    printf("  Begin events: %lu\n", stats.begin_events);
    printf("  End events: %lu\n", stats.end_events);
    printf("  Instant events: %lu\n", stats.instant_events);
    printf("  Buffer used: %zu / %zu\n", stats.buffer_used, stats.buffer_capacity);
    
    assert(stats.total_events >= 3);
    assert(stats.begin_events >= 1);
    assert(stats.end_events >= 1);
    assert(stats.instant_events >= 1);
    
    // Disable tracing
    result = lgx_trace_enable(false);
    assert(result == LGX_SUCCESS);
    assert(lgx_trace_is_enabled() == false);
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Basic trace events test passed\n");
}

void test_trace_export(void) {
    printf("\n=== Test: Trace Export to JSON ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Enable tracing
    result = lgx_trace_enable(true);
    assert(result == LGX_SUCCESS);
    
    // Record multiple trace events
    for (int i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof(name), "operation_%d", i);
        lgx_trace_begin(name, i * 100);
        usleep(100);  // 0.1ms
        lgx_trace_end(name);
    }
    
    // Export to JSON
    const char* output_path = "/tmp/lgx_trace_test.json";
    result = lgx_trace_export(output_path);
    assert(result == LGX_SUCCESS);
    
    printf("  ✓ Trace exported to %s\n", output_path);
    
    // Verify file exists
    FILE* file = fopen(output_path, "r");
    assert(file != NULL);
    
    // Read first few lines to verify JSON format
    char line[256];
    if (fgets(line, sizeof(line), file)) {
        printf("  First line: %s", line);
        assert(strstr(line, "{") != NULL);  // Should start with {
    }
    
    fclose(file);
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Trace export test passed\n");
}

void test_trace_buffer_overflow(void) {
    printf("\n=== Test: Trace Buffer Overflow ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Enable tracing
    result = lgx_trace_enable(true);
    assert(result == LGX_SUCCESS);
    
    // Get initial stats
    lgx_trace_stats_t stats_before;
    result = lgx_trace_get_stats(&stats_before);
    assert(result == LGX_SUCCESS);
    
    printf("  Buffer capacity: %zu events\n", stats_before.buffer_capacity);
    
    // Try to overflow the buffer (generate more events than capacity)
    size_t num_events = stats_before.buffer_capacity + 1000;
    printf("  Generating %zu events...\n", num_events);
    
    for (size_t i = 0; i < num_events; i++) {
        lgx_trace_instant("overflow_test", i);
    }
    
    // Get final stats
    lgx_trace_stats_t stats_after;
    result = lgx_trace_get_stats(&stats_after);
    assert(result == LGX_SUCCESS);
    
    printf("  Total events: %lu\n", stats_after.total_events);
    printf("  Dropped events: %lu\n", stats_after.dropped_events);
    printf("  Buffer used: %zu / %zu\n", stats_after.buffer_used, stats_after.buffer_capacity);
    
    // Should have dropped some events
    assert(stats_after.dropped_events > 0);
    printf("  ✓ Buffer overflow handled correctly (%lu events dropped)\n", 
           stats_after.dropped_events);
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Trace buffer overflow test passed\n");
}

void test_trace_clear(void) {
    printf("\n=== Test: Trace Clear ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Enable tracing
    result = lgx_trace_enable(true);
    assert(result == LGX_SUCCESS);
    
    // Record some events
    for (int i = 0; i < 100; i++) {
        lgx_trace_instant("test_event", i);
    }
    
    // Get stats before clear
    lgx_trace_stats_t stats_before;
    result = lgx_trace_get_stats(&stats_before);
    assert(result == LGX_SUCCESS);
    assert(stats_before.total_events >= 100);
    
    printf("  Events before clear: %lu\n", stats_before.total_events);
    
    // Clear buffer
    result = lgx_trace_clear();
    assert(result == LGX_SUCCESS);
    
    // Get stats after clear
    lgx_trace_stats_t stats_after;
    result = lgx_trace_get_stats(&stats_after);
    assert(result == LGX_SUCCESS);
    
    printf("  Events after clear: %lu\n", stats_after.total_events);
    
    assert(stats_after.total_events == 0);
    assert(stats_after.buffer_used == 0);
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Trace clear test passed\n");
}

void test_trace_integration_hooks(void) {
    printf("\n=== Test: Integration Hooks ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Enable integration hooks
    result = lgx_trace_enable_perf_integration(true);
    assert(result == LGX_SUCCESS);
    printf("  ✓ Perf integration enabled\n");
    
    result = lgx_trace_enable_valgrind_integration(true);
    assert(result == LGX_SUCCESS);
    printf("  ✓ Valgrind integration enabled\n");
    
    result = lgx_trace_enable_tracy_integration(true);
    assert(result == LGX_SUCCESS);
    printf("  ✓ Tracy integration enabled\n");
    
    // Disable integration hooks
    result = lgx_trace_enable_perf_integration(false);
    assert(result == LGX_SUCCESS);
    
    result = lgx_trace_enable_valgrind_integration(false);
    assert(result == LGX_SUCCESS);
    
    result = lgx_trace_enable_tracy_integration(false);
    assert(result == LGX_SUCCESS);
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Integration hooks test passed\n");
}

int main(void) {
    printf("=================================================\n");
    printf("  LGX Trace Event System Tests\n");
    printf("=================================================\n");
    
    test_trace_basic();
    test_trace_export();
    test_trace_buffer_overflow();
    test_trace_clear();
    test_trace_integration_hooks();
    
    printf("\n=================================================\n");
    printf("  All trace event tests passed! ✓\n");
    printf("=================================================\n");
    
    return 0;
}
