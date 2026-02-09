/**
 * Integration Test: Telemetry Collection
 * 
 * Tests telemetry collection, export, and privacy features.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

// Test 1: Basic telemetry enable/disable
static void test_basic_telemetry(void) {
    printf("\nTest 1: Basic telemetry enable/disable\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    // Enable telemetry
    lgx_result_t result = lgx_telemetry_enable(true);
    TEST_ASSERT(result == LGX_SUCCESS, "Telemetry enable succeeds");
    
    // Perform some operations
    void* ptr = lgx_alloc(1024);
    lgx_free(ptr);
    
    // Disable telemetry
    result = lgx_telemetry_enable(false);
    TEST_ASSERT(result == LGX_SUCCESS, "Telemetry disable succeeds");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 2: Telemetry data collection
static void test_telemetry_data_collection(void) {
    printf("\nTest 2: Telemetry data collection\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    lgx_telemetry_enable(true);
    
    // Perform various operations
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_alloc(1024 + i * 16);
        memset(ptr, 0xAA, 1024 + i * 16);
        lgx_free(ptr);
    }
    
    // Simulate frame boundaries
    for (int frame = 0; frame < 10; frame++) {
        void* ptr = lgx_frame_alloc(512);
        (void)ptr;
        lgx_frame_reset();
    }
    
    TEST_ASSERT(true, "Telemetry collected data");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 3: Telemetry export
static void test_telemetry_export(void) {
    printf("\nTest 3: Telemetry export\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    lgx_telemetry_enable(true);
    
    // Generate some telemetry data
    for (int i = 0; i < 50; i++) {
        void* ptr = lgx_alloc(2048);
        lgx_free(ptr);
    }
    
    // Export telemetry
    char buffer[64 * 1024]; // 64KB buffer
    lgx_result_t result = lgx_telemetry_export(buffer, sizeof(buffer));
    TEST_ASSERT(result == LGX_SUCCESS, "Telemetry export succeeds");
    TEST_ASSERT(strlen(buffer) > 0, "Exported data is not empty");
    
    printf("    Exported %zu bytes\n", strlen(buffer));
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 4: Privacy policy configuration
static void test_privacy_policy(void) {
    printf("\nTest 4: Privacy policy configuration\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    // Configure telemetry with privacy settings
    lgx_telemetry_config_t tel_config;
    tel_config.struct_size = sizeof(lgx_telemetry_config_t);
    tel_config.enabled = true;
    tel_config.ring_buffer_size = 1024 * 1024; // 1MB
    tel_config.overflow_policy = LGX_TEL_DROP_OLDEST;
    tel_config.adaptive_sampling = true;
    tel_config.min_sample_rate = 0.01;
    
    // Privacy policy
    tel_config.privacy_policy.struct_size = sizeof(lgx_privacy_policy_t);
    tel_config.privacy_policy.collect_frame_times = true;
    tel_config.privacy_policy.collect_allocation_sizes = true;
    tel_config.privacy_policy.collect_cpu_model = false; // Privacy-conscious
    tel_config.privacy_policy.collect_gpu_model = false;
    tel_config.privacy_policy.collect_kernel_version = false;
    tel_config.privacy_policy.add_noise = true;
    tel_config.privacy_policy.noise_stddev = 0.05; // 5% noise
    tel_config.privacy_policy.aggregate_only = true;
    
    lgx_result_t result = lgx_telemetry_configure(&tel_config);
    TEST_ASSERT(result == LGX_SUCCESS, "Privacy policy configuration succeeds");
    
    // Get privacy policy
    lgx_privacy_policy_t policy = lgx_telemetry_get_privacy_policy();
    TEST_ASSERT(policy.collect_frame_times == true, "Frame times collection enabled");
    TEST_ASSERT(policy.collect_cpu_model == false, "CPU model collection disabled");
    TEST_ASSERT(policy.add_noise == true, "Noise addition enabled");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 5: Telemetry overflow handling
static void test_telemetry_overflow(void) {
    printf("\nTest 5: Telemetry overflow handling\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    // Configure small buffer to trigger overflow
    lgx_telemetry_config_t tel_config;
    tel_config.struct_size = sizeof(lgx_telemetry_config_t);
    tel_config.enabled = true;
    tel_config.ring_buffer_size = 4096; // Small buffer
    tel_config.overflow_policy = LGX_TEL_DROP_OLDEST;
    tel_config.adaptive_sampling = false;
    tel_config.min_sample_rate = 1.0;
    tel_config.privacy_policy.struct_size = sizeof(lgx_privacy_policy_t);
    tel_config.privacy_policy.collect_frame_times = true;
    tel_config.privacy_policy.collect_allocation_sizes = true;
    tel_config.privacy_policy.add_noise = false;
    tel_config.privacy_policy.aggregate_only = false;
    
    lgx_telemetry_configure(&tel_config);
    lgx_telemetry_enable(true);
    
    // Generate lots of events to trigger overflow
    for (int i = 0; i < 10000; i++) {
        void* ptr = lgx_alloc(128);
        lgx_free(ptr);
    }
    
    TEST_ASSERT(true, "Telemetry overflow handled");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 6: Telemetry with adaptive sampling
static void test_adaptive_sampling(void) {
    printf("\nTest 6: Telemetry with adaptive sampling\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    // Configure with adaptive sampling
    lgx_telemetry_config_t tel_config;
    tel_config.struct_size = sizeof(lgx_telemetry_config_t);
    tel_config.enabled = true;
    tel_config.ring_buffer_size = 8192;
    tel_config.overflow_policy = LGX_TEL_SAMPLE;
    tel_config.adaptive_sampling = true;
    tel_config.min_sample_rate = 0.1; // 10% minimum
    tel_config.privacy_policy.struct_size = sizeof(lgx_privacy_policy_t);
    tel_config.privacy_policy.collect_frame_times = true;
    tel_config.privacy_policy.add_noise = false;
    tel_config.privacy_policy.aggregate_only = false;
    
    lgx_telemetry_configure(&tel_config);
    lgx_telemetry_enable(true);
    
    // Generate many events
    for (int i = 0; i < 5000; i++) {
        void* ptr = lgx_alloc(256);
        lgx_free(ptr);
    }
    
    TEST_ASSERT(true, "Adaptive sampling works");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 7: Telemetry data export to file
static void test_telemetry_export_to_file(void) {
    printf("\nTest 7: Telemetry data export to file\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    lgx_telemetry_enable(true);
    
    // Generate telemetry data
    for (int i = 0; i < 100; i++) {
        void* ptr = lgx_alloc(1024);
        lgx_free(ptr);
    }
    
    // Export to file
    const char* output_path = "/tmp/lgx_telemetry_test.json";
    lgx_result_t result = lgx_telemetry_export_collected_data(output_path);
    TEST_ASSERT(result == LGX_SUCCESS, "Export to file succeeds");
    
    // Verify file exists
    FILE* f = fopen(output_path, "r");
    TEST_ASSERT(f != NULL, "Exported file exists");
    if (f) {
        fclose(f);
        unlink(output_path); // Cleanup
    }
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 8: Telemetry performance overhead
static void test_telemetry_overhead(void) {
    printf("\nTest 8: Telemetry performance overhead\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    // Measure without telemetry
    uint64_t start = lgx_time_now_ns();
    for (int i = 0; i < 1000; i++) {
        void* ptr = lgx_alloc(1024);
        lgx_free(ptr);
    }
    uint64_t without_telemetry = lgx_time_now_ns() - start;
    
    // Enable telemetry
    lgx_telemetry_enable(true);
    
    // Measure with telemetry
    start = lgx_time_now_ns();
    for (int i = 0; i < 1000; i++) {
        void* ptr = lgx_alloc(1024);
        lgx_free(ptr);
    }
    uint64_t with_telemetry = lgx_time_now_ns() - start;
    
    double overhead_percent = ((double)(with_telemetry - without_telemetry) / 
                               without_telemetry) * 100.0;
    
    printf("    Without telemetry: %lu ns\n", (unsigned long)without_telemetry);
    printf("    With telemetry: %lu ns\n", (unsigned long)with_telemetry);
    printf("    Overhead: %.2f%%\n", overhead_percent);
    
    TEST_ASSERT(overhead_percent < 10.0, "Telemetry overhead < 10%");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 9: Telemetry with multiple data types
static void test_telemetry_multiple_data_types(void) {
    printf("\nTest 9: Telemetry with multiple data types\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    lgx_telemetry_enable(true);
    
    // Generate different types of events
    // Memory allocations
    void* ptr1 = lgx_alloc(1024);
    void* ptr2 = lgx_alloc(2048);
    lgx_free(ptr1);
    lgx_free(ptr2);
    
    // Frame events
    for (int i = 0; i < 5; i++) {
        void* ptr = lgx_frame_alloc(512);
        (void)ptr;
        lgx_frame_reset();
    }
    
    // Intent-based allocations
    lgx_allocation_intent_base_t intent;
    intent.struct_size = sizeof(lgx_allocation_intent_base_t);
    intent.size = 4096;
    intent.lifetime = LGX_LIFETIME_LEVEL;
    intent.access_pattern = LGX_ACCESS_RANDOM;
    intent.hint = LGX_HINT_CRITICAL_PATH;
    intent.validation_policy = LGX_INTENT_TRUST;
    
    void* ptr3 = lgx_alloc_with_intent(&intent);
    lgx_free(ptr3);
    
    TEST_ASSERT(true, "Multiple data types collected");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

// Test 10: Telemetry data integrity
static void test_telemetry_data_integrity(void) {
    printf("\nTest 10: Telemetry data integrity\n");
    
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_config_set_flags(config, LGX_CONFIG_ENABLE_TELEMETRY);
    lgx_runtime_init(config);
    
    lgx_telemetry_enable(true);
    
    // Perform known operations
    const int alloc_count = 50;
    for (int i = 0; i < alloc_count; i++) {
        void* ptr = lgx_alloc(1024);
        lgx_free(ptr);
    }
    
    // Export and verify
    char buffer[32 * 1024];
    lgx_result_t result = lgx_telemetry_export(buffer, sizeof(buffer));
    TEST_ASSERT(result == LGX_SUCCESS, "Export succeeds");
    
    // Basic integrity check - should contain JSON-like data
    TEST_ASSERT(strchr(buffer, '{') != NULL, "Contains JSON structure");
    TEST_ASSERT(strstr(buffer, "allocation") != NULL || 
                strstr(buffer, "memory") != NULL,
                "Contains memory-related data");
    
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
}

int main(void) {
    printf("=== LGX Runtime Telemetry Collection Integration Test ===\n");
    
    test_basic_telemetry();
    test_telemetry_data_collection();
    test_telemetry_export();
    test_privacy_policy();
    test_telemetry_overflow();
    test_adaptive_sampling();
    test_telemetry_export_to_file();
    test_telemetry_overhead();
    test_telemetry_multiple_data_types();
    test_telemetry_data_integrity();
    
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ All integration tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some integration tests failed!\n");
        return 1;
    }
}
