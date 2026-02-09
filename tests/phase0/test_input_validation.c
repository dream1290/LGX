/**
 * Test: Input Validation
 * 
 * Validates input validation functions for security hardening.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_pointer_validation(void) {
    printf("Test: Pointer validation...\n");
    
    // Valid pointer
    int value = 42;
    assert(lgx_validate_pointer(&value, "test_ptr") == true);
    (void)value;  // Mark as used
    
    // NULL pointer
    assert(lgx_validate_pointer(NULL, "null_ptr") == false);
    
    printf("  Pointer validation: OK\n");
}

static void test_size_validation(void) {
    printf("Test: Size validation...\n");
    
    // Valid size
    assert(lgx_validate_size(100, 1, 1000, "size") == true);
    
    // Below minimum
    assert(lgx_validate_size(0, 1, 1000, "size") == false);
    
    // Above maximum
    assert(lgx_validate_size(2000, 1, 1000, "size") == false);
    
    printf("  Size validation: OK\n");
}

static void test_allocation_size_validation(void) {
    printf("Test: Allocation size validation...\n");
    
    // Valid sizes
    assert(lgx_validate_allocation_size(1) == true);
    assert(lgx_validate_allocation_size(1024) == true);
    assert(lgx_validate_allocation_size(1024 * 1024) == true);
    
    // Zero size
    assert(lgx_validate_allocation_size(0) == false);
    
    // Too large (> 16 GB)
    assert(lgx_validate_allocation_size(17ULL * 1024 * 1024 * 1024) == false);
    
    printf("  Allocation size validation: OK\n");
}

static void test_alignment_validation(void) {
    printf("Test: Alignment validation...\n");
    
    // Valid alignments (powers of 2)
    assert(lgx_validate_alignment(1) == true);
    assert(lgx_validate_alignment(2) == true);
    assert(lgx_validate_alignment(4) == true);
    assert(lgx_validate_alignment(8) == true);
    assert(lgx_validate_alignment(16) == true);
    assert(lgx_validate_alignment(64) == true);
    assert(lgx_validate_alignment(256) == true);
    
    // Invalid alignments (not powers of 2)
    assert(lgx_validate_alignment(3) == false);
    assert(lgx_validate_alignment(5) == false);
    assert(lgx_validate_alignment(7) == false);
    assert(lgx_validate_alignment(15) == false);
    
    // Zero alignment
    assert(lgx_validate_alignment(0) == false);
    
    // Too large alignment
    assert(lgx_validate_alignment(2 * 1024 * 1024) == false);
    
    printf("  Alignment validation: OK\n");
}

static void test_string_validation(void) {
    printf("Test: String validation...\n");
    
    // Valid strings
    assert(lgx_validate_string("hello", 100, "str") == true);
    assert(lgx_validate_string("", 100, "str") == true);
    
    // NULL string
    assert(lgx_validate_string(NULL, 100, "str") == false);
    
    // String too long
    char long_str[200];
    memset(long_str, 'a', 199);
    long_str[199] = '\0';
    assert(lgx_validate_string(long_str, 100, "str") == false);
    
    printf("  String validation: OK\n");
}

static void test_string_truncation(void) {
    printf("Test: String truncation...\n");
    
    char dst[10];
    memset(dst, 0, sizeof(dst));  // Initialize to avoid warnings
    
    // Short string (no truncation)
    assert(lgx_validate_and_truncate_string("hello", dst, sizeof(dst), "str") == true);
    assert(strcmp(dst, "hello") == 0);
    
    // Long string (truncation)
    assert(lgx_validate_and_truncate_string("hello world this is long", dst, sizeof(dst), "str") == true);
    assert(strlen(dst) == 9);  // 10 - 1 for null terminator
    
    printf("  String truncation: OK\n");
}

static void test_path_validation(void) {
    printf("Test: Path validation...\n");
    
    // Valid paths
    assert(lgx_validate_path("/tmp/test.txt", "path") == true);
    assert(lgx_validate_path("relative/path.txt", "path") == true);
    
    // Path traversal attempt
    assert(lgx_validate_path("../etc/passwd", "path") == false);
    assert(lgx_validate_path("/tmp/../etc/passwd", "path") == false);
    
    // NULL path
    assert(lgx_validate_path(NULL, "path") == false);
    
    printf("  Path validation: OK\n");
}

static void test_enum_validation(void) {
    printf("Test: Enum validation...\n");
    
    // Valid enum values
    assert(lgx_validate_enum(0, 0, 10, "enum") == true);
    assert(lgx_validate_enum(5, 0, 10, "enum") == true);
    assert(lgx_validate_enum(10, 0, 10, "enum") == true);
    
    // Invalid enum values
    assert(lgx_validate_enum(-1, 0, 10, "enum") == false);
    assert(lgx_validate_enum(11, 0, 10, "enum") == false);
    
    printf("  Enum validation: OK\n");
}

static void test_capability_validation(void) {
    printf("Test: Capability validation...\n");
    
    // Valid capabilities
    assert(lgx_validate_capability(LGX_CAP_HUGE_PAGES) == true);
    assert(lgx_validate_capability(LGX_CAP_GPU_ACCELERATION) == true);
    assert(lgx_validate_capability(LGX_CAP_MESH_SHADERS) == true);
    
    // Invalid capability
    assert(lgx_validate_capability((lgx_capability_t)999) == false);
    
    printf("  Capability validation: OK\n");
}

static void test_log_level_validation(void) {
    printf("Test: Log level validation...\n");
    
    // Valid log levels
    assert(lgx_validate_log_level(LGX_LOG_DEBUG) == true);
    assert(lgx_validate_log_level(LGX_LOG_INFO) == true);
    assert(lgx_validate_log_level(LGX_LOG_WARN) == true);
    assert(lgx_validate_log_level(LGX_LOG_ERROR) == true);
    
    // Invalid log level
    assert(lgx_validate_log_level((lgx_log_level_t)999) == false);
    
    printf("  Log level validation: OK\n");
}

static void test_allocation_intent_validation(void) {
    printf("Test: Allocation intent validation...\n");
    
    // Valid intent
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    assert(lgx_validate_allocation_intent(&intent) == true);
    
    // NULL intent
    assert(lgx_validate_allocation_intent(NULL) == false);
    
    // Invalid size
    lgx_allocation_intent_base_t bad_intent = intent;
    bad_intent.size = 0;
    assert(lgx_validate_allocation_intent(&bad_intent) == false);
    (void)bad_intent;  // Mark as used
    
    printf("  Allocation intent validation: OK\n");
}

static void test_validation_limits(void) {
    printf("Test: Validation limits...\n");
    
    char buffer[1024];
    lgx_validation_get_limits(buffer, sizeof(buffer));
    
    printf("%s", buffer);
    
    // Validate we got output
    assert(strlen(buffer) > 0);
    assert(strstr(buffer, "Max allocation size") != NULL);
    
    printf("  Validation limits: OK\n");
}

int main(void) {
    printf("=== Input Validation Tests ===\n\n");
    
    test_pointer_validation();
    printf("\n");
    
    test_size_validation();
    printf("\n");
    
    test_allocation_size_validation();
    printf("\n");
    
    test_alignment_validation();
    printf("\n");
    
    test_string_validation();
    printf("\n");
    
    test_string_truncation();
    printf("\n");
    
    test_path_validation();
    printf("\n");
    
    test_enum_validation();
    printf("\n");
    
    test_capability_validation();
    printf("\n");
    
    test_log_level_validation();
    printf("\n");
    
    test_allocation_intent_validation();
    printf("\n");
    
    test_validation_limits();
    printf("\n");
    
    printf("=== All Input Validation Tests Passed ===\n");
    return 0;
}
