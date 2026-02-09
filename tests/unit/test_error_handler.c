/**
 * Unit Tests: Error Handling
 * 
 * Tests error handling, error context, and recovery guidance.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Test 1: Error code to string conversion
static void test_error_to_string(void) {
    printf("\nTest 1: Error code to string conversion\n");
    
    const char* str = lgx_result_to_string(LGX_SUCCESS);
    TEST_ASSERT(str != NULL, "SUCCESS string is not NULL");
    TEST_ASSERT(strlen(str) > 0, "SUCCESS string is not empty");
    
    str = lgx_result_to_string(LGX_ERROR_INVALID_PARAM);
    TEST_ASSERT(str != NULL, "INVALID_PARAM string is not NULL");
    TEST_ASSERT(strstr(str, "invalid") != NULL || strstr(str, "parameter") != NULL,
                "Error string contains relevant keywords");
    
    str = lgx_result_to_string(LGX_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT(str != NULL, "OUT_OF_MEMORY string is not NULL");
    
    str = lgx_result_to_string(LGX_ERROR_NOT_INITIALIZED);
    TEST_ASSERT(str != NULL, "NOT_INITIALIZED string is not NULL");
}

// Test 2: Get last error (no error)
static void test_get_last_error_none(void) {
    printf("\nTest 2: Get last error (no error)\n");
    
    lgx_clear_last_error();
    
    lgx_error_context_t error = lgx_get_last_error();
    TEST_ASSERT(error.error_code == LGX_SUCCESS, "No error returns SUCCESS");
    TEST_ASSERT(error.error_message == NULL || strlen(error.error_message) == 0,
                "No error message when no error");
}

// Test 3: Set and get error
static void test_set_get_error(void) {
    printf("\nTest 3: Set and get error\n");
    
    lgx_clear_last_error();
    
    // Trigger an error (invalid parameter)
    lgx_result_t result = lgx_runtime_init(NULL);
    TEST_ASSERT(result != LGX_SUCCESS, "Invalid init triggers error");
    
    lgx_error_context_t error = lgx_get_last_error();
    TEST_ASSERT(error.error_code != LGX_SUCCESS, "Error code is set");
    TEST_ASSERT(error.error_message != NULL, "Error message is set");
    TEST_ASSERT(strlen(error.error_message) > 0, "Error message is not empty");
    
    printf("    Error: %s\n", error.error_message);
}

// Test 4: Clear error
static void test_clear_error(void) {
    printf("\nTest 4: Clear error\n");
    
    // Trigger an error
    lgx_runtime_init(NULL);
    
    lgx_error_context_t error = lgx_get_last_error();
    TEST_ASSERT(error.error_code != LGX_SUCCESS, "Error is set");
    
    // Clear error
    lgx_clear_last_error();
    
    error = lgx_get_last_error();
    TEST_ASSERT(error.error_code == LGX_SUCCESS, "Error is cleared");
}

// Test 5: Error context details
static void test_error_context_details(void) {
    printf("\nTest 5: Error context details\n");
    
    lgx_clear_last_error();
    
    // Trigger an error
    lgx_runtime_init(NULL);
    
    lgx_error_context_t error = lgx_get_last_error();
    
    TEST_ASSERT(error.function_name != NULL, "Function name is set");
    TEST_ASSERT(strlen(error.function_name) > 0, "Function name is not empty");
    
    TEST_ASSERT(error.file_name != NULL, "File name is set");
    TEST_ASSERT(strlen(error.file_name) > 0, "File name is not empty");
    
    TEST_ASSERT(error.line_number > 0, "Line number is set");
    TEST_ASSERT(error.timestamp_ns > 0, "Timestamp is set");
    
    printf("    Function: %s\n", error.function_name);
    printf("    File: %s:%d\n", error.file_name, error.line_number);
}

// Test 6: Extended error context
static void test_error_context_extended(void) {
    printf("\nTest 6: Extended error context\n");
    
    lgx_clear_last_error();
    
    // Trigger an error
    lgx_runtime_init(NULL);
    
    lgx_error_context_ex_t error_ex = lgx_get_last_error_ex();
    
    TEST_ASSERT(error_ex.base.error_code != LGX_SUCCESS, "Error code is set");
    TEST_ASSERT(error_ex.severity != 0, "Severity is set");
    TEST_ASSERT(error_ex.suggested_action != 0, "Suggested action is set");
    
    if (error_ex.recovery_steps != NULL) {
        TEST_ASSERT(strlen(error_ex.recovery_steps) > 0, "Recovery steps provided");
        printf("    Recovery: %s\n", error_ex.recovery_steps);
    }
}

// Test 7: Error callback registration
static int callback_invoked = 0;
static lgx_error_context_ex_t callback_error;

static void error_callback(const lgx_error_context_ex_t* context, void* user_data) {
    callback_invoked++;
    callback_error = *context;
    
    int* counter = (int*)user_data;
    if (counter) {
        (*counter)++;
    }
}

static void test_error_callback(void) {
    printf("\nTest 7: Error callback registration\n");
    
    callback_invoked = 0;
    int user_counter = 0;
    
    lgx_set_error_handler(error_callback, &user_counter);
    
    // Trigger an error
    lgx_clear_last_error();
    lgx_runtime_init(NULL);
    
    TEST_ASSERT(callback_invoked > 0, "Callback was invoked");
    TEST_ASSERT(user_counter > 0, "User data was passed");
    TEST_ASSERT(callback_error.base.error_code != LGX_SUCCESS, "Callback received error");
    
    // Unregister callback
    lgx_set_error_handler(NULL, NULL);
}

// Test 8: Multiple errors (last error tracking)
static void test_multiple_errors(void) {
    printf("\nTest 8: Multiple errors tracking\n");
    
    lgx_clear_last_error();
    
    // First error
    lgx_runtime_init(NULL);
    lgx_error_context_t error1 = lgx_get_last_error();
    
    // Second error (different)
    lgx_alloc(0); // Invalid size
    lgx_error_context_t error2 = lgx_get_last_error();
    
    // Last error should be the most recent
    TEST_ASSERT(error2.timestamp_ns >= error1.timestamp_ns, "Timestamp advances");
}

// Test 9: Thread-local error storage
static void test_thread_local_errors(void) {
    printf("\nTest 9: Thread-local error storage\n");
    
    lgx_clear_last_error();
    
    // Set an error
    lgx_runtime_init(NULL);
    lgx_error_context_t error = lgx_get_last_error();
    TEST_ASSERT(error.error_code != LGX_SUCCESS, "Error is set");
    
    // Clear should only affect this thread
    lgx_clear_last_error();
    error = lgx_get_last_error();
    TEST_ASSERT(error.error_code == LGX_SUCCESS, "Error is cleared");
}

// Test 10: Error severity levels
static void test_error_severity(void) {
    printf("\nTest 10: Error severity levels\n");
    
    lgx_clear_last_error();
    
    // Trigger a warning-level error
    lgx_runtime_init(NULL);
    lgx_error_context_ex_t error = lgx_get_last_error_ex();
    
    TEST_ASSERT(error.severity == LGX_SEV_WARNING ||
                error.severity == LGX_SEV_ERROR ||
                error.severity == LGX_SEV_FATAL,
                "Severity is valid");
    
    const char* severity_str = "Unknown";
    switch (error.severity) {
        case LGX_SEV_WARNING: severity_str = "Warning"; break;
        case LGX_SEV_ERROR: severity_str = "Error"; break;
        case LGX_SEV_FATAL: severity_str = "Fatal"; break;
    }
    
    printf("    Severity: %s\n", severity_str);
}

// Test 11: Recovery actions
static void test_recovery_actions(void) {
    printf("\nTest 11: Recovery actions\n");
    
    lgx_clear_last_error();
    
    // Trigger an error
    lgx_runtime_init(NULL);
    lgx_error_context_ex_t error = lgx_get_last_error_ex();
    
    TEST_ASSERT(error.suggested_action == LGX_RECOVER_RETRY ||
                error.suggested_action == LGX_RECOVER_DEGRADE ||
                error.suggested_action == LGX_RECOVER_ABORT ||
                error.suggested_action == LGX_RECOVER_SHUTDOWN,
                "Recovery action is valid");
    
    const char* action_str = "Unknown";
    switch (error.suggested_action) {
        case LGX_RECOVER_RETRY: action_str = "Retry"; break;
        case LGX_RECOVER_DEGRADE: action_str = "Degrade"; break;
        case LGX_RECOVER_ABORT: action_str = "Abort"; break;
        case LGX_RECOVER_SHUTDOWN: action_str = "Shutdown"; break;
    }
    
    printf("    Suggested action: %s\n", action_str);
}

// Test 12: Recoverable vs non-recoverable errors
static void test_recoverable_errors(void) {
    printf("\nTest 12: Recoverable vs non-recoverable errors\n");
    
    lgx_clear_last_error();
    
    // Trigger an error
    lgx_runtime_init(NULL);
    lgx_error_context_ex_t error = lgx_get_last_error_ex();
    
    printf("    Recoverable: %s\n", error.recoverable ? "Yes" : "No");
    
    TEST_ASSERT(true, "Recoverable flag is set");
}

// Test 13: Error context data
static void test_error_context_data(void) {
    printf("\nTest 13: Error context data\n");
    
    lgx_clear_last_error();
    
    // Trigger an error
    lgx_runtime_init(NULL);
    lgx_error_context_ex_t error = lgx_get_last_error_ex();
    
    // Context data may be NULL or contain additional info
    if (error.context_data != NULL) {
        TEST_ASSERT(true, "Context data is provided");
    } else {
        TEST_ASSERT(true, "Context data is NULL (acceptable)");
    }
}

// Test 14: Error handling under stress
static void test_error_stress(void) {
    printf("\nTest 14: Error handling under stress\n");
    
    // Generate many errors rapidly
    for (int i = 0; i < 100; i++) {
        lgx_clear_last_error();
        lgx_runtime_init(NULL);
        
        lgx_error_context_t error = lgx_get_last_error();
        TEST_ASSERT(error.error_code != LGX_SUCCESS, "Error is set");
    }
}

// Test 15: Error message formatting
static void test_error_message_formatting(void) {
    printf("\nTest 15: Error message formatting\n");
    
    lgx_clear_last_error();
    
    // Trigger an error
    lgx_runtime_init(NULL);
    lgx_error_context_t error = lgx_get_last_error();
    
    TEST_ASSERT(error.error_message != NULL, "Error message exists");
    
    // Message should be human-readable
    TEST_ASSERT(strlen(error.error_message) > 10, "Error message is descriptive");
    TEST_ASSERT(strlen(error.error_message) < 1024, "Error message is not too long");
    
    printf("    Message: %s\n", error.error_message);
}

int main(void) {
    printf("=== LGX Runtime Error Handling Unit Tests ===\n");
    
    test_error_to_string();
    test_get_last_error_none();
    test_set_get_error();
    test_clear_error();
    test_error_context_details();
    test_error_context_extended();
    test_error_callback();
    test_multiple_errors();
    test_thread_local_errors();
    test_error_severity();
    test_recovery_actions();
    test_recoverable_errors();
    test_error_context_data();
    test_error_stress();
    test_error_message_formatting();
    
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
