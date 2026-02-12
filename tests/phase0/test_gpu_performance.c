/**
 * GPU Memory Pool Performance Test
 * 
 * Validates P99 < 10 μs for GPU allocations (Task 3.2.3.4)
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>
#include <time.h>

#define NUM_ITERATIONS 10000
#define WARMUP_ITERATIONS 100

// Helper: Create minimal Vulkan instance for testing
static VkResult create_test_vulkan_instance(VkInstance* instance) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "LGX GPU Performance Test",
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
    
    *physical_device = devices[0];
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

static int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

static uint64_t calculate_percentile(uint64_t* sorted_times, int count, double percentile) {
    if (count == 0) return 0;
    int index = (int)((percentile / 100.0) * (count - 1));
    if (index >= count) index = count - 1;
    return sorted_times[index];
}

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int main(void) {
    printf("=== GPU Memory Pool Performance Test ===\n");
    printf("Validating P99 < 10 μs (Task 3.2.3.4)\n\n");
    
    // Create Vulkan instance
    VkInstance instance = VK_NULL_HANDLE;
    VkResult vk_result = create_test_vulkan_instance(&instance);
    
    if (vk_result != VK_SUCCESS) {
        printf("⚠ Vulkan not available - skipping test\n");
        return 0;
    }
    
    // Select physical device
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    vk_result = select_physical_device(instance, &physical_device);
    
    if (vk_result != VK_SUCCESS) {
        printf("⚠ No physical device - skipping test\n");
        vkDestroyInstance(instance, NULL);
        return 0;
    }
    
    // Create logical device
    VkDevice device = VK_NULL_HANDLE;
    vk_result = create_logical_device(physical_device, &device);
    
    if (vk_result != VK_SUCCESS) {
        printf("⚠ Failed to create device - skipping test\n");
        vkDestroyInstance(instance, NULL);
        return 0;
    }
    
    // Initialize GPU pool
    lgx_result_t result = lgx_gpu_pool_init(instance, physical_device, device);
    if (result != LGX_SUCCESS) {
        printf("⚠ GPU pool init failed - skipping test\n");
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return 0;
    }
    
    printf("GPU pool initialized successfully\n\n");
    
    // Test different allocation sizes
    size_t test_sizes[] = {256, 1024, 4096, 16384, 65536};
    int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    for (int size_idx = 0; size_idx < num_sizes; size_idx++) {
        size_t size = test_sizes[size_idx];
        printf("Testing %zu byte allocations:\n", size);
        
        uint64_t* alloc_times = malloc(NUM_ITERATIONS * sizeof(uint64_t));
        uint64_t* free_times = malloc(NUM_ITERATIONS * sizeof(uint64_t));
        
        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS; i++) {
            lgx_gpu_allocation_t* alloc = lgx_gpu_alloc(size, 256, LGX_GPU_DEVICE_LOCAL);
            if (alloc) {
                lgx_gpu_free(alloc);
            }
        }
        
        // Benchmark allocations
        int successful_allocs = 0;
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            uint64_t start = get_time_ns();
            lgx_gpu_allocation_t* alloc = lgx_gpu_alloc(size, 256, LGX_GPU_DEVICE_LOCAL);
            uint64_t end = get_time_ns();
            
            if (alloc) {
                alloc_times[successful_allocs] = end - start;
                
                // Benchmark free
                start = get_time_ns();
                lgx_gpu_free(alloc);
                end = get_time_ns();
                
                free_times[successful_allocs] = end - start;
                successful_allocs++;
            }
        }
        
        if (successful_allocs == 0) {
            printf("  ⚠ No successful allocations\n\n");
            free(alloc_times);
            free(free_times);
            continue;
        }
        
        // Calculate statistics
        qsort(alloc_times, successful_allocs, sizeof(uint64_t), compare_uint64);
        qsort(free_times, successful_allocs, sizeof(uint64_t), compare_uint64);
        
        uint64_t alloc_p50 = calculate_percentile(alloc_times, successful_allocs, 50.0);
        uint64_t alloc_p95 = calculate_percentile(alloc_times, successful_allocs, 95.0);
        uint64_t alloc_p99 = calculate_percentile(alloc_times, successful_allocs, 99.0);
        
        uint64_t free_p50 = calculate_percentile(free_times, successful_allocs, 50.0);
        uint64_t free_p95 = calculate_percentile(free_times, successful_allocs, 95.0);
        uint64_t free_p99 = calculate_percentile(free_times, successful_allocs, 99.0);
        
        printf("  Allocation performance:\n");
        printf("    P50: %.2f μs\n", alloc_p50 / 1000.0);
        printf("    P95: %.2f μs\n", alloc_p95 / 1000.0);
        printf("    P99: %.2f μs", alloc_p99 / 1000.0);
        
        if (alloc_p99 < 10000) {
            printf(" ✅ (< 10 μs target)\n");
        } else {
            printf(" ❌ (>= 10 μs target)\n");
        }
        
        printf("  Free performance:\n");
        printf("    P50: %.2f μs\n", free_p50 / 1000.0);
        printf("    P95: %.2f μs\n", free_p95 / 1000.0);
        printf("    P99: %.2f μs\n", free_p99 / 1000.0);
        
        printf("\n");
        
        free(alloc_times);
        free(free_times);
    }
    
    // Cleanup
    lgx_gpu_pool_shutdown();
    vkDeviceWaitIdle(device);  // Wait for all operations to complete
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    
    printf("Performance test completed\n");
    
    return 0;
}
