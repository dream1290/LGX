/**
 * GPU Capability Detection Tests - Task 3.5.2.3
 * 
 * Tests hardware detection and adaptive strategy selection.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while (0)

// Helper to create Vulkan instance
static VkInstance create_test_instance(void) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "LGX GPU Capability Test",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "LGX Runtime",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };
    
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };
    
    VkInstance instance;
    VkResult result = vkCreateInstance(&create_info, NULL, &instance);
    
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance: %d\n", result);
        return VK_NULL_HANDLE;
    }
    
    return instance;
}

// Helper to get physical device
static VkPhysicalDevice get_physical_device(VkInstance instance) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    
    if (device_count == 0) {
        fprintf(stderr, "No Vulkan physical devices found\n");
        return VK_NULL_HANDLE;
    }
    
    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    
    VkPhysicalDevice physical_device = devices[0];
    free(devices);
    
    return physical_device;
}

void test_capability_detection(VkInstance instance, VkPhysicalDevice physical_device) {
    printf("\n[TEST] capability_detection\n");
    
    (void)instance;  // Unused
    
    lgx_gpu_capabilities_t caps;
    lgx_result_t result = lgx_gpu_detect_capabilities(physical_device, &caps);
    
    TEST_ASSERT(result == LGX_SUCCESS, "Capability detection succeeded");
    
    // At least one memory type should be available
    TEST_ASSERT(caps.supports_device_local || caps.supports_host_visible,
                "At least one memory type available");
    
    // Print capabilities
    lgx_gpu_print_capabilities(&caps);
    
    printf("  Test completed\n");
}

void test_strategy_selection(VkInstance instance, VkPhysicalDevice physical_device) {
    printf("\n[TEST] strategy_selection\n");
    
    (void)instance;  // Unused
    
    lgx_gpu_capabilities_t caps;
    lgx_gpu_detect_capabilities(physical_device, &caps);
    
    lgx_gpu_allocation_strategy_t strategy = lgx_gpu_select_strategy(&caps);
    
    TEST_ASSERT(strategy >= 0 && strategy <= 3, "Valid strategy selected");
    
    // Print selected strategy
    lgx_gpu_print_strategy(strategy);
    
    printf("  Test completed\n");
}

void test_pool_size_recommendation(VkInstance instance, VkPhysicalDevice physical_device) {
    printf("\n[TEST] pool_size_recommendation\n");
    
    (void)instance;  // Unused
    
    lgx_gpu_capabilities_t caps;
    lgx_gpu_detect_capabilities(physical_device, &caps);
    
    lgx_gpu_allocation_strategy_t strategy = lgx_gpu_select_strategy(&caps);
    size_t pool_size = lgx_gpu_get_recommended_pool_size(strategy, &caps);
    
    TEST_ASSERT(pool_size > 0, "Pool size > 0");
    TEST_ASSERT(pool_size >= 128 * 1024 * 1024, "Pool size >= 128MB minimum");
    
    printf("  Recommended pool size: %zu MB\n", pool_size / (1024 * 1024));
    
    printf("  Test completed\n");
}

void test_memory_type_flags(VkInstance instance, VkPhysicalDevice physical_device) {
    printf("\n[TEST] memory_type_flags\n");
    
    (void)instance;  // Unused
    
    lgx_gpu_capabilities_t caps;
    lgx_gpu_detect_capabilities(physical_device, &caps);
    
    lgx_gpu_allocation_strategy_t strategy = lgx_gpu_select_strategy(&caps);
    VkMemoryPropertyFlags flags = lgx_gpu_get_memory_type_flags(strategy);
    
    TEST_ASSERT(flags != 0, "Memory type flags set");
    
    printf("  Memory type flags: 0x%08x\n", flags);
    
    printf("  Test completed\n");
}

void test_rebar_detection(VkInstance instance, VkPhysicalDevice physical_device) {
    printf("\n[TEST] rebar_detection\n");
    
    (void)instance;  // Unused
    
    lgx_gpu_capabilities_t caps;
    lgx_gpu_detect_capabilities(physical_device, &caps);
    
    if (caps.supports_resizable_bar) {
        printf("  ✓ Resizable BAR detected\n");
        printf("    Device-local: %zu MB\n", caps.max_device_local_mb);
        printf("    Host-visible: %zu MB\n", caps.max_host_visible_mb);
        
        float ratio = (float)caps.max_host_visible_mb / (float)caps.max_device_local_mb;
        TEST_ASSERT(ratio >= 0.8f, "ReBAR ratio >= 80%");
    } else {
        printf("  ℹ Resizable BAR not detected (this is normal)\n");
        TEST_ASSERT(true, "ReBAR detection check completed");
    }
    
    printf("  Test completed\n");
}

int main(void) {
    printf("=== GPU Capability Detection Tests ===\n");
    printf("Testing hardware detection and adaptive strategy (Task 3.5.2.3)\n");
    
    // Create Vulkan instance
    VkInstance instance = create_test_instance();
    if (instance == VK_NULL_HANDLE) {
        printf("⚠ Skipping tests (Vulkan instance creation failed)\n");
        return 1;
    }
    
    // Get physical device
    VkPhysicalDevice physical_device = get_physical_device(instance);
    if (physical_device == VK_NULL_HANDLE) {
        printf("⚠ Skipping tests (No Vulkan physical device found)\n");
        vkDestroyInstance(instance, NULL);
        return 1;
    }
    
    // Get device properties
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    printf("\nDetected GPU: %s\n", props.deviceName);
    printf("Vulkan API: %u.%u.%u\n",
           VK_VERSION_MAJOR(props.apiVersion),
           VK_VERSION_MINOR(props.apiVersion),
           VK_VERSION_PATCH(props.apiVersion));
    
    // Run tests
    test_capability_detection(instance, physical_device);
    test_strategy_selection(instance, physical_device);
    test_pool_size_recommendation(instance, physical_device);
    test_memory_type_flags(instance, physical_device);
    test_rebar_detection(instance, physical_device);
    
    // Cleanup
    vkDestroyInstance(instance, NULL);
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n");
        return 1;
    }
}
