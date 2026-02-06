/**
 * GPU Memory Pool Tests
 * 
 * Tests GPU memory type detection and budget management (Task 3.2.1)
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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

// Helper: Create minimal Vulkan instance for testing
static VkResult create_test_vulkan_instance(VkInstance* instance) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "LGX GPU Pool Test",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "LGX Runtime",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };
    
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = 0,
        .enabledExtensionCount = 0
    };
    
    return vkCreateInstance(&create_info, NULL, instance);
}

// Helper: Select physical device
static VkResult select_physical_device(VkInstance instance, VkPhysicalDevice* physical_device) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    
    if (device_count == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices);
    
    *physical_device = devices[0];  // Use first device
    free(devices);
    
    return VK_SUCCESS;
}

// Helper: Create logical device
static VkResult create_logical_device(VkPhysicalDevice physical_device, VkDevice* device) {
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = 0,
        .enabledLayerCount = 0
    };
    
    return vkCreateDevice(physical_device, &create_info, NULL, device);
}

void test_vulkan_availability(void) {
    printf("\n[TEST] vulkan_availability\n");
    
    // Try to create Vulkan instance
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = create_test_vulkan_instance(&instance);
    
    if (result != VK_SUCCESS) {
        printf("  ⚠ Vulkan not available (result: %d)\n", result);
        printf("  This is expected if no GPU or Vulkan drivers are installed\n");
        printf("  Skipping GPU pool tests\n");
        return;
    }
    
    TEST_ASSERT(instance != VK_NULL_HANDLE, "Vulkan instance created");
    
    // Enumerate physical devices
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    
    printf("  Found %u physical device(s)\n", device_count);
    TEST_ASSERT(device_count > 0, "At least one physical device found");
    
    // Cleanup
    vkDestroyInstance(instance, NULL);
    printf("  Test completed\n");
}

void test_gpu_pool_initialization(void) {
    printf("\n[TEST] gpu_pool_initialization\n");
    
    // Create Vulkan instance
    VkInstance instance = VK_NULL_HANDLE;
    VkResult vk_result = create_test_vulkan_instance(&instance);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (Vulkan not available)\n");
        return;
    }
    
    // Select physical device
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    vk_result = select_physical_device(instance, &physical_device);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (No physical device)\n");
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Create logical device
    VkDevice device = VK_NULL_HANDLE;
    vk_result = create_logical_device(physical_device, &device);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (Failed to create device)\n");
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Initialize GPU pool
    lgx_result_t result = lgx_gpu_pool_init(instance, physical_device, device);
    TEST_ASSERT(result == LGX_SUCCESS, "GPU pool initialization");
    
    if (result == LGX_SUCCESS) {
        TEST_ASSERT(lgx_gpu_pool_is_initialized(), "GPU pool is initialized");
        
        // Cleanup
        lgx_gpu_pool_shutdown();
    }
    
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    printf("  Test completed\n");
}

void test_memory_type_detection(void) {
    printf("\n[TEST] memory_type_detection\n");
    
    // Create Vulkan instance
    VkInstance instance = VK_NULL_HANDLE;
    VkResult vk_result = create_test_vulkan_instance(&instance);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (Vulkan not available)\n");
        return;
    }
    
    // Select physical device
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    vk_result = select_physical_device(instance, &physical_device);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (No physical device)\n");
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Create logical device
    VkDevice device = VK_NULL_HANDLE;
    vk_result = create_logical_device(physical_device, &device);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (Failed to create device)\n");
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Initialize GPU pool
    lgx_result_t result = lgx_gpu_pool_init(instance, physical_device, device);
    
    if (result != LGX_SUCCESS) {
        printf("  ⚠ Skipping test (GPU pool init failed)\n");
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Check memory type availability
    bool device_local = lgx_gpu_pool_is_memory_type_available(LGX_GPU_DEVICE_LOCAL);
    bool host_visible = lgx_gpu_pool_is_memory_type_available(LGX_GPU_HOST_VISIBLE);
    bool host_cached = lgx_gpu_pool_is_memory_type_available(LGX_GPU_HOST_CACHED);
    
    printf("  Device-local: %s\n", device_local ? "available" : "unavailable");
    printf("  Host-visible: %s\n", host_visible ? "available" : "unavailable");
    printf("  Host-cached:  %s\n", host_cached ? "available" : "unavailable");
    
    // At least device-local should be available on any GPU
    TEST_ASSERT(device_local, "Device-local memory available");
    
    // Get memory budgets
    if (device_local) {
        VkDeviceSize budget = lgx_gpu_pool_get_memory_budget(LGX_GPU_DEVICE_LOCAL);
        printf("  Device-local budget: %llu MB\n", 
               (unsigned long long)(budget / (1024 * 1024)));
        TEST_ASSERT(budget > 0, "Device-local budget > 0");
    }
    
    if (host_visible) {
        VkDeviceSize budget = lgx_gpu_pool_get_memory_budget(LGX_GPU_HOST_VISIBLE);
        printf("  Host-visible budget: %llu MB\n", 
               (unsigned long long)(budget / (1024 * 1024)));
        TEST_ASSERT(budget > 0, "Host-visible budget > 0");
    }
    
    // Cleanup
    lgx_gpu_pool_shutdown();
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    printf("  Test completed\n");
}

void test_memory_budget_tracking(void) {
    printf("\n[TEST] memory_budget_tracking\n");
    
    // Create Vulkan instance
    VkInstance instance = VK_NULL_HANDLE;
    VkResult vk_result = create_test_vulkan_instance(&instance);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (Vulkan not available)\n");
        return;
    }
    
    // Select physical device
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    vk_result = select_physical_device(instance, &physical_device);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (No physical device)\n");
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Create logical device
    VkDevice device = VK_NULL_HANDLE;
    vk_result = create_logical_device(physical_device, &device);
    
    if (vk_result != VK_SUCCESS) {
        printf("  ⚠ Skipping test (Failed to create device)\n");
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Initialize GPU pool
    lgx_result_t result = lgx_gpu_pool_init(instance, physical_device, device);
    
    if (result != LGX_SUCCESS) {
        printf("  ⚠ Skipping test (GPU pool init failed)\n");
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    // Check initial used memory (should be 0)
    VkDeviceSize used = lgx_gpu_pool_get_memory_used(LGX_GPU_DEVICE_LOCAL);
    TEST_ASSERT(used == 0, "Initial used memory is 0");
    
    // Cleanup
    lgx_gpu_pool_shutdown();
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    printf("  Test completed\n");
}

int main(void) {
    printf("=== GPU Memory Pool Tests ===\n");
    printf("Testing GPU memory type detection and budget management (Task 3.2.1)\n");
    
    // Run tests
    test_vulkan_availability();
    test_gpu_pool_initialization();
    test_memory_type_detection();
    test_memory_budget_tracking();
    
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
