/**
 * Test: Filesystem API
 * 
 * Tests the filesystem abstraction layer.
 */

#include "lgx_runtime.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

void test_file_write_read(void) {
    printf("\n=== Test: File Write and Read ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    const char* test_path = "/tmp/lgx_test_file.txt";
    const char* test_data = "Hello, LGX Runtime!";
    size_t test_data_len = strlen(test_data);
    
    // Write to file
    lgx_file_t* file = NULL;
    result = lgx_fs_open(test_path, "w", &file);
    assert(result == LGX_SUCCESS);
    assert(file != NULL);
    
    result = lgx_fs_write(file, test_data, test_data_len);
    assert(result == LGX_SUCCESS);
    
    result = lgx_fs_close(file);
    assert(result == LGX_SUCCESS);
    
    printf("  ✓ File written successfully\n");
    
    // Read from file
    file = NULL;
    result = lgx_fs_open(test_path, "r", &file);
    assert(result == LGX_SUCCESS);
    assert(file != NULL);
    
    char buffer[256] = {0};
    size_t bytes_read = 0;
    result = lgx_fs_read(file, buffer, sizeof(buffer) - 1, &bytes_read);
    assert(result == LGX_SUCCESS);
    assert(bytes_read == test_data_len);
    assert(strcmp(buffer, test_data) == 0);
    
    result = lgx_fs_close(file);
    assert(result == LGX_SUCCESS);
    
    printf("  ✓ File read successfully\n");
    printf("  Data: \"%s\"\n", buffer);
    
    // Cleanup
    unlink(test_path);
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ File write/read test passed\n");
}

void test_file_error_handling(void) {
    printf("\n=== Test: File Error Handling ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    // Try to open non-existent file for reading
    lgx_file_t* file = NULL;
    result = lgx_fs_open("/tmp/nonexistent_file_12345.txt", "r", &file);
    assert(result == LGX_ERROR_IO_ERROR);
    assert(file == NULL);
    
    printf("  ✓ Non-existent file error handled correctly\n");
    
    // Try to write to invalid file handle
    result = lgx_fs_write(NULL, "test", 4);
    assert(result == LGX_ERROR_INVALID_PARAM);
    
    printf("  ✓ NULL file handle error handled correctly\n");
    
    // Try to read with NULL buffer
    file = NULL;
    result = lgx_fs_open("/tmp/lgx_test_error.txt", "w", &file);
    assert(result == LGX_SUCCESS);
    
    size_t bytes_read = 0;
    result = lgx_fs_read(file, NULL, 100, &bytes_read);
    assert(result == LGX_ERROR_INVALID_PARAM);
    
    lgx_fs_close(file);
    unlink("/tmp/lgx_test_error.txt");
    
    printf("  ✓ NULL buffer error handled correctly\n");
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ File error handling test passed\n");
}

void test_file_append(void) {
    printf("\n=== Test: File Append ===\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    assert(config != NULL);
    
    lgx_result_t result = lgx_runtime_init(config);
    assert(result == LGX_SUCCESS);
    (void)result;  // Suppress unused warning
    
    const char* test_path = "/tmp/lgx_test_append.txt";
    
    // Write initial data
    lgx_file_t* file = NULL;
    result = lgx_fs_open(test_path, "w", &file);
    assert(result == LGX_SUCCESS);
    
    result = lgx_fs_write(file, "Line 1\n", 7);
    assert(result == LGX_SUCCESS);
    
    lgx_fs_close(file);
    
    // Append more data
    file = NULL;
    result = lgx_fs_open(test_path, "a", &file);
    assert(result == LGX_SUCCESS);
    
    result = lgx_fs_write(file, "Line 2\n", 7);
    assert(result == LGX_SUCCESS);
    
    lgx_fs_close(file);
    
    // Read and verify
    file = NULL;
    result = lgx_fs_open(test_path, "r", &file);
    assert(result == LGX_SUCCESS);
    
    char buffer[256] = {0};
    size_t bytes_read = 0;
    result = lgx_fs_read(file, buffer, sizeof(buffer) - 1, &bytes_read);
    assert(result == LGX_SUCCESS);
    assert(bytes_read == 14);  // "Line 1\nLine 2\n"
    assert(strcmp(buffer, "Line 1\nLine 2\n") == 0);
    
    lgx_fs_close(file);
    
    printf("  ✓ File append successful\n");
    printf("  Content:\n%s", buffer);
    
    // Cleanup
    unlink(test_path);
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("  ✓ File append test passed\n");
}

int main(void) {
    printf("=================================================\n");
    printf("  LGX Filesystem API Tests\n");
    printf("=================================================\n");
    
    test_file_write_read();
    test_file_error_handling();
    test_file_append();
    
    printf("\n=================================================\n");
    printf("  All filesystem tests passed! ✓\n");
    printf("=================================================\n");
    
    return 0;
}
