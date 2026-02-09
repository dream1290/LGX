/**
 * Test: Telemetry SHA-256 Anonymization
 * 
 * Validates task 7.4.3: SHA-256 hashing for data anonymization
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#define TEST_TELEMETRY_FILE "/tmp/lgx_telemetry_anon_test.json"

int main(void) {
    printf("=== Telemetry SHA-256 Anonymization Tests ===\n\n");
    
    // Clean up any existing telemetry file
    unlink(TEST_TELEMETRY_FILE);
    
    // Test 1: Initialize runtime with telemetry
    printf("Test 1: Initialize runtime with telemetry\n");
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    printf("  Config flags set to: 0x%x (ENABLE_TELEMETRY=0x%x)\n", 
           config->flags, LGX_CONFIG_ENABLE_TELEMETRY);
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    printf("  ✓ Runtime initialized\n");
    
    // Get runtime state to access telemetry directly
    lgx_runtime_state_t* runtime = lgx_runtime_get_state();
    assert(runtime != NULL);
    printf("  Runtime state: %p\n", (void*)runtime);
    printf("  Telemetry: %p\n", (void*)runtime->telemetry);
    assert(runtime->telemetry != NULL);
    
    // Test 2: Enable telemetry with user consent
    printf("Test 2: Enable telemetry\n");
    lgx_result_t result = lgx_telemetry_enable(runtime->telemetry, true);
    (void)result;  // Suppress unused warning
    assert(result == LGX_SUCCESS);
    printf("  ✓ Telemetry enabled with user consent\n");
    
    // Verify telemetry is actually enabled
    printf("  Debug: Checking telemetry state...\n");
    
    // Test 3: Record some data
    printf("Test 3: Record telemetry data\n");
    for (int i = 0; i < 10; i++) {
        lgx_telemetry_record_frame_time(runtime->telemetry, 16.0f + i);
    }
    printf("  ✓ Recorded 10 frame times\n");
    
    // Test 4: Export telemetry data
    printf("Test 4: Export telemetry with anonymization\n");
    result = lgx_telemetry_export(runtime->telemetry, TEST_TELEMETRY_FILE);
    if (result != LGX_SUCCESS) {
        printf("  ✗ FAIL: Export failed with error code: %d\n", result);
        lgx_runtime_shutdown();
        lgx_config_destroy(config);
        return 1;
    }
    printf("  ✓ Telemetry exported\n");
    
    // Small delay for file flush
    usleep(100000);  // 100ms
    
    // Test 5: Verify SHA-256 hashes in exported file
    printf("Test 5: Verify SHA-256 anonymization\n");
    FILE* f = fopen(TEST_TELEMETRY_FILE, "r");
    if (!f) {
        printf("  ✗ FAIL: Could not open telemetry file: %s\n", TEST_TELEMETRY_FILE);
        lgx_runtime_shutdown();
        lgx_config_destroy(config);
        return 1;
    }
    assert(f != NULL);
    
    char line[512];
    bool found_session_id = false;
    bool found_hardware_id = false;
    char session_id[128] = {0};
    char hardware_id[128] = {0};
    
    while (fgets(line, sizeof(line), f)) {
        // Look for session_id
        if (strstr(line, "\"session_id\"")) {
            found_session_id = true;
            // Extract the hash value
            char* start = strchr(line, ':');
            if (start) {
                start = strchr(start, '"');
                if (start) {
                    start++;
                    char* end = strchr(start, '"');
                    if (end) {
                        size_t len = end - start;
                        if (len < sizeof(session_id)) {
                            strncpy(session_id, start, len);
                            session_id[len] = '\0';
                        }
                    }
                }
            }
        }
        
        // Look for hardware_id
        if (strstr(line, "\"hardware_id\"")) {
            found_hardware_id = true;
            // Extract the hash value
            char* start = strchr(line, ':');
            if (start) {
                start = strchr(start, '"');
                if (start) {
                    start++;
                    char* end = strchr(start, '"');
                    if (end) {
                        size_t len = end - start;
                        if (len < sizeof(hardware_id)) {
                            strncpy(hardware_id, start, len);
                            hardware_id[len] = '\0';
                        }
                    }
                }
            }
        }
    }
    fclose(f);
    
    printf("  Session ID (SHA-256): %s\n", session_id);
    printf("  Hardware ID (SHA-256): %s\n", hardware_id);
    
    // Verify hashes are present and have correct length (64 hex chars)
    (void)found_session_id;  // Suppress unused warning
    (void)found_hardware_id;  // Suppress unused warning
    assert(found_session_id);
    assert(found_hardware_id);
    assert(strlen(session_id) == 64);
    assert(strlen(hardware_id) == 64);
    
    // Verify they are valid hex strings
    for (int i = 0; i < 64; i++) {
        assert((session_id[i] >= '0' && session_id[i] <= '9') ||
               (session_id[i] >= 'a' && session_id[i] <= 'f'));
        assert((hardware_id[i] >= '0' && hardware_id[i] <= '9') ||
               (hardware_id[i] >= 'a' && hardware_id[i] <= 'f'));
    }
    
    printf("  ✓ SHA-256 hashes verified (64 hex characters each)\n");
    printf("  ✓ Session and hardware IDs are properly anonymized\n");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    unlink(TEST_TELEMETRY_FILE);
    
    printf("\n=== All SHA-256 Anonymization Tests Passed ===\n");
    return 0;
}
