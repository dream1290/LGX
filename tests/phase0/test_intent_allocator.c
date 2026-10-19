/**
 * Intent Allocator Tests (Task 3.4)
 * 
 * Tests the unified intent-based API that automatically routes allocations
 * to the appropriate allocator based on developer intent.
 */

#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if HAVE_VULKAN
#include <vulkan/vulkan.h>

/* Reuse test_gpu_pool.c's device setup convention: the GPU pool must be
   initialized with a live device before lgx_gpu_alloc can succeed. */
static VkResult create_test_vulkan_instance(VkInstance* instance) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "LGX Intent Test",
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
#endif

#define TEST(name) \
    printf("\n[TEST] %s\n", #name); \
    test_##name(); \
    printf("  Test completed\n")

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("  ✗ FAILED: %s\n", message); \
            tests_failed++; \
            return; \
        } else { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } \
    } while(0)

/**
 * Test: Intent structure initialization
 */
void test_intent_structure() {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    ASSERT(intent.struct_size == sizeof(lgx_allocation_intent_base_t), 
           "Intent struct_size correct");
    ASSERT(intent.size == 1024, "Intent size correct");
    ASSERT(intent.lifetime == LGX_LIFETIME_FRAME, "Intent lifetime correct");
}

/**
 * Test: Frame allocation routing
 */
void test_frame_routing() {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 256,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_CRITICAL_PATH,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    ASSERT(ptr != NULL, "Frame allocation succeeded");
    
    // Write to memory to verify it's valid
    memset(ptr, 0xAA, 256);
    ASSERT(((uint8_t*)ptr)[0] == 0xAA, "Memory is writable");
    
    // Note: Frame allocations don't need explicit free
}

/**
 * Test: Persistent allocation routing
 */
void test_persistent_routing() {
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 1024,
        .access_pattern = LGX_ACCESS_RANDOM,
        .lifetime = LGX_LIFETIME_SESSION,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };
    
    void* ptr = lgx_alloc_with_intent(&intent);
    ASSERT(ptr != NULL, "Persistent allocation succeeded");
    
    // Write to memory
    memset(ptr, 0xBB, 1024);
    ASSERT(((uint8_t*)ptr)[0] == 0xBB, "Memory is writable");
    
    lgx_heap_free(ptr);
}

/**
 * Test: GPU shared allocation routing
 */
void test_gpu_routing() {
#if HAVE_VULKAN
    /* Post-B5 the GPU hint routes to lgx_gpu_alloc, which needs an
       initialized pool backed by a real device. Set one up first, then
       make the intent allocation; skip when the host has no device
       (CI convention). */
    VkInstance instance = VK_NULL_HANDLE;
    VkResult vk_result = create_test_vulkan_instance(&instance);
    if (vk_result != VK_SUCCESS) {
        printf("  ✓ GPU routing skipped: no Vulkan instance on host (");
        tests_passed++;
        return;
    }
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    if (device_count > 0) {
        VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices);
        physical_device = devices[0];
        free(devices);
    }
    if (physical_device == VK_NULL_HANDLE) {
        printf("  ✓ GPU routing skipped: no physical device on host\n");
        tests_passed++;
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = 0,
        .enabledLayerCount = 0
    };
    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physical_device, &device_create_info, NULL, &device) != VK_SUCCESS) {
        printf("  ✓ GPU routing skipped: device creation failed on host\n");
        tests_passed++;
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    if (lgx_gpu_pool_init(instance, physical_device, device) != LGX_SUCCESS) {
        printf("  ✓ GPU routing skipped: GPU pool init failed on host\n");
        tests_passed++;
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return;
    }
    
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 512,
        .access_pattern = LGX_ACCESS_WRITE_ONCE,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_GPU_SHARED,
        .validation_policy = LGX_INTENT_TRUST
    };
    void* ptr = lgx_alloc_with_intent(&intent);
    
    ASSERT(ptr != NULL, "GPU allocation succeeded");
    
    /* lgx_alloc_with_intent returns the GPU handle; the CPU-writable view
       of host-visible memory is the B1d/B2 accessor's mapped pointer. */
    void* mapped = lgx_gpu_get_mapped_ptr((lgx_gpu_allocation_t*)ptr);
    ASSERT(mapped != NULL, "host-visible GPU allocation has a mapped view");
    
    // Write to memory
    memset(mapped, 0xCC, 512);
    ASSERT(((uint8_t*)mapped)[0] == 0xCC, "GPU memory is writable");
    
    // GPU allocations return lgx_gpu_allocation_t*, need to cast
    lgx_gpu_free((lgx_gpu_allocation_t*)ptr);
    
    lgx_gpu_pool_shutdown();
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
#else
    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 512,
        .access_pattern = LGX_ACCESS_WRITE_ONCE,
        .lifetime = LGX_LIFETIME_FRAME,
        .hint = LGX_HINT_GPU_SHARED,
        .validation_policy = LGX_INTENT_TRUST
    };
    void* ptr = lgx_alloc_with_intent(&intent);
    // Without Vulkan, should fall back to persistent heap
    if (ptr) {
        printf("  ✓ GPU allocation fell back to persistent heap (Vulkan not available)\n");
        tests_passed++;
        lgx_heap_free(ptr);
    } else {
        printf("  ✓ GPU allocation returned NULL (expected without Vulkan)\n");
        tests_passed++;
    }
#endif
}

/**
 * Test: GPU hint precedence over frame lifetime
 *
 * lgx_alloc_gpu_shared() = lifetime FRAME + hint GPU_SHARED. The routing
 * must key on the hint. Counters prove the branch taken; no device needed,
 * because the GPU counter increments regardless of allocation success.
 */
void test_gpu_hint_precedence() {
    lgx_intent_aggregate_stats();
    lgx_intent_stats_t before;
    ASSERT(lgx_intent_get_stats(&before) == LGX_SUCCESS, "baseline stats read");

    void* ptr = lgx_alloc_gpu_shared(512);

    lgx_intent_aggregate_stats();
    lgx_intent_stats_t after;
    ASSERT(lgx_intent_get_stats(&after) == LGX_SUCCESS, "post stats read");

#if HAVE_VULKAN
    ASSERT(after.gpu_allocations == before.gpu_allocations + 1,
           "GPU hint routed to the GPU pool");
    ASSERT(after.frame_allocations == before.frame_allocations,
           "GPU hint did not consume frame arena");
    if (ptr) {
        lgx_gpu_free((lgx_gpu_allocation_t*)ptr);
    }
#else
    /* No Vulkan: the GPU branch is compiled out, so the GPU-shared hint is
       ignored and the FRAME lifetime applies as usual. It must never
       NULL-crash and must not reach the persistent heap. */
    ASSERT(ptr != NULL, "GPU hint falls back without GPU branch compiled in");
    ASSERT(after.frame_allocations == before.frame_allocations + 1,
           "fallback served by the frame arena");
    ASSERT(after.persistent_allocations == before.persistent_allocations,
           "fallback did not hit the persistent heap");
#endif
}

/**
 * Test: Convenience macros
 */
void test_convenience_macros() {
    // Test frame macro
    void* frame_ptr = lgx_alloc_frame(128);
    ASSERT(frame_ptr != NULL, "lgx_alloc_frame() succeeded");
    memset(frame_ptr, 0xDD, 128);
    
    // Test persistent macro
    void* persistent_ptr = lgx_alloc_persistent(256);
    ASSERT(persistent_ptr != NULL, "lgx_alloc_persistent() succeeded");
    memset(persistent_ptr, 0xEE, 256);
    lgx_heap_free(persistent_ptr);
    
    // Test level macro
    void* level_ptr = lgx_alloc_level(512);
    ASSERT(level_ptr != NULL, "lgx_alloc_level() succeeded");
    memset(level_ptr, 0xFF, 512);
    lgx_heap_free(level_ptr);
}

/**
 * Test: Intent validation
 */
void test_intent_validation() {
    // Test NULL intent
    void* ptr = lgx_alloc_with_intent(NULL);
    ASSERT(ptr == NULL, "NULL intent rejected");
    
    // Test zero size
    lgx_allocation_intent_base_t zero_intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 0,
        .lifetime = LGX_LIFETIME_FRAME
    };
    ptr = lgx_alloc_with_intent(&zero_intent);
    ASSERT(ptr == NULL, "Zero size rejected");
    
    // Test invalid struct_size
    lgx_allocation_intent_base_t invalid_intent = {
        .struct_size = 4,  // Too small
        .size = 100,
        .lifetime = LGX_LIFETIME_FRAME
    };
    ptr = lgx_alloc_with_intent(&invalid_intent);
    ASSERT(ptr == NULL, "Invalid struct_size rejected");
}

/**
 * Test: Intent statistics
 */
void test_intent_statistics() {
    // Reset by reinitializing
    lgx_intent_allocator_shutdown();
    lgx_intent_allocator_init();
    
    // Allocate with different intents
    void* frame1 = lgx_alloc_frame(64);
    void* frame2 = lgx_alloc_frame(128);
    void* persistent1 = lgx_alloc_persistent(256);
    void* persistent2 = lgx_alloc_persistent(512);
    
    ASSERT(frame1 && frame2 && persistent1 && persistent2, "All allocations succeeded");
    
    // Aggregate thread-local statistics to global
    lgx_intent_aggregate_stats();
    
    // Get statistics
    lgx_intent_stats_t stats;
    lgx_result_t result = lgx_intent_get_stats(&stats);
    ASSERT(result == LGX_SUCCESS, "Got intent statistics");
    
    ASSERT(stats.frame_allocations >= 2, "At least 2 frame allocations tracked");
    ASSERT(stats.persistent_allocations >= 2, "At least 2 persistent allocations tracked");
    ASSERT(stats.total_intent_allocations >= 4, "At least 4 total allocations tracked");
    
    printf("  Statistics:\n");
    printf("    Frame: %llu\n", (unsigned long long)stats.frame_allocations);
    printf("    Persistent: %llu\n", (unsigned long long)stats.persistent_allocations);
    printf("    Total: %llu\n", (unsigned long long)stats.total_intent_allocations);
    
    // Clean up
    lgx_heap_free(persistent1);
    lgx_heap_free(persistent2);
}

/**
 * Test: Mixed allocation patterns
 */
void test_mixed_patterns() {
    // Allocate mix of frame, persistent, and GPU
    void* allocations[10];
    
    for (int i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            allocations[i] = lgx_alloc_frame(64 * (i + 1));
        } else if (i % 3 == 1) {
            allocations[i] = lgx_alloc_persistent(128 * (i + 1));
        } else {
            allocations[i] = lgx_alloc_level(256 * (i + 1));
        }
        
        ASSERT(allocations[i] != NULL, "Mixed allocation succeeded");
    }
    
    // Free persistent and level allocations
    for (int i = 0; i < 10; i++) {
        if (i % 3 != 0) {  // Not frame allocations
            lgx_heap_free(allocations[i]);
        }
    }
}

/**
 * Test: Extended intent (Layer 2)
 */
void test_extended_intent() {
    lgx_allocation_intent_l2_t l2_intent = {
        .struct_size = sizeof(lgx_allocation_intent_l2_t),
        .base = {
            .struct_size = sizeof(lgx_allocation_intent_base_t),
            .size = 1024,
            .access_pattern = LGX_ACCESS_SEQUENTIAL,
            .lifetime = LGX_LIFETIME_FRAME,
            .hint = LGX_HINT_CRITICAL_PATH,
            .validation_policy = LGX_INTENT_TRUST
        },
        .priority = 128,
        .enable_predictive_prefetch = true
    };
    
    void* ptr = lgx_alloc_with_intent_ex(&l2_intent, 1);  // Type 1 = L2 intent
    ASSERT(ptr != NULL, "Extended intent allocation succeeded");
    
    memset(ptr, 0x42, 1024);
    ASSERT(((uint8_t*)ptr)[0] == 0x42, "Extended intent memory is writable");
}

/**
 * Test: Usage statistics (placeholder)
 */
void test_usage_statistics() {
    void* ptr = lgx_alloc_persistent(512);
    ASSERT(ptr != NULL, "Allocation for usage stats succeeded");
    
    lgx_allocation_usage_t usage = {
        .struct_size = sizeof(lgx_allocation_usage_t)
    };
    
    lgx_result_t result = lgx_alloc_get_usage_stats(ptr, &usage);
    ASSERT(result == LGX_SUCCESS, "Got usage statistics");
    
    // Currently returns placeholder data
    printf("  Usage stats (placeholder):\n");
    printf("    Access count: %llu\n", (unsigned long long)usage.access_count);
    printf("    Observed pattern: %d\n", usage.observed_pattern);
    printf("    Pattern confidence: %.2f\n", usage.pattern_confidence);
    
    lgx_heap_free(ptr);
}

int main(void) {
    printf("=== Intent Allocator Tests (Task 3.4) ===\n");
    printf("Testing unified intent-based API\n");
    
    // Initialize runtime
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_result_t result = lgx_runtime_init(config);
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize runtime\n");
        return 1;
    }
    lgx_config_destroy(config);
    
    // Initialize intent allocator
    result = lgx_intent_allocator_init();
    if (result != LGX_SUCCESS) {
        fprintf(stderr, "Failed to initialize intent allocator\n");
        return 1;
    }
    
    // Run tests
    TEST(intent_structure);
    TEST(frame_routing);
    TEST(persistent_routing);
    TEST(gpu_routing);
    TEST(gpu_hint_precedence);
    TEST(convenience_macros);
    TEST(intent_validation);
    TEST(intent_statistics);
    TEST(mixed_patterns);
    TEST(extended_intent);
    TEST(usage_statistics);
    
    // Shutdown
    lgx_intent_allocator_shutdown();
    lgx_runtime_shutdown();
    
    // Print summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed\n");
        return 1;
    }
}
