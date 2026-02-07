/**
 * LGX GPU Capability Detection - Task 3.5.2.3
 * 
 * Detects GPU hardware capabilities and selects optimal allocation strategy.
 * Adapts memory pool configuration based on available GPU memory types.
 */

#define _GNU_SOURCE
#include "lgx_runtime_internal.h"
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <string.h>

/**
 * Detect GPU capabilities from Vulkan physical device
 */
lgx_result_t lgx_gpu_detect_capabilities(VkPhysicalDevice physical_device, 
                                         lgx_gpu_capabilities_t* caps) {
    if (!physical_device || !caps) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    memset(caps, 0, sizeof(lgx_gpu_capabilities_t));
    
    // Get memory properties
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    
    // Scan memory types
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        VkMemoryType type = mem_props.memoryTypes[i];
        VkMemoryHeap heap = mem_props.memoryHeaps[type.heapIndex];
        VkMemoryPropertyFlags flags = type.propertyFlags;
        
        // Device-local memory (GPU-only, fastest)
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            if (!caps->supports_device_local || heap.size > caps->max_device_local_mb * 1024 * 1024) {
                caps->supports_device_local = true;
                caps->max_device_local_mb = heap.size / (1024 * 1024);
                caps->device_local_heap_index = type.heapIndex;
            }
        }
        
        // Host-visible memory (CPU-writable, GPU-readable)
        if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            caps->supports_host_visible = true;
            caps->max_host_visible_mb += heap.size / (1024 * 1024);
            caps->host_visible_heap_index = type.heapIndex;
            
            if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
                caps->supports_host_cached = true;
            }
            
            if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
                caps->supports_host_coherent = true;
            }
        }
    }
    
    // Detect ReBAR: Host-visible memory >= 80% of device-local memory
    if (caps->supports_host_visible && caps->supports_device_local) {
        float ratio = (float)caps->max_host_visible_mb / (float)caps->max_device_local_mb;
        caps->supports_resizable_bar = (ratio >= 0.8f);
    }
    
    return LGX_SUCCESS;
}

/**
 * Select optimal allocation strategy based on capabilities
 */
lgx_gpu_allocation_strategy_t lgx_gpu_select_strategy(const lgx_gpu_capabilities_t* caps) {
    if (!caps) {
        return LGX_GPU_STRATEGY_FALLBACK;
    }
    
    // Best case: ReBAR with large host-visible memory (8GB+)
    if (caps->supports_resizable_bar && caps->max_host_visible_mb > 8192) {
        return LGX_GPU_STRATEGY_RESIZABLE_BAR;
    }
    
    // Good case: Device-local memory available (2GB+)
    if (caps->supports_device_local && caps->max_device_local_mb > 2048) {
        return LGX_GPU_STRATEGY_DEVICE_LOCAL;
    }
    
    // Acceptable: Host-visible with caching
    if (caps->supports_host_visible && caps->supports_host_cached) {
        return LGX_GPU_STRATEGY_HOST_VISIBLE;
    }
    
    // Fallback: Whatever is available
    return LGX_GPU_STRATEGY_FALLBACK;
}

/**
 * Get recommended pool size based on strategy
 */
size_t lgx_gpu_get_recommended_pool_size(lgx_gpu_allocation_strategy_t strategy,
                                         const lgx_gpu_capabilities_t* caps) {
    if (!caps) {
        return 128 * 1024 * 1024;  // 128MB minimum
    }
    
    switch (strategy) {
        case LGX_GPU_STRATEGY_DEVICE_LOCAL:
            // Use 50% of available device-local memory
            return (caps->max_device_local_mb * 1024 * 1024) / 2;
            
        case LGX_GPU_STRATEGY_HOST_VISIBLE:
            // Use 25% of available host-visible memory
            return (caps->max_host_visible_mb * 1024 * 1024) / 4;
            
        case LGX_GPU_STRATEGY_RESIZABLE_BAR:
            // Use 75% of available host-visible memory (ReBAR allows more)
            return (caps->max_host_visible_mb * 1024 * 1024) * 3 / 4;
            
        case LGX_GPU_STRATEGY_FALLBACK:
            // Conservative: 128MB
            return 128 * 1024 * 1024;
    }
    
    return 128 * 1024 * 1024;
}

/**
 * Get memory type flags based on strategy
 */
VkMemoryPropertyFlags lgx_gpu_get_memory_type_flags(lgx_gpu_allocation_strategy_t strategy) {
    switch (strategy) {
        case LGX_GPU_STRATEGY_DEVICE_LOCAL:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            
        case LGX_GPU_STRATEGY_HOST_VISIBLE:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                   VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            
        case LGX_GPU_STRATEGY_RESIZABLE_BAR:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | 
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            
        case LGX_GPU_STRATEGY_FALLBACK:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}

/**
 * Print GPU capabilities (for debugging)
 */
void lgx_gpu_print_capabilities(const lgx_gpu_capabilities_t* caps) {
    if (!caps) {
        return;
    }
    
    printf("[LGX INFO] GPU Capabilities:\n");
    printf("  Device-local: %s (%zu MB)\n",
           caps->supports_device_local ? "YES" : "NO",
           caps->max_device_local_mb);
    printf("  Host-visible: %s (%zu MB)\n",
           caps->supports_host_visible ? "YES" : "NO",
           caps->max_host_visible_mb);
    printf("  Host-cached: %s\n",
           caps->supports_host_cached ? "YES" : "NO");
    printf("  Host-coherent: %s\n",
           caps->supports_host_coherent ? "YES" : "NO");
    printf("  Resizable BAR: %s\n",
           caps->supports_resizable_bar ? "YES" : "NO");
}

/**
 * Print selected strategy (for debugging)
 */
void lgx_gpu_print_strategy(lgx_gpu_allocation_strategy_t strategy) {
    const char* strategy_name = "UNKNOWN";
    
    switch (strategy) {
        case LGX_GPU_STRATEGY_DEVICE_LOCAL:
            strategy_name = "DEVICE_LOCAL (optimal)";
            break;
        case LGX_GPU_STRATEGY_HOST_VISIBLE:
            strategy_name = "HOST_VISIBLE (acceptable)";
            break;
        case LGX_GPU_STRATEGY_RESIZABLE_BAR:
            strategy_name = "RESIZABLE_BAR (best)";
            break;
        case LGX_GPU_STRATEGY_FALLBACK:
            strategy_name = "FALLBACK (minimum)";
            break;
    }
    
    printf("[LGX INFO] Selected GPU strategy: %s\n", strategy_name);
}
