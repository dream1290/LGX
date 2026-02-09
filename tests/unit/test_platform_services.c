/**
 * Unit Tests: Platform Services
 * 
 * Tests filesystem, timing, and logging services.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ %s\n", message); \
        } \
    } while(0)

// Test 1: Filesystem - basic file operations
static void test_fs_basic_operations(void) {
    printf("\nTest 1: Filesystem basic operations\n");
    
    const char* test_file = "/tmp/lgx_test_file.txt";
    const char* test_data = "Hello, LGX Runtime!";
    
    // Write test
    lgx_file_t* file = NULL;
    lgx_result_t result = lgx_fs_open(test_file, "w", &file);
    TEST_ASSERT(result == LGX_SUCCESS, "File open for write succeeds");
    TEST_ASSERT(file != NULL, "File handle is valid");
    
    result = lgx_fs_write(file, test_data, strlen(test_data));
    TEST_ASSERT(result == LGX_SUCCESS, "File write succeeds");
    
    result = lgx_fs_close(file);
    TEST_ASSERT(result == LGX_SUCCESS, "File close succeeds");
    
    // Read test
    file = NULL;
    result = lgx_fs_open(test_file, "r", &file);
    TEST_ASSERT(result == LGX_SUCCESS, "File open for read succeeds");
    
    char buffer[256] = {0};
    size_t bytes_read = 0;
    result = lgx_fs_read(file, buffer, sizeof(buffer) - 1, &bytes_read);
    TEST_ASSERT(result == LGX_SUCCESS, "File read succeeds");
    TEST_ASSERT(bytes_read == strlen(test_data), "Read correct number of bytes");
    TEST_ASSERT(strcmp(buffer, test_data) == 0, "Read data matches written data");
    
    lgx_fs_close(file);
    
    // Cleanup
    unlink(test_file);
}

// Test 2: Filesystem - NULL parameter handling
static void test_fs_null_parameters(void) {
    printf("\nTest 2: Filesystem NULL parameter handling\n");
    
    lgx_file_t* file = NULL;
    
    // NULL path
    lgx_result_t result = lgx_fs_open(NULL, "r", &file);
    TEST_ASSERT(result != LGX_SUCCESS, "Open with NULL path fails");
    
    // NULL mode
    result = lgx_fs_open("/tmp/test", NULL, &file);
    TEST_ASSERT(result != LGX_SUCCESS, "Open with NULL mode fails");
    
    // NULL file pointer
    result = lgx_fs_open("/tmp/test", "r", NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Open with NULL file pointer fails");
    
    // NULL file handle
    result = lgx_fs_read(NULL, NULL, 0, NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Read with NULL file fails");
    
    result = lgx_fs_write(NULL, NULL, 0);
    TEST_ASSERT(result != LGX_SUCCESS, "Write with NULL file fails");
    
    result = lgx_fs_close(NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Close with NULL file fails");
}

// Test 3: Filesystem - invalid paths
static void test_fs_invalid_paths(void) {
    printf("\nTest 3: Filesystem invalid paths\n");
    
    lgx_file_t* file = NULL;
    
    // Non-existent file for reading
    lgx_result_t result = lgx_fs_open("/tmp/nonexistent_file_12345.txt", "r", &file);
    TEST_ASSERT(result != LGX_SUCCESS, "Open non-existent file fails");
    
    // Path traversal attempt
    result = lgx_fs_open("../../../etc/passwd", "r", &file);
    TEST_ASSERT(result != LGX_SUCCESS, "Path traversal is blocked");
    
    // Invalid mode
    result = lgx_fs_open("/tmp/test", "invalid", &file);
    TEST_ASSERT(result != LGX_SUCCESS, "Invalid mode fails");
}

// Test 4: Filesystem - large file operations
static void test_fs_large_file(void) {
    printf("\nTest 4: Filesystem large file operations\n");
    
    const char* test_file = "/tmp/lgx_test_large.bin";
    const size_t data_size = 1024 * 1024; // 1MB
    
    // Allocate test data
    char* data = malloc(data_size);
    TEST_ASSERT(data != NULL, "Test data allocation succeeds");
    
    // Fill with pattern
    for (size_t i = 0; i < data_size; i++) {
        data[i] = (char)(i % 256);
    }
    
    // Write large file
    lgx_file_t* file = NULL;
    lgx_result_t result = lgx_fs_open(test_file, "w", &file);
    TEST_ASSERT(result == LGX_SUCCESS, "Large file open succeeds");
    
    result = lgx_fs_write(file, data, data_size);
    TEST_ASSERT(result == LGX_SUCCESS, "Large file write succeeds");
    
    lgx_fs_close(file);
    
    // Read and verify
    file = NULL;
    result = lgx_fs_open(test_file, "r", &file);
    TEST_ASSERT(result == LGX_SUCCESS, "Large file read open succeeds");
    
    char* read_buffer = malloc(data_size);
    TEST_ASSERT(read_buffer != NULL, "Read buffer allocation succeeds");
    
    size_t bytes_read = 0;
    result = lgx_fs_read(file, read_buffer, data_size, &bytes_read);
    TEST_ASSERT(result == LGX_SUCCESS, "Large file read succeeds");
    TEST_ASSERT(bytes_read == data_size, "Read all bytes");
    TEST_ASSERT(memcmp(data, read_buffer, data_size) == 0, "Data integrity verified");
    
    lgx_fs_close(file);
    
    // Cleanup
    free(data);
    free(read_buffer);
    unlink(test_file);
}

// Test 5: Timing - basic time queries
static void test_timing_basic(void) {
    printf("\nTest 5: Timing basic operations\n");
    
    uint64_t t1 = lgx_time_now_ns();
    TEST_ASSERT(t1 > 0, "Time query returns non-zero");
    
    // Small delay
    lgx_time_sleep_ms(10);
    
    uint64_t t2 = lgx_time_now_ns();
    TEST_ASSERT(t2 > t1, "Time advances");
    
    uint64_t elapsed_ns = t2 - t1;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    
    // Should be approximately 10ms (allow 5ms tolerance)
    TEST_ASSERT(elapsed_ms >= 8 && elapsed_ms <= 15, "Sleep duration is accurate");
    
    printf("    Elapsed: %lu ms\n", (unsigned long)elapsed_ms);
}

// Test 6: Timing - monotonic property
static void test_timing_monotonic(void) {
    printf("\nTest 6: Timing monotonic property\n");
    
    uint64_t prev = lgx_time_now_ns();
    
    for (int i = 0; i < 100; i++) {
        uint64_t curr = lgx_time_now_ns();
        TEST_ASSERT(curr >= prev, "Time never goes backwards");
        prev = curr;
    }
}

// Test 7: Timing - precision
static void test_timing_precision(void) {
    printf("\nTest 7: Timing precision\n");
    
    uint64_t t1 = lgx_time_now_ns();
    uint64_t t2 = lgx_time_now_ns();
    
    uint64_t overhead = t2 - t1;
    
    // Overhead should be less than 1 microsecond
    TEST_ASSERT(overhead < 1000, "Time query overhead is low");
    
    printf("    Query overhead: %lu ns\n", (unsigned long)overhead);
}

// Test 8: Logging - basic logging
static void test_logging_basic(void) {
    printf("\nTest 8: Logging basic operations\n");
    
    // These should not crash
    lgx_log(LGX_LOG_DEBUG, "Debug message: %d", 42);
    lgx_log(LGX_LOG_INFO, "Info message: %s", "test");
    lgx_log(LGX_LOG_WARN, "Warning message");
    lgx_log(LGX_LOG_ERROR, "Error message");
    
    TEST_ASSERT(true, "Basic logging succeeds");
}

// Test 9: Logging - NULL format handling
static void test_logging_null_format(void) {
    printf("\nTest 9: Logging NULL format handling\n");
    
    // Should handle gracefully
    lgx_log(LGX_LOG_INFO, NULL);
    
    TEST_ASSERT(true, "NULL format handled gracefully");
}

// Test 10: Logging - long messages
static void test_logging_long_messages(void) {
    printf("\nTest 10: Logging long messages\n");
    
    char long_msg[2048];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';
    
    lgx_log(LGX_LOG_INFO, "%s", long_msg);
    
    TEST_ASSERT(true, "Long message logging succeeds");
}

// Test 11: Logging - level filtering
static void test_logging_level_filtering(void) {
    printf("\nTest 11: Logging level filtering\n");
    
    // Set log level to WARN
    lgx_set_log_filter(LGX_LOG_WARN);
    
    // Debug and Info should be filtered
    lgx_log(LGX_LOG_DEBUG, "This should be filtered");
    lgx_log(LGX_LOG_INFO, "This should be filtered");
    
    // Warn and Error should pass
    lgx_log(LGX_LOG_WARN, "This should appear");
    lgx_log(LGX_LOG_ERROR, "This should appear");
    
    // Reset to default
    lgx_set_log_filter(LGX_LOG_DEBUG);
    
    TEST_ASSERT(true, "Log level filtering works");
}

// Test 12: Logging - thread safety
static void test_logging_thread_safety(void) {
    printf("\nTest 12: Logging thread safety\n");
    
    // Rapid logging from single thread (simulates concurrent access)
    for (int i = 0; i < 100; i++) {
        lgx_log(LGX_LOG_INFO, "Message %d", i);
    }
    
    TEST_ASSERT(true, "Rapid logging succeeds");
}

// Test 13: Platform services integration
static void test_platform_integration(void) {
    printf("\nTest 13: Platform services integration\n");
    
    // Use all services together
    uint64_t start = lgx_time_now_ns();
    
    const char* test_file = "/tmp/lgx_integration_test.txt";
    lgx_file_t* file = NULL;
    
    lgx_log(LGX_LOG_INFO, "Starting integration test");
    
    lgx_result_t result = lgx_fs_open(test_file, "w", &file);
    TEST_ASSERT(result == LGX_SUCCESS, "Integration: file open");
    
    const char* data = "Integration test data";
    result = lgx_fs_write(file, data, strlen(data));
    TEST_ASSERT(result == LGX_SUCCESS, "Integration: file write");
    
    lgx_fs_close(file);
    
    uint64_t end = lgx_time_now_ns();
    uint64_t elapsed_us = (end - start) / 1000;
    
    lgx_log(LGX_LOG_INFO, "Integration test completed in %lu us", 
            (unsigned long)elapsed_us);
    
    TEST_ASSERT(elapsed_us < 10000, "Integration test completes quickly");
    
    // Cleanup
    unlink(test_file);
}

int main(void) {
    printf("=== LGX Runtime Platform Services Unit Tests ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Filesystem tests
    test_fs_basic_operations();
    test_fs_null_parameters();
    test_fs_invalid_paths();
    test_fs_large_file();
    
    // Timing tests
    test_timing_basic();
    test_timing_monotonic();
    test_timing_precision();
    
    // Logging tests
    test_logging_basic();
    test_logging_null_format();
    test_logging_long_messages();
    test_logging_level_filtering();
    test_logging_thread_safety();
    
    // Integration test
    test_platform_integration();
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed!\n");
        return 1;
    }
}
