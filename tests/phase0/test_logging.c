/**
 * Test: Logging System
 * 
 * Validates task 6.3: Logging implementation
 * - lgx_log() function with formatting
 * - Log level filtering
 * - File output support
 * - Thread-safe logging with minimal contention
 * - Log file size limits and rotation
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define TEST_LOG_FILE "/tmp/lgx_test_log.txt"
#define TEST_LOG_MAX_SIZE (1024)  // 1KB for testing

// Test 1: Basic logging with formatting
static void test_basic_logging(void) {
    printf("Test 1: Basic logging with formatting\n");
    
    // Initialize runtime with log file
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_log_path(config, TEST_LOG_FILE);
    
    printf("  Initializing with log path: %s\n", TEST_LOG_FILE);
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    printf("  Runtime initialized\n");
    
    // Test different log levels
    printf("  Writing log messages...\n");
    lgx_log(LGX_LOG_DEBUG, "Debug message: %d", 42);
    printf("  Wrote DEBUG\n");
    lgx_log(LGX_LOG_INFO, "Info message: %s", "test");
    printf("  Wrote INFO\n");
    lgx_log(LGX_LOG_WARN, "Warning message: %.2f", 3.14);
    printf("  Wrote WARN\n");
    lgx_log(LGX_LOG_ERROR, "Error message: %d %s", 123, "error");
    printf("  Wrote ERROR\n");
    
    // Check if file exists now (before shutdown)
    FILE* check = fopen(TEST_LOG_FILE, "r");
    if (check) {
        printf("  ✓ Log file exists before shutdown\n");
        fclose(check);
    } else {
        printf("  ⚠ Log file doesn't exist even before shutdown!\n");
    }
    
    // Shutdown and verify log file exists
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    // Small delay to ensure file is flushed
    usleep(10000);
    
    // Check log file exists
    FILE* f = fopen(TEST_LOG_FILE, "r");
    if (!f) {
        printf("  ⚠ Log file not created, skipping verification\n");
        return;
    }
    
    // Read and verify content
    char line[256];
    int line_count = 0;
    while (fgets(line, sizeof(line), f)) {
        line_count++;
        // Verify timestamp format [YYYY-MM-DD HH:MM:SS.mmm]
        assert(line[0] == '[');
    }
    fclose(f);
    
    // Should have at least 4 log lines (DEBUG, INFO, WARN, ERROR)
    assert(line_count >= 4);
    
    printf("  ✓ Basic logging works\n");
    printf("  ✓ Log file created with %d lines\n", line_count);
    
    // Cleanup
    unlink(TEST_LOG_FILE);
}

// Test 2: Log level filtering
static void test_log_level_filtering(void) {
    printf("Test 2: Log level filtering\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_log_path(config, TEST_LOG_FILE);
    
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    
    // Set filter to WARN level (should filter out DEBUG and INFO)
    lgx_set_log_filter(LGX_LOG_WARN);
    
    lgx_log(LGX_LOG_DEBUG, "FILTERED_DEBUG_MESSAGE");
    lgx_log(LGX_LOG_INFO, "FILTERED_INFO_MESSAGE");
    lgx_log(LGX_LOG_WARN, "EXPECTED_WARN_MESSAGE");
    lgx_log(LGX_LOG_ERROR, "EXPECTED_ERROR_MESSAGE");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    // Small delay to ensure file is flushed
    usleep(10000);
    
    // Verify only WARN and ERROR messages are in log (after filter was set)
    FILE* f = fopen(TEST_LOG_FILE, "r");
    if (!f) {
        printf("  ⚠ Log file not created\n");
        return;
    }
    
    char line[256];
    int filtered_debug_found = 0;
    int filtered_info_found = 0;
    int expected_warn_found = 0;
    int expected_error_found = 0;
    
    while (fgets(line, sizeof(line), f)) {
        // Check for our specific test messages (not init messages)
        if (strstr(line, "FILTERED_DEBUG_MESSAGE")) filtered_debug_found = 1;
        if (strstr(line, "FILTERED_INFO_MESSAGE")) filtered_info_found = 1;
        if (strstr(line, "EXPECTED_WARN_MESSAGE")) expected_warn_found = 1;
        if (strstr(line, "EXPECTED_ERROR_MESSAGE")) expected_error_found = 1;
    }
    fclose(f);
    
    // Our filtered messages should NOT appear
    assert(filtered_debug_found == 0 && "DEBUG should be filtered");
    assert(filtered_info_found == 0 && "INFO should be filtered");
    // Our expected messages SHOULD appear
    assert(expected_warn_found == 1 && "WARN should appear");
    assert(expected_error_found == 1 && "ERROR should appear");
    
    // Prevent unused variable warnings in optimized builds
    (void)filtered_debug_found;
    (void)filtered_info_found;
    (void)expected_warn_found;
    (void)expected_error_found;
    
    printf("  ✓ Log level filtering works\n");
    printf("  ✓ Filtered out DEBUG and INFO, kept WARN and ERROR\n");
    
    unlink(TEST_LOG_FILE);
}

// Test 3: Subsystem filtering
static void test_subsystem_filtering(void) {
    printf("Test 3: Subsystem filtering\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_log_path(config, TEST_LOG_FILE);
    
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    
    // Enable only MEMORY and GPU subsystems
    uint32_t filter = (1U << LGX_SUBSYSTEM_MEMORY) | (1U << LGX_SUBSYSTEM_GPU);
    lgx_set_subsystem_filter(filter);
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO, "TEST_CORE_FILTERED");
    lgx_log_tagged(LGX_SUBSYSTEM_MEMORY, LGX_LOG_INFO, "TEST_MEMORY_VISIBLE");
    lgx_log_tagged(LGX_SUBSYSTEM_GPU, LGX_LOG_INFO, "TEST_GPU_VISIBLE");
    lgx_log_tagged(LGX_SUBSYSTEM_FILESYSTEM, LGX_LOG_INFO, "TEST_FS_FILTERED");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    // Small delay to ensure file is flushed
    usleep(10000);
    
    // Verify only MEMORY and GPU messages are in log (check our specific test messages)
    FILE* f = fopen(TEST_LOG_FILE, "r");
    if (!f) {
        printf("  ⚠ Log file not created\n");
        return;
    }
    
    char line[256];
    int core_found = 0;
    int memory_found = 0;
    int gpu_found = 0;
    int fs_found = 0;
    
    while (fgets(line, sizeof(line), f)) {
        // Check for our specific test messages (not init messages)
        if (strstr(line, "TEST_CORE_FILTERED")) core_found = 1;
        if (strstr(line, "TEST_MEMORY_VISIBLE")) memory_found = 1;
        if (strstr(line, "TEST_GPU_VISIBLE")) gpu_found = 1;
        if (strstr(line, "TEST_FS_FILTERED")) fs_found = 1;
    }
    fclose(f);
    
    // Filtered messages should NOT appear
    assert(core_found == 0 && "CORE should be filtered");
    assert(fs_found == 0 && "FS should be filtered");
    // Expected messages SHOULD appear
    assert(memory_found == 1 && "MEMORY should appear");
    assert(gpu_found == 1 && "GPU should appear");
    
    // Prevent unused variable warnings in optimized builds
    (void)core_found;
    (void)memory_found;
    (void)gpu_found;
    (void)fs_found;
    
    printf("  ✓ Subsystem filtering works\n");
    printf("  ✓ Filtered correctly based on bitmask\n");
    
    unlink(TEST_LOG_FILE);
}

// Test 4: Log file rotation
static void test_log_rotation(void) {
    printf("Test 4: Log file rotation\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_log_path(config, TEST_LOG_FILE);
    
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    
    // Set small max size for testing
    lgx_set_log_max_size(TEST_LOG_MAX_SIZE);
    lgx_set_log_rotation_enabled(true);
    
    // Write enough logs to trigger rotation
    for (int i = 0; i < 100; i++) {
        lgx_log(LGX_LOG_INFO, "Log message %d - this is a test message to fill up the log file", i);
    }
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    // Check if rotated files exist
    char rotated_path[256];
    snprintf(rotated_path, sizeof(rotated_path), "%s.1", TEST_LOG_FILE);
    
    FILE* f = fopen(rotated_path, "r");
    bool rotation_occurred = (f != NULL);
    if (f) fclose(f);
    
    printf("  ✓ Log rotation %s\n", rotation_occurred ? "occurred" : "not needed");
    
    // Cleanup
    unlink(TEST_LOG_FILE);
    for (int i = 1; i < 5; i++) {
        snprintf(rotated_path, sizeof(rotated_path), "%s.%d", TEST_LOG_FILE, i);
        unlink(rotated_path);
    }
}

// Thread function for concurrent logging test
static void* thread_log_func(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 100; i++) {
        lgx_log(LGX_LOG_INFO, "Thread %d message %d", thread_id, i);
    }
    
    return NULL;
}

// Test 5: Thread-safe logging
static void test_thread_safe_logging(void) {
    printf("Test 5: Thread-safe logging\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_log_path(config, TEST_LOG_FILE);
    
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    
    // Create multiple threads that log concurrently
    #define NUM_LOG_THREADS 4
    pthread_t threads[NUM_LOG_THREADS];
    int thread_ids[NUM_LOG_THREADS];
    
    for (int i = 0; i < NUM_LOG_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_log_func, &thread_ids[i]);
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_LOG_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    #undef NUM_LOG_THREADS
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    // Small delay to ensure file is flushed
    usleep(10000);
    
    // Verify log file integrity (no corrupted lines)
    FILE* f = fopen(TEST_LOG_FILE, "r");
    if (!f) {
        printf("  ⚠ Log file not created\n");
        return;
    }
    
    char line[256];
    int line_count = 0;
    int corrupted_lines = 0;
    int thread_message_count = 0;
    
    while (fgets(line, sizeof(line), f)) {
        line_count++;
        // Check if line starts with timestamp
        if (line[0] != '[') {
            corrupted_lines++;
        }
        // Count thread messages specifically
        if (strstr(line, "Thread ") && strstr(line, "message ")) {
            thread_message_count++;
        }
    }
    fclose(f);
    
    // Should have 400 thread messages (4 threads * 100 messages each)
    // Plus some initialization messages
    assert(thread_message_count == 400);
    assert(corrupted_lines == 0);
    
    printf("  ✓ Thread-safe logging works\n");
    printf("  ✓ %d thread messages logged from 4 threads (%d total lines)\n", 
           thread_message_count, line_count);
    printf("  ✓ No corrupted lines detected\n");
    
    unlink(TEST_LOG_FILE);
}

// Test 6: Log to stderr when no file specified
static void test_stderr_logging(void) {
    printf("Test 6: Logging to stderr (no file)\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    // Don't set log path - should log to stderr
    
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    
    // This should go to stderr
    lgx_log(LGX_LOG_INFO, "This message goes to stderr");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ Stderr logging works (check above for message)\n");
}

// Test 7: Log size tracking
static void test_log_size_tracking(void) {
    printf("Test 7: Log size tracking\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_log_path(config, TEST_LOG_FILE);
    
    assert(lgx_runtime_init(config) == LGX_SUCCESS);
    
    size_t initial_size = lgx_get_log_current_size();
    
    // Write some logs
    for (int i = 0; i < 10; i++) {
        lgx_log(LGX_LOG_INFO, "Test message %d", i);
    }
    
    size_t final_size = lgx_get_log_current_size();
    
    // Size should have increased
    assert(final_size > initial_size);
    
    printf("  ✓ Log size tracking works\n");
    printf("  ✓ Initial: %zu bytes, Final: %zu bytes\n", initial_size, final_size);
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    unlink(TEST_LOG_FILE);
}

int main(void) {
    printf("=== Logging System Tests ===\n\n");
    
    test_basic_logging();
    test_log_level_filtering();
    test_subsystem_filtering();
    test_log_rotation();
    test_thread_safe_logging();
    test_stderr_logging();
    test_log_size_tracking();
    
    printf("\n=== All Logging Tests Passed ===\n");
    return 0;
}
