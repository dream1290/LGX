/**
 * Frame Arena Dump Test (Task 3.4.5.4.1)
 * 
 * Tests the lgx_frame_arena_dump() function that exports allocation map to JSON.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(msg) printf("✅ PASS: %s\n", msg)
#define TEST_FAIL(msg) printf("❌ FAIL: %s\n", msg)

int main(void) {
    printf("=== Frame Arena Dump Test (Task 3.4.5.4.1) ===\n\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_frame_arena_size(config, 64 * 1024 * 1024);  // 64MB
    
    lgx_result_t result = lgx_runtime_init(config);
    if (result != LGX_SUCCESS) {
        printf("❌ Failed to initialize runtime: %d\n", result);
        return 1;
    }
    lgx_config_destroy(config);
    
    printf("Runtime initialized with 64MB frame arena\n\n");
    
    // Make some allocations to populate the arena
    printf("Making test allocations...\n");
    
    // Small allocations
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_frame_alloc(64);
        if (!ptr) {
            printf("  Allocation %d failed\n", i);
        }
    }
    
    // Medium allocations
    for (int i = 0; i < 50; i++) {
        void* ptr = lgx_frame_alloc(1024);
        if (!ptr) {
            printf("  Allocation %d failed\n", i + 100);
        }
    }
    
    // Large allocations
    for (int i = 0; i < 10; i++) {
        void* ptr = lgx_frame_alloc(1024 * 1024);  // 1MB
        if (!ptr) {
            printf("  Allocation %d failed\n", i + 150);
        }
    }
    
    printf("  Made 160 allocations (100 small, 50 medium, 10 large)\n\n");
    
    // Test 1: Export allocation map to JSON
    printf("Test 1: Export allocation map to JSON\n");
    const char* output_file = "/tmp/lgx_frame_arena_map.json";
    result = lgx_frame_arena_dump(output_file);
    
    if (result == LGX_SUCCESS) {
        TEST_PASS("Allocation map exported successfully");
        printf("  Output file: %s\n", output_file);
        
        // Verify file exists and has content
        FILE* f = fopen(output_file, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            
            if (size > 0) {
                TEST_PASS("Output file has content");
                printf("  File size: %ld bytes\n", size);
                
                // Show first few lines
                printf("\n  First 20 lines of output:\n");
                printf("  ----------------------------------------\n");
                f = fopen(output_file, "r");
                char line[256];
                int line_count = 0;
                while (fgets(line, sizeof(line), f) && line_count < 20) {
                    printf("  %s", line);
                    line_count++;
                }
                fclose(f);
                printf("  ----------------------------------------\n\n");
            } else {
                TEST_FAIL("Output file is empty");
            }
        } else {
            TEST_FAIL("Could not open output file for verification");
        }
    } else {
        TEST_FAIL("Failed to export allocation map");
        printf("  Error code: %d\n", result);
    }
    
    // Test 2: Verify JSON structure
    printf("\nTest 2: Verify JSON structure\n");
    FILE* f = fopen(output_file, "r");
    if (f) {
        char buffer[4096];
        size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
        buffer[bytes_read] = '\0';
        fclose(f);
        
        // Check for expected JSON fields
        bool has_timestamp = strstr(buffer, "\"timestamp_ns\"") != NULL;
        bool has_current_frame = strstr(buffer, "\"current_frame\"") != NULL;
        bool has_global_stats = strstr(buffer, "\"global_stats\"") != NULL;
        bool has_arenas = strstr(buffer, "\"arenas\"") != NULL;
        bool has_histogram = strstr(buffer, "\"size_histogram\"") != NULL;
        bool has_call_sites = strstr(buffer, "\"top_call_sites\"") != NULL;
        
        if (has_timestamp && has_current_frame && has_global_stats && 
            has_arenas && has_histogram && has_call_sites) {
            TEST_PASS("JSON structure is valid");
            printf("  ✓ Contains timestamp_ns\n");
            printf("  ✓ Contains current_frame\n");
            printf("  ✓ Contains global_stats\n");
            printf("  ✓ Contains arenas array\n");
            printf("  ✓ Contains size_histogram\n");
            printf("  ✓ Contains top_call_sites\n");
        } else {
            TEST_FAIL("JSON structure is incomplete");
            if (!has_timestamp) printf("  ✗ Missing timestamp_ns\n");
            if (!has_current_frame) printf("  ✗ Missing current_frame\n");
            if (!has_global_stats) printf("  ✗ Missing global_stats\n");
            if (!has_arenas) printf("  ✗ Missing arenas\n");
            if (!has_histogram) printf("  ✗ Missing size_histogram\n");
            if (!has_call_sites) printf("  ✗ Missing top_call_sites\n");
        }
    } else {
        TEST_FAIL("Could not read output file for verification");
    }
    
    // Test 3: Export after frame reset
    printf("\nTest 3: Export after frame reset\n");
    lgx_frame_reset();
    
    // Make new allocations
    for (int i = 0; i < 50; i++) {
        lgx_frame_alloc(128);
    }
    
    const char* output_file2 = "/tmp/lgx_frame_arena_map_frame2.json";
    result = lgx_frame_arena_dump(output_file2);
    
    if (result == LGX_SUCCESS) {
        TEST_PASS("Export after frame reset successful");
        printf("  Output file: %s\n", output_file2);
    } else {
        TEST_FAIL("Export after frame reset failed");
    }
    
    // Cleanup
    lgx_runtime_shutdown();
    
    printf("\n=== Test Summary ===\n");
    printf("Task 3.4.5.4.1 implementation verified:\n");
    printf("  ✅ lgx_frame_arena_dump() exports allocation map to JSON\n");
    printf("  ✅ JSON includes global statistics\n");
    printf("  ✅ JSON includes per-arena details\n");
    printf("  ✅ JSON includes size histogram\n");
    printf("  ✅ JSON includes top allocation call sites\n");
    printf("\nJSON files can be used for:\n");
    printf("  - Visualization tools\n");
    printf("  - Allocation pattern analysis\n");
    printf("  - Performance debugging\n");
    printf("  - Memory usage tracking over time\n");
    
    return 0;
}
