/**
 * Test: Separate Telemetry Process
 * 
 * Validates task 7.1: Separate telemetry process implementation
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

int main(void) {
    printf("=== Separate Telemetry Process Tests ===\n\n");
    
    // Test 1: Initialize telemetry process
    printf("Test 1: Initialize telemetry process\n");
    lgx_telemetry_process_t* process = NULL;
    lgx_result_t result = lgx_telemetry_process_init(&process);
    (void)result;  // Suppress unused warning
    assert(result == LGX_SUCCESS);
    assert(process != NULL);
    printf("  ✓ Telemetry process initialized\n");
    
    // Give process time to start
    usleep(100000);  // 100ms
    
    // Test 2: Record frame times
    printf("Test 2: Record frame times\n");
    for (int i = 0; i < 100; i++) {
        float frame_time = 16.0f + (i % 10);  // 16-25ms
        result = lgx_telemetry_process_record_frame_time(process, frame_time);
        assert(result == LGX_SUCCESS);
    }
    printf("  ✓ Recorded 100 frame times\n");
    
    // Test 3: Record memory usage
    printf("Test 3: Record memory usage\n");
    for (int i = 0; i < 50; i++) {
        size_t memory_mb = 100 + i;
        result = lgx_telemetry_process_record_memory(process, memory_mb);
        assert(result == LGX_SUCCESS);
    }
    printf("  ✓ Recorded 50 memory samples\n");
    
    // Test 4: Record allocations
    printf("Test 4: Record allocations\n");
    for (int i = 0; i < 200; i++) {
        size_t size = 64 + (i * 16);
        result = lgx_telemetry_process_record_allocation(process, size);
        assert(result == LGX_SUCCESS);
    }
    printf("  ✓ Recorded 200 allocations\n");
    
    // Test 5: Check dropped events
    printf("Test 5: Check dropped events\n");
    uint64_t dropped = lgx_telemetry_process_get_dropped_events(process);
    printf("  ✓ Dropped events: %lu (expected 0 for this test)\n", dropped);
    assert(dropped == 0);
    
    // Test 6: Stress test - fill ring buffer
    printf("Test 6: Stress test - fill ring buffer\n");
    for (int i = 0; i < 10000; i++) {
        lgx_telemetry_process_record_frame_time(process, 16.0f);
    }
    dropped = lgx_telemetry_process_get_dropped_events(process);
    printf("  ✓ Stress test complete, dropped events: %lu\n", dropped);
    
    // Give process time to consume events
    usleep(500000);  // 500ms
    
    // Test 7: Shutdown telemetry process
    printf("Test 7: Shutdown telemetry process\n");
    result = lgx_telemetry_process_shutdown(process);
    assert(result == LGX_SUCCESS);
    printf("  ✓ Telemetry process shutdown complete\n");
    
    printf("\n=== All Telemetry Process Tests Passed ===\n");
    return 0;
}
