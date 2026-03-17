/**
 * LGX Graphics Module v1.2 — Core Implementation
 * Thin Vulkan wrapper: device, resources, pipelines, shaders, cache.
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under Apache License 2.0
 */

#define _GNU_SOURCE
#include "lgx_graphics.h"
#include "lgx_runtime.h"

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LGX_GFX_MAX_FRAMES_IN_FLIGHT 3
#define LGX_GFX_SHADER_CACHE_MAGIC   0x4C475853  /* "LGXS" */

struct lgx_gfx_device {
    VkInstance           instance;
    VkPhysicalDevice     physical_device;
    VkDevice             device;
    VkQueue              graphics_queue;
    VkQueue              transfer_queue;
    uint32_t             graphics_queue_family;
    uint32_t             transfer_queue_family;
    VkCommandPool        command_pool;
    VkPipelineCache      pipeline_cache;
    VkDebugUtilsMessengerEXT debug_messenger;

    /* GPU info cache */
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties mem_properties;

    /* Shader cache config */
    char                 cache_dir[512];
    bool                 disk_cache_enabled;
    size_t               max_cache_size;

    /* Statistics */
    lgx_gfx_stats_t      stats;
    bool                 validation_enabled;
};

struct lgx_gfx_swapchain {
    lgx_gfx_device_t*   device;
    VkSurfaceKHR         surface;
    VkSwapchainKHR       swapchain;
    VkImage*             images;
    VkImageView*         image_views;
    VkFramebuffer*       framebuffers;
    uint32_t             image_count;
    uint32_t             width;
    uint32_t             height;
    VkFormat             format;
    VkSemaphore          image_available[LGX_GFX_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore          render_finished[LGX_GFX_MAX_FRAMES_IN_FLIGHT];
    VkFence              in_flight[LGX_GFX_MAX_FRAMES_IN_FLIGHT];
    uint32_t             current_frame;
    uint32_t             current_image;
};

struct lgx_gfx_cmd_buffer {
    lgx_gfx_device_t*   device;
    VkCommandBuffer      vk_cmd;
    bool                 recording;
};

struct lgx_gfx_pipeline {
    lgx_gfx_device_t*   device;
    VkPipeline           vk_pipeline;
    VkPipelineLayout     vk_layout;
};

struct lgx_gfx_renderpass {
    lgx_gfx_device_t*   device;
    VkRenderPass         vk_renderpass;
    uint32_t             attachment_count;
};

struct lgx_gfx_buffer {
    lgx_gfx_device_t*   device;
    VkBuffer             vk_buffer;
    VkDeviceMemory       vk_memory;
    size_t               size;
    uint32_t             usage;
    bool                 host_visible;
    void*                mapped;
};

struct lgx_gfx_image {
    lgx_gfx_device_t*   device;
    VkImage              vk_image;
    VkDeviceMemory       vk_memory;
    VkImageView          vk_view;
    uint32_t             width;
    uint32_t             height;
    VkFormat             vk_format;
};

struct lgx_gfx_shader {
    lgx_gfx_device_t*   device;
    VkShaderModule       vk_module;
    lgx_gfx_shader_stage_t stage;
    char                 entry_point[64];
};

struct lgx_gfx_descriptor_set {
    lgx_gfx_device_t*     device;
    VkDescriptorPool      vk_pool;
    VkDescriptorSetLayout vk_layout;
    VkDescriptorSet       vk_set;
    VkPipelineLayout      vk_pipeline_layout;  /* Layout that includes this set */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Format Conversion Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static VkFormat lgx_to_vk_format(lgx_gfx_format_t fmt) {
    switch (fmt) {
        case LGX_GFX_FORMAT_RGBA8_UNORM:   return VK_FORMAT_R8G8B8A8_UNORM;
        case LGX_GFX_FORMAT_RGBA8_SRGB:    return VK_FORMAT_R8G8B8A8_SRGB;
        case LGX_GFX_FORMAT_BGRA8_UNORM:   return VK_FORMAT_B8G8R8A8_UNORM;
        case LGX_GFX_FORMAT_BGRA8_SRGB:    return VK_FORMAT_B8G8R8A8_SRGB;
        case LGX_GFX_FORMAT_R32_SFLOAT:    return VK_FORMAT_R32_SFLOAT;
        case LGX_GFX_FORMAT_RG32_SFLOAT:   return VK_FORMAT_R32G32_SFLOAT;
        case LGX_GFX_FORMAT_RGB32_SFLOAT:  return VK_FORMAT_R32G32B32_SFLOAT;
        case LGX_GFX_FORMAT_RGBA32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case LGX_GFX_FORMAT_RGBA16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case LGX_GFX_FORMAT_D32_SFLOAT:    return VK_FORMAT_D32_SFLOAT;
        case LGX_GFX_FORMAT_D24_S8:        return VK_FORMAT_D24_UNORM_S8_UINT;
        default:                            return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static uint32_t find_memory_type(lgx_gfx_device_t* dev, uint32_t type_filter,
                                  VkMemoryPropertyFlags props) {
    for (uint32_t i = 0; i < dev->mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1u << i)) &&
            (dev->mem_properties.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}


/** Create a host-visible staging buffer, write data to it */
static VkBuffer create_staging_buffer(lgx_gfx_device_t* dev, const void* data,
                                       size_t size, VkDeviceMemory* out_mem) {
    VkBufferCreateInfo bi = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer staging;
    if (vkCreateBuffer(dev->device, &bi, NULL, &staging) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev->device, staging, &req);
    uint32_t mt = find_memory_type(dev, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mt == UINT32_MAX) {
        vkDestroyBuffer(dev->device, staging, NULL);
        return VK_NULL_HANDLE;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = mt,
    };
    if (vkAllocateMemory(dev->device, &mai, NULL, out_mem) != VK_SUCCESS) {
        vkDestroyBuffer(dev->device, staging, NULL);
        return VK_NULL_HANDLE;
    }
    vkBindBufferMemory(dev->device, staging, *out_mem, 0);

    void* mapped;
    vkMapMemory(dev->device, *out_mem, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(dev->device, *out_mem);
    return staging;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image Layout Transition Helper
 * ═══════════════════════════════════════════════════════════════════════════ */

static void transition_image_layout(VkCommandBuffer cmd, VkImage image,
                                     VkImageLayout old_layout,
                                     VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0, .levelCount = 1,
            .baseArrayLayer = 0, .layerCount = 1,
        },
    };

    VkPipelineStageFlags src_stage, dst_stage;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0,
                         0, NULL, 0, NULL, 1, &barrier);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Debug Callback
 * ═══════════════════════════════════════════════════════════════════════════ */

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user_data) {
    (void)type; (void)user_data;
    const char* level = "INFO";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) level = "ERROR";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) level = "WARN";
    fprintf(stderr, "[LGX GFX %s] %s\n", level, data->pMessage);
    return VK_FALSE;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Device API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_gfx_device_t* lgx_gfx_device_create(const lgx_gfx_device_config_t* config) {
    lgx_gfx_device_t* dev = calloc(1, sizeof(*dev));
    if (!dev) return NULL;

    /* Defaults */
    const char* app_name = config ? config->app_name : "LGX Application";
    bool validation = config ? config->enable_validation : false;
    lgx_gfx_gpu_preference_t pref = config ? config->gpu_preference : LGX_GFX_GPU_PREFER_DISCRETE;
    dev->validation_enabled = validation;

    /* --- VkInstance --- */
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name,
        .applicationVersion = config ? config->app_version : VK_MAKE_VERSION(1,0,0),
        .pEngineName = "LGX Runtime Platform",
        .engineVersion = VK_MAKE_VERSION(LGX_GRAPHICS_VERSION_MAJOR,
                                          LGX_GRAPHICS_VERSION_MINOR,
                                          LGX_GRAPHICS_VERSION_PATCH),
        .apiVersion = VK_API_VERSION_1_3,
    };

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_XLIB_KHR
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    /* Count valid extensions (skip NULLs) */
    uint32_t ext_count = 0;
    for (uint32_t i = 0; i < sizeof(extensions)/sizeof(extensions[0]); i++) {
        if (extensions[i]) ext_count = i + 1;
    }

    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    VkInstanceCreateInfo inst_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = validation ? 1 : 0,
        .ppEnabledLayerNames = validation ? layers : NULL,
    };

    VkResult vr = vkCreateInstance(&inst_info, NULL, &dev->instance);
    if (vr != VK_SUCCESS) {
        fprintf(stderr, "[LGX GFX] vkCreateInstance failed: %d\n", vr);
        free(dev);
        return NULL;
    }

    /* Debug messenger */
    if (validation) {
        PFN_vkCreateDebugUtilsMessengerEXT create_dbg =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                dev->instance, "vkCreateDebugUtilsMessengerEXT");
        if (create_dbg) {
            VkDebugUtilsMessengerCreateInfoEXT dbg_info = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = debug_callback,
            };
            create_dbg(dev->instance, &dbg_info, NULL, &dev->debug_messenger);
        }
    }

    /* --- Physical Device Selection --- */
    uint32_t gpu_count = 0;
    vkEnumeratePhysicalDevices(dev->instance, &gpu_count, NULL);
    if (gpu_count == 0) {
        fprintf(stderr, "[LGX GFX] No Vulkan-capable GPUs found\n");
        lgx_gfx_device_destroy(dev);
        return NULL;
    }

    VkPhysicalDevice* gpus = calloc(gpu_count, sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(dev->instance, &gpu_count, gpus);

    /* Score GPUs and pick best match */
    dev->physical_device = gpus[0];  /* Fallback */
    for (uint32_t i = 0; i < gpu_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(gpus[i], &props);
        if (pref == LGX_GFX_GPU_PREFER_DISCRETE &&
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            dev->physical_device = gpus[i];
            break;
        }
        if (pref == LGX_GFX_GPU_PREFER_INTEGRATED &&
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            dev->physical_device = gpus[i];
            break;
        }
    }
    free(gpus);

    vkGetPhysicalDeviceProperties(dev->physical_device, &dev->properties);
    vkGetPhysicalDeviceMemoryProperties(dev->physical_device, &dev->mem_properties);

    printf("[LGX GFX] Selected GPU: %s\n", dev->properties.deviceName);

    /* --- Queue Families --- */
    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev->physical_device, &qf_count, NULL);
    VkQueueFamilyProperties* qf_props = calloc(qf_count, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(dev->physical_device, &qf_count, qf_props);

    dev->graphics_queue_family = UINT32_MAX;
    dev->transfer_queue_family = UINT32_MAX;
    for (uint32_t i = 0; i < qf_count; i++) {
        if ((qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            dev->graphics_queue_family == UINT32_MAX) {
            dev->graphics_queue_family = i;
        }
        if ((qf_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            dev->transfer_queue_family == UINT32_MAX) {
            dev->transfer_queue_family = i;
        }
    }
    free(qf_props);

    if (dev->graphics_queue_family == UINT32_MAX) {
        fprintf(stderr, "[LGX GFX] No graphics queue family found\n");
        lgx_gfx_device_destroy(dev);
        return NULL;
    }
    /* Fallback: use graphics queue for transfers if no dedicated transfer queue */
    if (dev->transfer_queue_family == UINT32_MAX)
        dev->transfer_queue_family = dev->graphics_queue_family;

    /* --- Logical Device --- */
    float priority = 1.0f;
    uint32_t unique_families[2] = { dev->graphics_queue_family, dev->transfer_queue_family };
    uint32_t family_count = (unique_families[0] == unique_families[1]) ? 1 : 2;

    VkDeviceQueueCreateInfo queue_infos[2];
    for (uint32_t i = 0; i < family_count; i++) {
        queue_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_families[i],
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
    }

    const char* dev_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceFeatures features = { .fillModeNonSolid = VK_TRUE };

    VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = family_count,
        .pQueueCreateInfos = queue_infos,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = dev_extensions,
        .pEnabledFeatures = &features,
    };

    vr = vkCreateDevice(dev->physical_device, &dev_info, NULL, &dev->device);
    if (vr != VK_SUCCESS) {
        fprintf(stderr, "[LGX GFX] vkCreateDevice failed: %d\n", vr);
        lgx_gfx_device_destroy(dev);
        return NULL;
    }

    vkGetDeviceQueue(dev->device, dev->graphics_queue_family, 0, &dev->graphics_queue);
    vkGetDeviceQueue(dev->device, dev->transfer_queue_family, 0, &dev->transfer_queue);

    /* --- Command Pool --- */
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = dev->graphics_queue_family,
    };
    vr = vkCreateCommandPool(dev->device, &pool_info, NULL, &dev->command_pool);
    if (vr != VK_SUCCESS) {
        fprintf(stderr, "[LGX GFX] vkCreateCommandPool failed: %d\n", vr);
        lgx_gfx_device_destroy(dev);
        return NULL;
    }

    /* --- Pipeline Cache --- */
    VkPipelineCacheCreateInfo cache_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    };
    vkCreatePipelineCache(dev->device, &cache_info, NULL, &dev->pipeline_cache);

    /* Init stats */
    memset(&dev->stats, 0, sizeof(dev->stats));
    dev->stats.struct_size = sizeof(lgx_gfx_stats_t);
    strncpy(dev->stats.gpu_name, dev->properties.deviceName, sizeof(dev->stats.gpu_name) - 1);
    dev->stats.vulkan_api_version = dev->properties.apiVersion;

    /* Calculate total device memory */
    for (uint32_t i = 0; i < dev->mem_properties.memoryHeapCount; i++) {
        if (dev->mem_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            dev->stats.device_memory_bytes += dev->mem_properties.memoryHeaps[i].size;
        }
    }

    snprintf(dev->cache_dir, sizeof(dev->cache_dir), "%s/.cache/lgx/shader_cache",
             getenv("HOME") ? getenv("HOME") : "/tmp");

    printf("[LGX GFX] Device created — Vulkan %u.%u.%u, %llu MB VRAM\n",
           VK_VERSION_MAJOR(dev->properties.apiVersion),
           VK_VERSION_MINOR(dev->properties.apiVersion),
           VK_VERSION_PATCH(dev->properties.apiVersion),
           (unsigned long long)(dev->stats.device_memory_bytes / (1024*1024)));

    return dev;
}

void lgx_gfx_device_destroy(lgx_gfx_device_t* dev) {
    if (!dev) return;
    if (dev->device) vkDeviceWaitIdle(dev->device);
    if (dev->pipeline_cache) vkDestroyPipelineCache(dev->device, dev->pipeline_cache, NULL);
    if (dev->command_pool) vkDestroyCommandPool(dev->device, dev->command_pool, NULL);
    if (dev->device) vkDestroyDevice(dev->device, NULL);
    if (dev->debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_dbg =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                dev->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_dbg) destroy_dbg(dev->instance, dev->debug_messenger, NULL);
    }
    if (dev->instance) vkDestroyInstance(dev->instance, NULL);
    free(dev);
}

lgx_result_t lgx_gfx_device_get_gpu_info(lgx_gfx_device_t* dev, lgx_gfx_gpu_info_t* info) {
    if (!dev || !info) return LGX_ERROR_INVALID_PARAM;
    memset(info, 0, sizeof(*info));
    info->struct_size = sizeof(lgx_gfx_gpu_info_t);
    strncpy(info->gpu_name, dev->properties.deviceName, sizeof(info->gpu_name) - 1);
    snprintf(info->driver_version, sizeof(info->driver_version), "%u.%u.%u",
             VK_VERSION_MAJOR(dev->properties.driverVersion),
             VK_VERSION_MINOR(dev->properties.driverVersion),
             VK_VERSION_PATCH(dev->properties.driverVersion));
    info->vulkan_api_version = dev->properties.apiVersion;
    info->max_image_dimension_2d = dev->properties.limits.maxImageDimension2D;
    info->max_framebuffer_width = dev->properties.limits.maxFramebufferWidth;
    info->max_framebuffer_height = dev->properties.limits.maxFramebufferHeight;
    info->is_discrete = (dev->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    info->device_memory_bytes = dev->stats.device_memory_bytes;

    /* Sum host-visible memory */
    for (uint32_t i = 0; i < dev->mem_properties.memoryHeapCount; i++) {
        if (!(dev->mem_properties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) {
            info->host_visible_memory_bytes += dev->mem_properties.memoryHeaps[i].size;
        }
    }

    /* Check features */
    VkPhysicalDeviceFeatures feats;
    vkGetPhysicalDeviceFeatures(dev->physical_device, &feats);
    info->supports_geometry_shader = feats.geometryShader;
    info->supports_tessellation = feats.tessellationShader;
    info->supports_compute = true;  /* Vulkan 1.0+ always supports compute */
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_device_wait_idle(lgx_gfx_device_t* dev) {
    if (!dev || !dev->device) return LGX_ERROR_INVALID_PARAM;
    VkResult vr = vkDeviceWaitIdle(dev->device);
    return (vr == VK_SUCCESS) ? LGX_SUCCESS : LGX_ERROR_INVALID_PARAM;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Buffer API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_gfx_buffer_t* lgx_gfx_buffer_create(lgx_gfx_device_t* dev, size_t size,
                                          uint32_t usage, bool host_visible) {
    if (!dev || size == 0) return NULL;

    lgx_gfx_buffer_t* buf = calloc(1, sizeof(*buf));
    if (!buf) return NULL;
    buf->device = dev;
    buf->size = size;
    buf->usage = usage;
    buf->host_visible = host_visible;

    /* Map LGX usage to Vulkan usage flags */
    VkBufferUsageFlags vk_usage = 0;
    if (usage & LGX_GFX_BUFFER_VERTEX)   vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage & LGX_GFX_BUFFER_INDEX)    vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage & LGX_GFX_BUFFER_UNIFORM)  vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage & LGX_GFX_BUFFER_STORAGE)  vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (usage & LGX_GFX_BUFFER_TRANSFER) vk_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    /* Always allow transfer dst for uploads */
    vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = vk_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult vr = vkCreateBuffer(dev->device, &buf_info, NULL, &buf->vk_buffer);
    if (vr != VK_SUCCESS) { free(buf); return NULL; }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(dev->device, buf->vk_buffer, &mem_req);

    VkMemoryPropertyFlags mem_props = host_visible
        ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t mem_type = find_memory_type(dev, mem_req.memoryTypeBits, mem_props);
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(dev->device, buf->vk_buffer, NULL);
        free(buf);
        return NULL;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    vr = vkAllocateMemory(dev->device, &alloc_info, NULL, &buf->vk_memory);
    if (vr != VK_SUCCESS) {
        vkDestroyBuffer(dev->device, buf->vk_buffer, NULL);
        free(buf);
        return NULL;
    }

    vkBindBufferMemory(dev->device, buf->vk_buffer, buf->vk_memory, 0);
    dev->stats.active_buffers++;
    return buf;
}

void lgx_gfx_buffer_destroy(lgx_gfx_buffer_t* buf) {
    if (!buf) return;
    if (buf->mapped) lgx_gfx_buffer_unmap(buf);
    if (buf->vk_buffer) vkDestroyBuffer(buf->device->device, buf->vk_buffer, NULL);
    if (buf->vk_memory) vkFreeMemory(buf->device->device, buf->vk_memory, NULL);
    if (buf->device->stats.active_buffers > 0) buf->device->stats.active_buffers--;
    free(buf);
}

void* lgx_gfx_buffer_map(lgx_gfx_buffer_t* buf) {
    if (!buf || !buf->host_visible || buf->mapped) return buf ? buf->mapped : NULL;
    vkMapMemory(buf->device->device, buf->vk_memory, 0, buf->size, 0, &buf->mapped);
    return buf->mapped;
}

void lgx_gfx_buffer_unmap(lgx_gfx_buffer_t* buf) {
    if (!buf || !buf->mapped) return;
    vkUnmapMemory(buf->device->device, buf->vk_memory);
    buf->mapped = NULL;
}

lgx_result_t lgx_gfx_buffer_upload(lgx_gfx_buffer_t* buf, const void* data,
                                    size_t size, size_t offset) {
    if (!buf || !data || offset + size > buf->size) return LGX_ERROR_INVALID_PARAM;
    if (buf->host_visible) {
        void* mapped = lgx_gfx_buffer_map(buf);
        if (!mapped) return LGX_ERROR_OUT_OF_MEMORY;
        memcpy((char*)mapped + offset, data, size);
        lgx_gfx_buffer_unmap(buf);
        return LGX_SUCCESS;
    }

    /* Staging buffer path for device-local buffers */
    VkDeviceMemory staging_mem;
    VkBuffer staging = create_staging_buffer(buf->device, data, size, &staging_mem);
    if (!staging) return LGX_ERROR_OUT_OF_MEMORY;

    /* One-shot copy command */
    VkCommandBufferAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = buf->device->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd;
    VkResult vr = vkAllocateCommandBuffers(buf->device->device, &ai, &cmd);
    if (vr != VK_SUCCESS) {
        vkDestroyBuffer(buf->device->device, staging, NULL);
        vkFreeMemory(buf->device->device, staging_mem, NULL);
        return LGX_ERROR_OUT_OF_MEMORY;
    }

    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy region = { .srcOffset = 0, .dstOffset = offset, .size = size };
    vkCmdCopyBuffer(cmd, staging, buf->vk_buffer, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    vkQueueSubmit(buf->device->transfer_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(buf->device->transfer_queue);

    vkFreeCommandBuffers(buf->device->device, buf->device->command_pool, 1, &cmd);
    vkDestroyBuffer(buf->device->device, staging, NULL);
    vkFreeMemory(buf->device->device, staging_mem, NULL);
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Image API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_gfx_image_t* lgx_gfx_image_create(lgx_gfx_device_t* dev, uint32_t width,
                                        uint32_t height, lgx_gfx_format_t format,
                                        uint32_t usage) {
    if (!dev || width == 0 || height == 0) return NULL;

    lgx_gfx_image_t* img = calloc(1, sizeof(*img));
    if (!img) return NULL;
    img->device = dev;
    img->width = width;
    img->height = height;
    img->vk_format = lgx_to_vk_format(format);

    VkImageUsageFlags vk_usage = 0;
    if (usage & LGX_GFX_IMAGE_SAMPLED)      vk_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usage & LGX_GFX_IMAGE_COLOR_TARGET)  vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usage & LGX_GFX_IMAGE_DEPTH_TARGET)  vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usage & LGX_GFX_IMAGE_TRANSFER_SRC)  vk_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usage & LGX_GFX_IMAGE_TRANSFER_DST)  vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = img->vk_format,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = vk_usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(dev->device, &img_info, NULL, &img->vk_image) != VK_SUCCESS) {
        free(img); return NULL;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(dev->device, img->vk_image, &mem_req);

    uint32_t mem_type = find_memory_type(dev, mem_req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type == UINT32_MAX) {
        vkDestroyImage(dev->device, img->vk_image, NULL);
        free(img); return NULL;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    if (vkAllocateMemory(dev->device, &alloc_info, NULL, &img->vk_memory) != VK_SUCCESS) {
        vkDestroyImage(dev->device, img->vk_image, NULL);
        free(img); return NULL;
    }
    vkBindImageMemory(dev->device, img->vk_image, img->vk_memory, 0);

    /* Image view */
    bool is_depth = (format == LGX_GFX_FORMAT_D32_SFLOAT || format == LGX_GFX_FORMAT_D24_S8);
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img->vk_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = img->vk_format,
        .subresourceRange = {
            .aspectMask = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0, .levelCount = 1,
            .baseArrayLayer = 0, .layerCount = 1,
        },
    };
    if (vkCreateImageView(dev->device, &view_info, NULL, &img->vk_view) != VK_SUCCESS) {
        vkFreeMemory(dev->device, img->vk_memory, NULL);
        vkDestroyImage(dev->device, img->vk_image, NULL);
        free(img); return NULL;
    }

    dev->stats.active_images++;
    return img;
}

void lgx_gfx_image_destroy(lgx_gfx_image_t* img) {
    if (!img) return;
    if (img->vk_view)   vkDestroyImageView(img->device->device, img->vk_view, NULL);
    if (img->vk_image)  vkDestroyImage(img->device->device, img->vk_image, NULL);
    if (img->vk_memory) vkFreeMemory(img->device->device, img->vk_memory, NULL);
    if (img->device->stats.active_images > 0) img->device->stats.active_images--;
    free(img);
}

lgx_result_t lgx_gfx_image_upload(lgx_gfx_image_t* img, const void* pixels,
                                   size_t data_size) {
    if (!img || !pixels || data_size == 0) return LGX_ERROR_INVALID_PARAM;

    lgx_gfx_device_t* dev = img->device;

    /* Create staging buffer with pixel data */
    VkDeviceMemory staging_mem;
    VkBuffer staging = create_staging_buffer(dev, pixels, data_size, &staging_mem);
    if (!staging) return LGX_ERROR_OUT_OF_MEMORY;

    /* One-shot command: transition → copy → transition */
    VkCommandBufferAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = dev->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(dev->device, &ai, &cmd) != VK_SUCCESS) {
        vkDestroyBuffer(dev->device, staging, NULL);
        vkFreeMemory(dev->device, staging_mem, NULL);
        return LGX_ERROR_OUT_OF_MEMORY;
    }

    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &bi);

    /* Transition: UNDEFINED → TRANSFER_DST_OPTIMAL */
    transition_image_layout(cmd, img->vk_image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    /* Copy staging buffer → image */
    VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,    /* tightly packed */
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { img->width, img->height, 1 },
    };
    vkCmdCopyBufferToImage(cmd, staging, img->vk_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copy_region);

    /* Transition: TRANSFER_DST → SHADER_READ_ONLY */
    transition_image_layout(cmd, img->vk_image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    vkQueueSubmit(dev->transfer_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(dev->transfer_queue);

    vkFreeCommandBuffers(dev->device, dev->command_pool, 1, &cmd);
    vkDestroyBuffer(dev->device, staging, NULL);
    vkFreeMemory(dev->device, staging_mem, NULL);
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shader API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_gfx_shader_t* lgx_gfx_shader_create(lgx_gfx_device_t* dev,
                                          const uint32_t* spirv_code,
                                          size_t code_size,
                                          lgx_gfx_shader_stage_t stage,
                                          const char* entry_point) {
    if (!dev || !spirv_code || code_size == 0) return NULL;

    lgx_gfx_shader_t* shader = calloc(1, sizeof(*shader));
    if (!shader) return NULL;
    shader->device = dev;
    shader->stage = stage;
    strncpy(shader->entry_point, entry_point ? entry_point : "main",
            sizeof(shader->entry_point) - 1);

    VkShaderModuleCreateInfo mod_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size,
        .pCode = spirv_code,
    };
    if (vkCreateShaderModule(dev->device, &mod_info, NULL, &shader->vk_module) != VK_SUCCESS) {
        free(shader); return NULL;
    }

    dev->stats.active_shaders++;
    dev->stats.shader_cache_misses++;
    return shader;
}

lgx_gfx_shader_t* lgx_gfx_shader_load(lgx_gfx_device_t* dev,
                                        const char* filepath,
                                        lgx_gfx_shader_stage_t stage) {
    if (!dev || !filepath) return NULL;

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "[LGX GFX] Failed to open shader: %s\n", filepath);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 16 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }

    uint32_t* code = malloc((size_t)file_size);
    if (!code) { fclose(f); return NULL; }
    if (fread(code, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(code); fclose(f); return NULL;
    }
    fclose(f);

    lgx_gfx_shader_t* shader = lgx_gfx_shader_create(dev, code, (size_t)file_size,
                                                       stage, "main");
    free(code);
    return shader;
}

void lgx_gfx_shader_destroy(lgx_gfx_shader_t* shader) {
    if (!shader) return;
    if (shader->vk_module)
        vkDestroyShaderModule(shader->device->device, shader->vk_module, NULL);
    if (shader->device->stats.active_shaders > 0)
        shader->device->stats.active_shaders--;
    free(shader);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Render Pass API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_gfx_renderpass_t* lgx_gfx_renderpass_create(lgx_gfx_device_t* dev,
                                                  const lgx_gfx_renderpass_config_t* config) {
    if (!dev || !config || config->attachment_count == 0) return NULL;

    lgx_gfx_renderpass_t* rp = calloc(1, sizeof(*rp));
    if (!rp) return NULL;
    rp->device = dev;
    rp->attachment_count = config->attachment_count;

    /* Build Vulkan attachment descriptions */
    VkAttachmentDescription* vk_attachments = calloc(config->attachment_count,
                                                      sizeof(VkAttachmentDescription));
    VkAttachmentReference* color_refs = calloc(config->attachment_count,
                                                sizeof(VkAttachmentReference));
    VkAttachmentReference depth_ref = { .attachment = VK_ATTACHMENT_UNUSED };
    uint32_t color_count = 0;

    for (uint32_t i = 0; i < config->attachment_count; i++) {
        const lgx_gfx_attachment_desc_t* att = &config->attachments[i];
        vk_attachments[i] = (VkAttachmentDescription){
            .format = lgx_to_vk_format(att->format),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = (att->load_op == LGX_GFX_LOAD_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                      (att->load_op == LGX_GFX_LOAD_LOAD) ? VK_ATTACHMENT_LOAD_OP_LOAD :
                      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = (att->store_op == LGX_GFX_STORE_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                       VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = att->is_depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                         : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };
        if (att->is_depth) {
            depth_ref.attachment = i;
            depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        } else {
            color_refs[color_count].attachment = i;
            color_refs[color_count].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_count++;
        }
    }

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = color_count,
        .pColorAttachments = color_refs,
        .pDepthStencilAttachment = (depth_ref.attachment != VK_ATTACHMENT_UNUSED) ? &depth_ref : NULL,
    };

    VkSubpassDependency dep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = config->attachment_count,
        .pAttachments = vk_attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dep,
    };

    VkResult vr = vkCreateRenderPass(dev->device, &rp_info, NULL, &rp->vk_renderpass);
    free(vk_attachments);
    free(color_refs);

    if (vr != VK_SUCCESS) { free(rp); return NULL; }
    return rp;
}

void lgx_gfx_renderpass_destroy(lgx_gfx_renderpass_t* rp) {
    if (!rp) return;
    if (rp->vk_renderpass) vkDestroyRenderPass(rp->device->device, rp->vk_renderpass, NULL);
    free(rp);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Pipeline API
 * ═══════════════════════════════════════════════════════════════════════════ */

static VkShaderStageFlagBits lgx_to_vk_stage(lgx_gfx_shader_stage_t stage) {
    switch (stage) {
        case LGX_GFX_SHADER_VERTEX:   return VK_SHADER_STAGE_VERTEX_BIT;
        case LGX_GFX_SHADER_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case LGX_GFX_SHADER_COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

lgx_gfx_pipeline_t* lgx_gfx_pipeline_create(lgx_gfx_device_t* dev,
                                              const lgx_gfx_pipeline_config_t* config) {
    if (!dev || !config || !config->vertex_shader || !config->fragment_shader ||
        !config->renderpass) return NULL;

    lgx_gfx_pipeline_t* pipe = calloc(1, sizeof(*pipe));
    if (!pipe) return NULL;
    pipe->device = dev;

    /* Shader stages */
    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = lgx_to_vk_stage(config->vertex_shader->stage),
            .module = config->vertex_shader->vk_module,
            .pName = config->vertex_shader->entry_point,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = lgx_to_vk_stage(config->fragment_shader->stage),
            .module = config->fragment_shader->vk_module,
            .pName = config->fragment_shader->entry_point,
        },
    };

    /* Vertex input */
    VkVertexInputAttributeDescription* vk_attrs = NULL;
    if (config->attribute_count > 0 && config->attributes) {
        vk_attrs = calloc(config->attribute_count, sizeof(VkVertexInputAttributeDescription));
        for (uint32_t i = 0; i < config->attribute_count; i++) {
            vk_attrs[i].location = config->attributes[i].location;
            vk_attrs[i].binding = 0;
            vk_attrs[i].format = lgx_to_vk_format(config->attributes[i].format);
            vk_attrs[i].offset = config->attributes[i].offset;
        }
    }

    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = config->vertex_stride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = (config->vertex_stride > 0) ? 1 : 0,
        .pVertexBindingDescriptions = (config->vertex_stride > 0) ? &binding : NULL,
        .vertexAttributeDescriptionCount = config->attribute_count,
        .pVertexAttributeDescriptions = vk_attrs,
    };

    /* Topology */
    VkPrimitiveTopology topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (config->topology == LGX_GFX_TOPOLOGY_TRIANGLE_STRIP) topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    else if (config->topology == LGX_GFX_TOPOLOGY_LINE_LIST) topo = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    else if (config->topology == LGX_GFX_TOPOLOGY_POINT_LIST) topo = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineInputAssemblyStateCreateInfo assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = topo,
    };

    /* Dynamic viewport + scissor */
    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };

    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    /* Rasterization */
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = (config->polygon_mode == LGX_GFX_POLYGON_WIREFRAME) ?
                       VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = (config->cull_mode == LGX_GFX_CULL_BACK) ? VK_CULL_MODE_BACK_BIT :
                    (config->cull_mode == LGX_GFX_CULL_FRONT) ? VK_CULL_MODE_FRONT_BIT :
                    VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    /* Depth */
    VkPipelineDepthStencilStateCreateInfo depth = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = config->depth_test ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = config->depth_write ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };

    /* Blending */
    VkPipelineColorBlendAttachmentState blend_att = {
        .blendEnable = config->blend_enable ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_att,
    };

    /* Pipeline layout (empty for now) */
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    if (vkCreatePipelineLayout(dev->device, &layout_info, NULL, &pipe->vk_layout) != VK_SUCCESS) {
        free(vk_attrs); free(pipe); return NULL;
    }

    /* Create pipeline */
    VkGraphicsPipelineCreateInfo pipe_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &raster,
        .pMultisampleState = &ms,
        .pDepthStencilState = &depth,
        .pColorBlendState = &blend,
        .pDynamicState = &dynamic,
        .layout = pipe->vk_layout,
        .renderPass = config->renderpass->vk_renderpass,
        .subpass = 0,
    };

    VkResult vr = vkCreateGraphicsPipelines(dev->device, dev->pipeline_cache,
                                             1, &pipe_info, NULL, &pipe->vk_pipeline);
    free(vk_attrs);
    if (vr != VK_SUCCESS) {
        vkDestroyPipelineLayout(dev->device, pipe->vk_layout, NULL);
        free(pipe);
        return NULL;
    }

    dev->stats.active_pipelines++;
    return pipe;
}

void lgx_gfx_pipeline_destroy(lgx_gfx_pipeline_t* pipe) {
    if (!pipe) return;
    if (pipe->vk_pipeline) vkDestroyPipeline(pipe->device->device, pipe->vk_pipeline, NULL);
    if (pipe->vk_layout) vkDestroyPipelineLayout(pipe->device->device, pipe->vk_layout, NULL);
    if (pipe->device->stats.active_pipelines > 0) pipe->device->stats.active_pipelines--;
    free(pipe);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Command Buffer API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_gfx_cmd_buffer_t* lgx_gfx_cmd_begin(lgx_gfx_device_t* dev) {
    if (!dev) return NULL;

    lgx_gfx_cmd_buffer_t* cmd = calloc(1, sizeof(*cmd));
    if (!cmd) return NULL;
    cmd->device = dev;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = dev->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vkAllocateCommandBuffers(dev->device, &alloc_info, &cmd->vk_cmd) != VK_SUCCESS) {
        free(cmd); return NULL;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (vkBeginCommandBuffer(cmd->vk_cmd, &begin_info) != VK_SUCCESS) {
        vkFreeCommandBuffers(dev->device, dev->command_pool, 1, &cmd->vk_cmd);
        free(cmd); return NULL;
    }

    cmd->recording = true;
    return cmd;
}

lgx_result_t lgx_gfx_cmd_end(lgx_gfx_cmd_buffer_t* cmd) {
    if (!cmd || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    VkResult vr = vkEndCommandBuffer(cmd->vk_cmd);
    cmd->recording = false;
    return (vr == VK_SUCCESS) ? LGX_SUCCESS : LGX_ERROR_INVALID_PARAM;
}

lgx_result_t lgx_gfx_cmd_submit(lgx_gfx_device_t* dev, lgx_gfx_cmd_buffer_t* cmd) {
    if (!dev || !cmd) return LGX_ERROR_INVALID_PARAM;

    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd->vk_cmd,
    };
    VkResult vr = vkQueueSubmit(dev->graphics_queue, 1, &submit, VK_NULL_HANDLE);
    if (vr != VK_SUCCESS) return LGX_ERROR_INVALID_PARAM;

    vkQueueWaitIdle(dev->graphics_queue);
    vkFreeCommandBuffers(dev->device, dev->command_pool, 1, &cmd->vk_cmd);
    free(cmd);
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_begin_renderpass(lgx_gfx_cmd_buffer_t* cmd,
                                           lgx_gfx_renderpass_t* renderpass,
                                           lgx_gfx_swapchain_t* swapchain,
                                           uint32_t image_index,
                                           float clear_r, float clear_g,
                                           float clear_b, float clear_a) {
    if (!cmd || !renderpass || !swapchain || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    if (!swapchain->framebuffers || image_index >= swapchain->image_count)
        return LGX_ERROR_INVALID_PARAM;

    VkClearValue clear = { .color = {{ clear_r, clear_g, clear_b, clear_a }} };
    VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderpass->vk_renderpass,
        .framebuffer = swapchain->framebuffers[image_index],
        .renderArea = { .offset = {0, 0}, .extent = { swapchain->width, swapchain->height } },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    vkCmdBeginRenderPass(cmd->vk_cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_end_renderpass(lgx_gfx_cmd_buffer_t* cmd) {
    if (!cmd || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    vkCmdEndRenderPass(cmd->vk_cmd);
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_bind_pipeline(lgx_gfx_cmd_buffer_t* cmd, lgx_gfx_pipeline_t* pipe) {
    if (!cmd || !pipe || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    vkCmdBindPipeline(cmd->vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->vk_pipeline);
    cmd->device->stats.pipeline_switches++;
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_bind_vertex_buffer(lgx_gfx_cmd_buffer_t* cmd, lgx_gfx_buffer_t* buf) {
    if (!cmd || !buf || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd->vk_cmd, 0, 1, &buf->vk_buffer, &offset);
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_bind_index_buffer(lgx_gfx_cmd_buffer_t* cmd, lgx_gfx_buffer_t* buf) {
    if (!cmd || !buf || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    vkCmdBindIndexBuffer(cmd->vk_cmd, buf->vk_buffer, 0, VK_INDEX_TYPE_UINT32);
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_draw(lgx_gfx_cmd_buffer_t* cmd, uint32_t vertex_count,
                               uint32_t instance_count, uint32_t first_vertex,
                               uint32_t first_instance) {
    if (!cmd || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    vkCmdDraw(cmd->vk_cmd, vertex_count, instance_count, first_vertex, first_instance);
    cmd->device->stats.draw_calls++;
    cmd->device->stats.vertices_submitted += vertex_count * instance_count;
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_draw_indexed(lgx_gfx_cmd_buffer_t* cmd, uint32_t index_count,
                                       uint32_t instance_count, uint32_t first_index,
                                       int32_t vertex_offset, uint32_t first_instance) {
    if (!cmd || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    vkCmdDrawIndexed(cmd->vk_cmd, index_count, instance_count, first_index,
                     vertex_offset, first_instance);
    cmd->device->stats.draw_calls++;
    cmd->device->stats.vertices_submitted += index_count * instance_count;
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_set_viewport(lgx_gfx_cmd_buffer_t* cmd,
                                       float x, float y, float width, float height,
                                       float min_depth, float max_depth) {
    if (!cmd || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    VkViewport vp = { x, y, width, height, min_depth, max_depth };
    vkCmdSetViewport(cmd->vk_cmd, 0, 1, &vp);
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_cmd_set_scissor(lgx_gfx_cmd_buffer_t* cmd,
                                      int32_t x, int32_t y,
                                      uint32_t width, uint32_t height) {
    if (!cmd || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    VkRect2D scissor = { .offset = { x, y }, .extent = { width, height } };
    vkCmdSetScissor(cmd->vk_cmd, 0, 1, &scissor);
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Swapchain API
 * ═══════════════════════════════════════════════════════════════════════════ */

static VkPresentModeKHR lgx_to_vk_present_mode(lgx_gfx_present_mode_t mode) {
    switch (mode) {
        case LGX_GFX_PRESENT_MAILBOX:   return VK_PRESENT_MODE_MAILBOX_KHR;
        case LGX_GFX_PRESENT_IMMEDIATE: return VK_PRESENT_MODE_IMMEDIATE_KHR;
        default:                         return VK_PRESENT_MODE_FIFO_KHR;
    }
}

static lgx_result_t swapchain_create_image_views(lgx_gfx_swapchain_t* sc) {
    sc->image_views = calloc(sc->image_count, sizeof(VkImageView));
    if (!sc->image_views) return LGX_ERROR_OUT_OF_MEMORY;

    for (uint32_t i = 0; i < sc->image_count; i++) {
        VkImageViewCreateInfo vi = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = sc->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = sc->format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1,
            },
        };
        if (vkCreateImageView(sc->device->device, &vi, NULL, &sc->image_views[i]) != VK_SUCCESS)
            return LGX_ERROR_INVALID_PARAM;
    }
    return LGX_SUCCESS;
}

static lgx_result_t swapchain_create_sync(lgx_gfx_swapchain_t* sc) {
    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (uint32_t i = 0; i < LGX_GFX_MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(sc->device->device, &sem_info, NULL, &sc->image_available[i]) != VK_SUCCESS ||
            vkCreateSemaphore(sc->device->device, &sem_info, NULL, &sc->render_finished[i]) != VK_SUCCESS ||
            vkCreateFence(sc->device->device, &fence_info, NULL, &sc->in_flight[i]) != VK_SUCCESS)
            return LGX_ERROR_OUT_OF_MEMORY;
    }
    return LGX_SUCCESS;
}

lgx_gfx_swapchain_t* lgx_gfx_swapchain_create(lgx_gfx_device_t* dev,
                                                const lgx_gfx_swapchain_config_t* config) {
    if (!dev || !config || !config->native_window) return NULL;

    lgx_gfx_swapchain_t* sc = calloc(1, sizeof(*sc));
    if (!sc) return NULL;
    sc->device = dev;
    sc->width = config->width;
    sc->height = config->height;

    /* Create VkSurfaceKHR from native window */
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    if (config->is_wayland && config->native_display) {
        VkWaylandSurfaceCreateInfoKHR wsi = {
            .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = (struct wl_display*)config->native_display,
            .surface = (struct wl_surface*)config->native_window,
        };
        PFN_vkCreateWaylandSurfaceKHR create_fn =
            (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(dev->instance, "vkCreateWaylandSurfaceKHR");
        if (!create_fn || create_fn(dev->instance, &wsi, NULL, &sc->surface) != VK_SUCCESS) {
            free(sc); return NULL;
        }
    } else
#endif
    {
#ifdef VK_USE_PLATFORM_XLIB_KHR
        VkXlibSurfaceCreateInfoKHR xsi = {
            .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .dpy = (Display*)config->native_display,
            .window = (Window)(uintptr_t)config->native_window,
        };
        PFN_vkCreateXlibSurfaceKHR create_fn =
            (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(dev->instance, "vkCreateXlibSurfaceKHR");
        if (!create_fn || create_fn(dev->instance, &xsi, NULL, &sc->surface) != VK_SUCCESS) {
            free(sc); return NULL;
        }
#else
        fprintf(stderr, "[LGX GFX] No windowing backend compiled (need VK_USE_PLATFORM_XLIB_KHR or WAYLAND)\n");
        free(sc); return NULL;
#endif
    }

    /* Query surface capabilities */
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev->physical_device, sc->surface, &caps);

    /* Choose format */
    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev->physical_device, sc->surface, &fmt_count, NULL);
    VkSurfaceFormatKHR* fmts = calloc(fmt_count, sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev->physical_device, sc->surface, &fmt_count, fmts);
    sc->format = fmts[0].format;  /* Fallback */
    VkColorSpaceKHR color_space = fmts[0].colorSpace;
    for (uint32_t i = 0; i < fmt_count; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            sc->format = fmts[i].format;
            color_space = fmts[i].colorSpace;
            break;
        }
    }
    free(fmts);

    /* Image count */
    sc->image_count = (config->image_count >= 2) ? config->image_count : caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && sc->image_count > caps.maxImageCount)
        sc->image_count = caps.maxImageCount;

    /* Create swapchain */
    VkSwapchainCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = sc->surface,
        .minImageCount = sc->image_count,
        .imageFormat = sc->format,
        .imageColorSpace = color_space,
        .imageExtent = { sc->width, sc->height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = lgx_to_vk_present_mode(config->present_mode),
        .clipped = VK_TRUE,
    };

    if (vkCreateSwapchainKHR(dev->device, &sci, NULL, &sc->swapchain) != VK_SUCCESS) {
        vkDestroySurfaceKHR(dev->instance, sc->surface, NULL);
        free(sc); return NULL;
    }

    /* Get swapchain images */
    vkGetSwapchainImagesKHR(dev->device, sc->swapchain, &sc->image_count, NULL);
    sc->images = calloc(sc->image_count, sizeof(VkImage));
    vkGetSwapchainImagesKHR(dev->device, sc->swapchain, &sc->image_count, sc->images);

    /* Image views + sync objects */
    if (swapchain_create_image_views(sc) != LGX_SUCCESS ||
        swapchain_create_sync(sc) != LGX_SUCCESS) {
        lgx_gfx_swapchain_destroy(sc);
        return NULL;
    }

    sc->current_frame = 0;
    printf("[LGX GFX] Swapchain created: %ux%u, %u images\n",
           sc->width, sc->height, sc->image_count);
    return sc;
}

void lgx_gfx_swapchain_destroy(lgx_gfx_swapchain_t* sc) {
    if (!sc) return;
    if (sc->device && sc->device->device) vkDeviceWaitIdle(sc->device->device);

    for (uint32_t i = 0; i < LGX_GFX_MAX_FRAMES_IN_FLIGHT; i++) {
        if (sc->image_available[i]) vkDestroySemaphore(sc->device->device, sc->image_available[i], NULL);
        if (sc->render_finished[i]) vkDestroySemaphore(sc->device->device, sc->render_finished[i], NULL);
        if (sc->in_flight[i]) vkDestroyFence(sc->device->device, sc->in_flight[i], NULL);
    }
    if (sc->framebuffers) {
        for (uint32_t i = 0; i < sc->image_count; i++)
            if (sc->framebuffers[i]) vkDestroyFramebuffer(sc->device->device, sc->framebuffers[i], NULL);
        free(sc->framebuffers);
    }
    if (sc->image_views) {
        for (uint32_t i = 0; i < sc->image_count; i++)
            if (sc->image_views[i]) vkDestroyImageView(sc->device->device, sc->image_views[i], NULL);
        free(sc->image_views);
    }
    free(sc->images);
    if (sc->swapchain) vkDestroySwapchainKHR(sc->device->device, sc->swapchain, NULL);
    if (sc->surface) vkDestroySurfaceKHR(sc->device->instance, sc->surface, NULL);
    free(sc);
}

lgx_result_t lgx_gfx_swapchain_acquire(lgx_gfx_swapchain_t* sc, uint32_t* idx) {
    if (!sc || !idx) return LGX_ERROR_INVALID_PARAM;

    vkWaitForFences(sc->device->device, 1, &sc->in_flight[sc->current_frame],
                    VK_TRUE, UINT64_MAX);
    vkResetFences(sc->device->device, 1, &sc->in_flight[sc->current_frame]);

    VkResult vr = vkAcquireNextImageKHR(sc->device->device, sc->swapchain, UINT64_MAX,
                                         sc->image_available[sc->current_frame],
                                         VK_NULL_HANDLE, &sc->current_image);
    if (vr == VK_ERROR_OUT_OF_DATE_KHR || vr == VK_SUBOPTIMAL_KHR) {
        *idx = UINT32_MAX;  /* Signal caller to resize */
        return LGX_ERROR_INVALID_PARAM;
    }
    *idx = sc->current_image;
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_swapchain_present(lgx_gfx_swapchain_t* sc) {
    if (!sc) return LGX_ERROR_INVALID_PARAM;

    VkPresentInfoKHR pi = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sc->render_finished[sc->current_frame],
        .swapchainCount = 1,
        .pSwapchains = &sc->swapchain,
        .pImageIndices = &sc->current_image,
    };
    vkQueuePresentKHR(sc->device->graphics_queue, &pi);
    sc->current_frame = (sc->current_frame + 1) % LGX_GFX_MAX_FRAMES_IN_FLIGHT;
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_swapchain_resize(lgx_gfx_swapchain_t* sc, uint32_t w, uint32_t h) {
    if (!sc || w == 0 || h == 0) return LGX_ERROR_INVALID_PARAM;
    vkDeviceWaitIdle(sc->device->device);

    /* Destroy old image views */
    if (sc->image_views) {
        for (uint32_t i = 0; i < sc->image_count; i++)
            if (sc->image_views[i]) vkDestroyImageView(sc->device->device, sc->image_views[i], NULL);
        free(sc->image_views);
        sc->image_views = NULL;
    }
    if (sc->framebuffers) {
        for (uint32_t i = 0; i < sc->image_count; i++)
            if (sc->framebuffers[i]) vkDestroyFramebuffer(sc->device->device, sc->framebuffers[i], NULL);
        free(sc->framebuffers);
        sc->framebuffers = NULL;
    }
    free(sc->images);
    sc->images = NULL;

    /* Recreate swapchain */
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(sc->device->physical_device, sc->surface, &caps);
    sc->width = w;
    sc->height = h;

    VkSwapchainKHR old_swapchain = sc->swapchain;
    VkSwapchainCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = sc->surface,
        .minImageCount = sc->image_count,
        .imageFormat = sc->format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = { w, h },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    if (vkCreateSwapchainKHR(sc->device->device, &sci, NULL, &sc->swapchain) != VK_SUCCESS)
        return LGX_ERROR_INVALID_PARAM;
    vkDestroySwapchainKHR(sc->device->device, old_swapchain, NULL);

    vkGetSwapchainImagesKHR(sc->device->device, sc->swapchain, &sc->image_count, NULL);
    sc->images = calloc(sc->image_count, sizeof(VkImage));
    vkGetSwapchainImagesKHR(sc->device->device, sc->swapchain, &sc->image_count, sc->images);

    return swapchain_create_image_views(sc);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Descriptor Set API
 * ═══════════════════════════════════════════════════════════════════════════ */

static VkDescriptorType lgx_to_vk_descriptor_type(lgx_gfx_descriptor_type_t type) {
    switch (type) {
        case LGX_GFX_DESCRIPTOR_UNIFORM_BUFFER:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case LGX_GFX_DESCRIPTOR_STORAGE_BUFFER:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case LGX_GFX_DESCRIPTOR_COMBINED_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case LGX_GFX_DESCRIPTOR_STORAGE_IMAGE:   return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

lgx_gfx_descriptor_set_t* lgx_gfx_descriptor_set_create(
    lgx_gfx_device_t* dev, const lgx_gfx_descriptor_set_config_t* config) {
    if (!dev || !config || config->binding_count == 0 || !config->bindings) return NULL;

    lgx_gfx_descriptor_set_t* ds = calloc(1, sizeof(*ds));
    if (!ds) return NULL;
    ds->device = dev;

    /* Descriptor set layout */
    VkDescriptorSetLayoutBinding* layout_bindings =
        calloc(config->binding_count, sizeof(VkDescriptorSetLayoutBinding));
    VkDescriptorPoolSize* pool_sizes =
        calloc(config->binding_count, sizeof(VkDescriptorPoolSize));

    for (uint32_t i = 0; i < config->binding_count; i++) {
        VkDescriptorType vk_type = lgx_to_vk_descriptor_type(config->bindings[i].type);
        layout_bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = config->bindings[i].binding,
            .descriptorType = vk_type,
            .descriptorCount = 1,
            .stageFlags = (config->bindings[i].stage == LGX_GFX_SHADER_VERTEX)
                          ? VK_SHADER_STAGE_VERTEX_BIT
                          : (config->bindings[i].stage == LGX_GFX_SHADER_FRAGMENT)
                            ? VK_SHADER_STAGE_FRAGMENT_BIT
                            : VK_SHADER_STAGE_ALL,
        };
        pool_sizes[i] = (VkDescriptorPoolSize){
            .type = vk_type,
            .descriptorCount = 1,
        };
    }

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = config->binding_count,
        .pBindings = layout_bindings,
    };
    VkResult vr = vkCreateDescriptorSetLayout(dev->device, &layout_info, NULL, &ds->vk_layout);
    free(layout_bindings);
    if (vr != VK_SUCCESS) { free(pool_sizes); free(ds); return NULL; }

    /* Descriptor pool */
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = config->binding_count,
        .pPoolSizes = pool_sizes,
    };
    vr = vkCreateDescriptorPool(dev->device, &pool_info, NULL, &ds->vk_pool);
    free(pool_sizes);
    if (vr != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(dev->device, ds->vk_layout, NULL);
        free(ds); return NULL;
    }

    /* Allocate descriptor set */
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ds->vk_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ds->vk_layout,
    };
    vr = vkAllocateDescriptorSets(dev->device, &alloc_info, &ds->vk_set);
    if (vr != VK_SUCCESS) {
        vkDestroyDescriptorPool(dev->device, ds->vk_pool, NULL);
        vkDestroyDescriptorSetLayout(dev->device, ds->vk_layout, NULL);
        free(ds); return NULL;
    }

    /* Write descriptors */
    VkWriteDescriptorSet* writes = calloc(config->binding_count, sizeof(VkWriteDescriptorSet));
    VkDescriptorBufferInfo* buf_infos = calloc(config->binding_count, sizeof(VkDescriptorBufferInfo));

    for (uint32_t i = 0; i < config->binding_count; i++) {
        const lgx_gfx_descriptor_binding_t* b = &config->bindings[i];
        VkDescriptorType vk_type = lgx_to_vk_descriptor_type(b->type);

        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds->vk_set,
            .dstBinding = b->binding,
            .descriptorCount = 1,
            .descriptorType = vk_type,
        };

        if (b->buffer && (vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                          vk_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
            buf_infos[i] = (VkDescriptorBufferInfo){
                .buffer = b->buffer->vk_buffer,
                .offset = b->buffer_offset,
                .range = (b->buffer_range > 0) ? b->buffer_range : VK_WHOLE_SIZE,
            };
            writes[i].pBufferInfo = &buf_infos[i];
        }
        /* Image sampler writes would go here when sampler support is added */
    }

    vkUpdateDescriptorSets(dev->device, config->binding_count, writes, 0, NULL);
    free(writes);
    free(buf_infos);

    /* Create pipeline layout that includes this descriptor set */
    VkPipelineLayoutCreateInfo pl_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ds->vk_layout,
    };
    vkCreatePipelineLayout(dev->device, &pl_info, NULL, &ds->vk_pipeline_layout);

    return ds;
}

void lgx_gfx_descriptor_set_destroy(lgx_gfx_descriptor_set_t* ds) {
    if (!ds) return;
    if (ds->vk_pipeline_layout)
        vkDestroyPipelineLayout(ds->device->device, ds->vk_pipeline_layout, NULL);
    if (ds->vk_pool) vkDestroyDescriptorPool(ds->device->device, ds->vk_pool, NULL);
    if (ds->vk_layout) vkDestroyDescriptorSetLayout(ds->device->device, ds->vk_layout, NULL);
    free(ds);
}

lgx_result_t lgx_gfx_cmd_bind_descriptor_set(lgx_gfx_cmd_buffer_t* cmd,
                                              lgx_gfx_pipeline_t* pipeline,
                                              lgx_gfx_descriptor_set_t* ds) {
    if (!cmd || !pipeline || !ds || !cmd->recording) return LGX_ERROR_INVALID_PARAM;
    VkPipelineLayout layout = ds->vk_pipeline_layout ? ds->vk_pipeline_layout : pipeline->vk_layout;
    vkCmdBindDescriptorSets(cmd->vk_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout, 0, 1, &ds->vk_set, 0, NULL);
    return LGX_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shader Cache API
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_gfx_shader_cache_init(lgx_gfx_device_t* dev,
                                        const lgx_gfx_shader_cache_config_t* config) {
    if (!dev) return LGX_ERROR_INVALID_PARAM;
    if (config && config->cache_directory) {
        strncpy(dev->cache_dir, config->cache_directory, sizeof(dev->cache_dir) - 1);
    }
    dev->disk_cache_enabled = config ? config->enable_disk_cache : false;
    dev->max_cache_size = (config && config->max_cache_size_mb > 0)
                          ? config->max_cache_size_mb * 1024 * 1024
                          : 256 * 1024 * 1024;

    /* Try to load existing pipeline cache from disk */
    if (dev->disk_cache_enabled) {
        char path[600];
        snprintf(path, sizeof(path), "%s/pipeline_cache.bin", dev->cache_dir);
        FILE* f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (size > 0 && (size_t)size <= dev->max_cache_size) {
                void* data = malloc((size_t)size);
                if (data && fread(data, 1, (size_t)size, f) == (size_t)size) {
                    /* Recreate pipeline cache with loaded data */
                    VkPipelineCacheCreateInfo ci = {
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                        .initialDataSize = (size_t)size,
                        .pInitialData = data,
                    };
                    VkPipelineCache new_cache;
                    if (vkCreatePipelineCache(dev->device, &ci, NULL, &new_cache) == VK_SUCCESS) {
                        vkDestroyPipelineCache(dev->device, dev->pipeline_cache, NULL);
                        dev->pipeline_cache = new_cache;
                        dev->stats.shader_cache_hits++;
                    }
                }
                free(data);
            }
            fclose(f);
        }
    }
    return LGX_SUCCESS;
}

lgx_result_t lgx_gfx_shader_cache_flush(lgx_gfx_device_t* dev) {
    if (!dev || !dev->disk_cache_enabled) return LGX_ERROR_INVALID_PARAM;

    size_t cache_size = 0;
    vkGetPipelineCacheData(dev->device, dev->pipeline_cache, &cache_size, NULL);
    if (cache_size == 0) return LGX_SUCCESS;

    void* data = malloc(cache_size);
    if (!data) return LGX_ERROR_OUT_OF_MEMORY;
    vkGetPipelineCacheData(dev->device, dev->pipeline_cache, &cache_size, data);

    /* Ensure directory exists */
    char cmd_buf[650];
    snprintf(cmd_buf, sizeof(cmd_buf), "mkdir -p %s", dev->cache_dir);
    (void)system(cmd_buf);

    char path[600];
    snprintf(path, sizeof(path), "%s/pipeline_cache.bin", dev->cache_dir);
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, cache_size, f);
        fclose(f);
        dev->stats.shader_cache_size_bytes = cache_size;
    }
    free(data);
    return LGX_SUCCESS;
}

void lgx_gfx_shader_cache_destroy(lgx_gfx_device_t* dev) {
    if (!dev) return;
    if (dev->disk_cache_enabled) lgx_gfx_shader_cache_flush(dev);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Statistics
 * ═══════════════════════════════════════════════════════════════════════════ */

lgx_result_t lgx_gfx_get_stats(lgx_gfx_device_t* dev, lgx_gfx_stats_t* stats) {
    if (!dev || !stats) return LGX_ERROR_INVALID_PARAM;
    memcpy(stats, &dev->stats, sizeof(lgx_gfx_stats_t));
    return LGX_SUCCESS;
}

void lgx_gfx_reset_frame_stats(lgx_gfx_device_t* dev) {
    if (!dev) return;
    dev->stats.draw_calls = 0;
    dev->stats.pipeline_switches = 0;
    dev->stats.vertices_submitted = 0;
}
