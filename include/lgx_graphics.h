/**
 * LGX Graphics Module v1.2 — Public API
 *
 * Thin Vulkan wrapper providing device management, command recording,
 * pipeline state, resource lifecycle, and shader caching.
 *
 * Design:
 * - Opaque handles for all GPU objects
 * - Follows lgx_gfx_* naming pattern (LGX_PLATFORM_ARCHITECTURE.md §2.1)
 * - struct_size first field for ABI evolution (§2.5)
 * - Uses lgx_result_t error codes (§2.3)
 * - Thread-safe unless documented otherwise
 *
 * Requires: lgx_runtime (v1.0), Vulkan 1.3+
 * Optional: lgx_threading (v1.1) for parallel command recording
 *
 * Copyright 2026 LGX Runtime Platform Contributors
 * Licensed under the Apache License, Version 2.0
 */

#ifndef LGX_GRAPHICS_H
#define LGX_GRAPHICS_H

#include "lgx_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Version
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LGX_GRAPHICS_VERSION_MAJOR  1
#define LGX_GRAPHICS_VERSION_MINOR  2
#define LGX_GRAPHICS_VERSION_PATCH  0

/* ═══════════════════════════════════════════════════════════════════════════
 * Opaque Handle Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/** GPU device — wraps VkInstance + VkPhysicalDevice + VkDevice + queues */
typedef struct lgx_gfx_device        lgx_gfx_device_t;

/** Swapchain — wraps VkSwapchainKHR + image views + sync objects */
typedef struct lgx_gfx_swapchain     lgx_gfx_swapchain_t;

/** Command buffer — wraps VkCommandBuffer from a managed pool */
typedef struct lgx_gfx_cmd_buffer    lgx_gfx_cmd_buffer_t;

/** Graphics pipeline — wraps VkPipeline + VkPipelineLayout */
typedef struct lgx_gfx_pipeline      lgx_gfx_pipeline_t;

/** Render pass — wraps VkRenderPass + VkFramebuffer set */
typedef struct lgx_gfx_renderpass    lgx_gfx_renderpass_t;

/** GPU buffer (vertex, index, uniform, storage) */
typedef struct lgx_gfx_buffer        lgx_gfx_buffer_t;

/** GPU image (texture, render target) */
typedef struct lgx_gfx_image         lgx_gfx_image_t;

/** Shader module — wraps VkShaderModule (SPIR-V) */
typedef struct lgx_gfx_shader        lgx_gfx_shader_t;

/** Descriptor set — wraps VkDescriptorPool + VkDescriptorSetLayout + VkDescriptorSet */
typedef struct lgx_gfx_descriptor_set lgx_gfx_descriptor_set_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Enumerations
 * ═══════════════════════════════════════════════════════════════════════════ */

/** GPU selection preference */
typedef enum lgx_gfx_gpu_preference {
    LGX_GFX_GPU_PREFER_DISCRETE   = 0,  /**< Prefer dedicated GPU (default) */
    LGX_GFX_GPU_PREFER_INTEGRATED = 1,  /**< Prefer integrated GPU (power saving) */
    LGX_GFX_GPU_PREFER_ANY        = 2   /**< Accept any available GPU */
} lgx_gfx_gpu_preference_t;

/** Present mode */
typedef enum lgx_gfx_present_mode {
    LGX_GFX_PRESENT_FIFO     = 0,  /**< V-Sync ON (default, no tearing) */
    LGX_GFX_PRESENT_MAILBOX  = 1,  /**< Triple buffer (low latency, no tearing) */
    LGX_GFX_PRESENT_IMMEDIATE = 2  /**< V-Sync OFF (lowest latency, may tear) */
} lgx_gfx_present_mode_t;

/** Buffer usage flags (combinable with |) */
typedef enum lgx_gfx_buffer_usage {
    LGX_GFX_BUFFER_VERTEX   = 0x01,  /**< Vertex buffer */
    LGX_GFX_BUFFER_INDEX    = 0x02,  /**< Index buffer */
    LGX_GFX_BUFFER_UNIFORM  = 0x04,  /**< Uniform buffer (shader constants) */
    LGX_GFX_BUFFER_STORAGE  = 0x08,  /**< Storage buffer (compute, SSBO) */
    LGX_GFX_BUFFER_TRANSFER = 0x10   /**< Staging buffer for uploads */
} lgx_gfx_buffer_usage_t;

/** Image format */
typedef enum lgx_gfx_format {
    LGX_GFX_FORMAT_RGBA8_UNORM  = 0,   /**< 8-bit RGBA (sRGB-like) */
    LGX_GFX_FORMAT_RGBA8_SRGB   = 1,   /**< 8-bit RGBA (sRGB) */
    LGX_GFX_FORMAT_BGRA8_UNORM  = 2,   /**< 8-bit BGRA (swapchain common) */
    LGX_GFX_FORMAT_BGRA8_SRGB   = 3,   /**< 8-bit BGRA (sRGB) */
    LGX_GFX_FORMAT_R32_SFLOAT   = 4,   /**< 32-bit float (depth, height maps) */
    LGX_GFX_FORMAT_RG32_SFLOAT  = 5,   /**< 2x 32-bit float (UV coords) */
    LGX_GFX_FORMAT_RGB32_SFLOAT = 6,   /**< 3x 32-bit float (positions) */
    LGX_GFX_FORMAT_RGBA32_SFLOAT = 7,  /**< 4x 32-bit float (HDR) */
    LGX_GFX_FORMAT_RGBA16_SFLOAT = 8,  /**< 4x 16-bit float (HDR) */
    LGX_GFX_FORMAT_D32_SFLOAT   = 9,   /**< 32-bit depth */
    LGX_GFX_FORMAT_D24_S8       = 10   /**< 24-bit depth + 8-bit stencil */
} lgx_gfx_format_t;

/** Image usage flags (combinable with |) */
typedef enum lgx_gfx_image_usage {
    LGX_GFX_IMAGE_SAMPLED      = 0x01,  /**< Can be sampled in shader */
    LGX_GFX_IMAGE_COLOR_TARGET = 0x02,  /**< Can be used as color attachment */
    LGX_GFX_IMAGE_DEPTH_TARGET = 0x04,  /**< Can be used as depth attachment */
    LGX_GFX_IMAGE_TRANSFER_SRC = 0x08,  /**< Can be used as transfer source */
    LGX_GFX_IMAGE_TRANSFER_DST = 0x10   /**< Can be used as transfer dest */
} lgx_gfx_image_usage_t;

/** Shader stage */
typedef enum lgx_gfx_shader_stage {
    LGX_GFX_SHADER_VERTEX   = 0,
    LGX_GFX_SHADER_FRAGMENT = 1,
    LGX_GFX_SHADER_COMPUTE  = 2   /**< Reserved for future use */
} lgx_gfx_shader_stage_t;

/** Primitive topology */
typedef enum lgx_gfx_topology {
    LGX_GFX_TOPOLOGY_TRIANGLE_LIST  = 0,
    LGX_GFX_TOPOLOGY_TRIANGLE_STRIP = 1,
    LGX_GFX_TOPOLOGY_LINE_LIST      = 2,
    LGX_GFX_TOPOLOGY_POINT_LIST     = 3
} lgx_gfx_topology_t;

/** Polygon fill mode */
typedef enum lgx_gfx_polygon_mode {
    LGX_GFX_POLYGON_FILL      = 0,
    LGX_GFX_POLYGON_WIREFRAME = 1
} lgx_gfx_polygon_mode_t;

/** Cull mode */
typedef enum lgx_gfx_cull_mode {
    LGX_GFX_CULL_NONE  = 0,
    LGX_GFX_CULL_BACK  = 1,
    LGX_GFX_CULL_FRONT = 2
} lgx_gfx_cull_mode_t;

/** Blend factor */
typedef enum lgx_gfx_blend_factor {
    LGX_GFX_BLEND_ZERO           = 0,
    LGX_GFX_BLEND_ONE            = 1,
    LGX_GFX_BLEND_SRC_ALPHA      = 2,
    LGX_GFX_BLEND_ONE_MINUS_SRC_ALPHA = 3
} lgx_gfx_blend_factor_t;

/** Load/store operations for render pass attachments */
typedef enum lgx_gfx_load_op {
    LGX_GFX_LOAD_CLEAR    = 0,  /**< Clear to a value */
    LGX_GFX_LOAD_LOAD     = 1,  /**< Load previous content */
    LGX_GFX_LOAD_DONT_CARE = 2  /**< Content undefined */
} lgx_gfx_load_op_t;

typedef enum lgx_gfx_store_op {
    LGX_GFX_STORE_STORE     = 0,  /**< Store results */
    LGX_GFX_STORE_DONT_CARE = 1   /**< Results undefined */
} lgx_gfx_store_op_t;

/** Descriptor type */
typedef enum lgx_gfx_descriptor_type {
    LGX_GFX_DESCRIPTOR_UNIFORM_BUFFER  = 0,  /**< Uniform buffer (UBO) */
    LGX_GFX_DESCRIPTOR_STORAGE_BUFFER  = 1,  /**< Storage buffer (SSBO) */
    LGX_GFX_DESCRIPTOR_COMBINED_SAMPLER = 2, /**< Combined image sampler */
    LGX_GFX_DESCRIPTOR_STORAGE_IMAGE   = 3   /**< Storage image (compute) */
} lgx_gfx_descriptor_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Device configuration */
typedef struct lgx_gfx_device_config {
    size_t struct_size;                     /**< Must be sizeof(lgx_gfx_device_config_t) */
    const char* app_name;                   /**< Application name (shown in debug tools) */
    uint32_t app_version;                   /**< Application version */
    lgx_gfx_gpu_preference_t gpu_preference; /**< GPU selection preference */
    bool enable_validation;                 /**< Enable Vulkan validation layers (debug) */
    bool enable_debug_markers;              /**< Enable GPU debug markers */
} lgx_gfx_device_config_t;

/** Swapchain configuration */
typedef struct lgx_gfx_swapchain_config {
    size_t struct_size;                     /**< Must be sizeof(lgx_gfx_swapchain_config_t) */
    void* native_window;                    /**< Native window handle (X11 Window or wl_surface*) */
    void* native_display;                   /**< Native display handle (X11 Display* or wl_display*) */
    uint32_t width;                         /**< Initial width in pixels */
    uint32_t height;                        /**< Initial height in pixels */
    lgx_gfx_present_mode_t present_mode;    /**< Presentation mode */
    lgx_gfx_format_t format;                /**< Surface format */
    uint32_t image_count;                   /**< Number of swapchain images (2=double, 3=triple) */
    bool is_wayland;                        /**< true for Wayland, false for X11 */
} lgx_gfx_swapchain_config_t;

/** Vertex attribute description */
typedef struct lgx_gfx_vertex_attribute {
    uint32_t location;                      /**< Shader location (layout(location=N)) */
    lgx_gfx_format_t format;                /**< Attribute format */
    uint32_t offset;                        /**< Byte offset within vertex struct */
} lgx_gfx_vertex_attribute_t;

/** Pipeline configuration */
typedef struct lgx_gfx_pipeline_config {
    size_t struct_size;                      /**< Must be sizeof(lgx_gfx_pipeline_config_t) */

    /* Shaders */
    lgx_gfx_shader_t* vertex_shader;         /**< Vertex shader (required) */
    lgx_gfx_shader_t* fragment_shader;       /**< Fragment shader (required) */

    /* Vertex input */
    const lgx_gfx_vertex_attribute_t* attributes;  /**< Vertex attribute array */
    uint32_t attribute_count;                /**< Number of attributes */
    uint32_t vertex_stride;                  /**< Byte stride between vertices */

    /* Rasterization */
    lgx_gfx_topology_t topology;             /**< Primitive topology */
    lgx_gfx_polygon_mode_t polygon_mode;     /**< Fill or wireframe */
    lgx_gfx_cull_mode_t cull_mode;           /**< Face culling */
    bool depth_test;                         /**< Enable depth testing */
    bool depth_write;                        /**< Enable depth writes */

    /* Blending */
    bool blend_enable;                       /**< Enable alpha blending */
    lgx_gfx_blend_factor_t src_blend;        /**< Source blend factor */
    lgx_gfx_blend_factor_t dst_blend;        /**< Dest blend factor */

    /* Render pass */
    lgx_gfx_renderpass_t* renderpass;        /**< Target render pass */
} lgx_gfx_pipeline_config_t;

/** Render pass attachment description */
typedef struct lgx_gfx_attachment_desc {
    lgx_gfx_format_t format;                /**< Attachment format */
    lgx_gfx_load_op_t load_op;              /**< What to do with existing content */
    lgx_gfx_store_op_t store_op;            /**< What to do after rendering */
    bool is_depth;                          /**< true if depth/stencil attachment */
} lgx_gfx_attachment_desc_t;

/** Render pass configuration */
typedef struct lgx_gfx_renderpass_config {
    size_t struct_size;                      /**< Must be sizeof(lgx_gfx_renderpass_config_t) */
    const lgx_gfx_attachment_desc_t* attachments;  /**< Attachment descriptions */
    uint32_t attachment_count;               /**< Number of attachments */
} lgx_gfx_renderpass_config_t;

/** Shader cache configuration */
typedef struct lgx_gfx_shader_cache_config {
    size_t struct_size;                      /**< Must be sizeof(lgx_gfx_shader_cache_config_t) */
    const char* cache_directory;             /**< Disk cache path (NULL = ~/.cache/lgx/) */
    size_t max_cache_size_mb;                /**< Max disk cache size (0 = 256MB default) */
    bool enable_disk_cache;                  /**< Persist pipeline cache to disk */
} lgx_gfx_shader_cache_config_t;

/** Descriptor binding description */
typedef struct lgx_gfx_descriptor_binding {
    uint32_t binding;                        /**< Binding index in shader */
    lgx_gfx_descriptor_type_t type;          /**< Descriptor type */
    lgx_gfx_shader_stage_t stage;            /**< Shader stage visibility */
    lgx_gfx_buffer_t* buffer;                /**< Buffer to bind (for UBO/SSBO) */
    lgx_gfx_image_t* image;                  /**< Image to bind (for samplers) */
    size_t buffer_offset;                    /**< Offset into buffer */
    size_t buffer_range;                     /**< Size of buffer range (0 = whole) */
} lgx_gfx_descriptor_binding_t;

/** Descriptor set configuration */
typedef struct lgx_gfx_descriptor_set_config {
    size_t struct_size;                      /**< Must be sizeof(lgx_gfx_descriptor_set_config_t) */
    const lgx_gfx_descriptor_binding_t* bindings; /**< Array of bindings */
    uint32_t binding_count;                  /**< Number of bindings */
} lgx_gfx_descriptor_set_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Statistics
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Graphics module statistics */
typedef struct lgx_gfx_stats {
    size_t struct_size;                      /**< Must be sizeof(lgx_gfx_stats_t) */

    /* Device info */
    char gpu_name[256];                      /**< GPU name string */
    uint32_t vulkan_api_version;             /**< Vulkan API version */
    uint64_t device_memory_bytes;            /**< Total device memory */

    /* Resource counts */
    uint32_t active_buffers;                 /**< Currently allocated buffers */
    uint32_t active_images;                  /**< Currently allocated images */
    uint32_t active_pipelines;               /**< Currently created pipelines */
    uint32_t active_shaders;                 /**< Currently loaded shader modules */

    /* Frame stats (reset per frame) */
    uint32_t draw_calls;                     /**< Draw calls this frame */
    uint32_t pipeline_switches;              /**< Pipeline bind calls this frame */
    uint64_t vertices_submitted;             /**< Vertices submitted this frame */

    /* Shader cache */
    uint64_t shader_cache_hits;              /**< Cache hit count */
    uint64_t shader_cache_misses;            /**< Cache miss count */
    uint64_t shader_cache_size_bytes;        /**< Pipeline cache size */
} lgx_gfx_stats_t;

/** GPU capability info (after device creation) */
typedef struct lgx_gfx_gpu_info {
    size_t struct_size;                      /**< Must be sizeof(lgx_gfx_gpu_info_t) */
    char gpu_name[256];                      /**< GPU name */
    char driver_version[64];                 /**< Driver version string */
    uint32_t vulkan_api_version;             /**< Supported Vulkan version */
    uint64_t device_memory_bytes;            /**< Total device-local memory */
    uint64_t host_visible_memory_bytes;      /**< Total host-visible memory */
    uint32_t max_image_dimension_2d;         /**< Max 2D image dimension */
    uint32_t max_framebuffer_width;          /**< Max framebuffer size */
    uint32_t max_framebuffer_height;
    bool supports_geometry_shader;
    bool supports_tessellation;
    bool supports_compute;
    bool supports_multiview;
    bool is_discrete;                        /**< true if dedicated GPU */
} lgx_gfx_gpu_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Device API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a GPU device. Initializes Vulkan instance, selects physical device,
 * creates logical device and queues.
 *
 * @param config  Device configuration (NULL for defaults)
 * @return Opaque device handle, or NULL on failure
 */
lgx_gfx_device_t* lgx_gfx_device_create(const lgx_gfx_device_config_t* config);

/** Destroy a device and all associated Vulkan objects. Safe with NULL. */
void lgx_gfx_device_destroy(lgx_gfx_device_t* device);

/** Get GPU capability information. */
lgx_result_t lgx_gfx_device_get_gpu_info(lgx_gfx_device_t* device,
                                          lgx_gfx_gpu_info_t* info);

/** Wait for all GPU operations to complete (device idle). */
lgx_result_t lgx_gfx_device_wait_idle(lgx_gfx_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * Swapchain API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a swapchain for presenting rendered frames.
 *
 * @param device   GPU device
 * @param config   Swapchain configuration
 * @return Opaque swapchain handle, or NULL on failure
 */
lgx_gfx_swapchain_t* lgx_gfx_swapchain_create(lgx_gfx_device_t* device,
                                                const lgx_gfx_swapchain_config_t* config);

/** Destroy a swapchain. Safe with NULL. */
void lgx_gfx_swapchain_destroy(lgx_gfx_swapchain_t* swapchain);

/**
 * Acquire the next swapchain image for rendering.
 * Returns the image index in *image_index.
 */
lgx_result_t lgx_gfx_swapchain_acquire(lgx_gfx_swapchain_t* swapchain,
                                        uint32_t* image_index);

/** Present a rendered frame to screen. */
lgx_result_t lgx_gfx_swapchain_present(lgx_gfx_swapchain_t* swapchain);

/**
 * Resize swapchain (call after window resize event).
 * Recreates internal VkSwapchainKHR.
 */
lgx_result_t lgx_gfx_swapchain_resize(lgx_gfx_swapchain_t* swapchain,
                                       uint32_t new_width, uint32_t new_height);

/* ═══════════════════════════════════════════════════════════════════════════
 * Command Buffer API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Begin recording a command buffer. Allocates from the device's command pool.
 * The returned buffer is valid until lgx_gfx_cmd_end() + submit.
 *
 * @param device  GPU device
 * @return Opaque command buffer handle, or NULL on failure
 */
lgx_gfx_cmd_buffer_t* lgx_gfx_cmd_begin(lgx_gfx_device_t* device);

/** End recording and finalize the command buffer. */
lgx_result_t lgx_gfx_cmd_end(lgx_gfx_cmd_buffer_t* cmd);

/** Submit a recorded command buffer to the GPU graphics queue. */
lgx_result_t lgx_gfx_cmd_submit(lgx_gfx_device_t* device,
                                 lgx_gfx_cmd_buffer_t* cmd);

/** Begin a render pass within a command buffer. */
lgx_result_t lgx_gfx_cmd_begin_renderpass(lgx_gfx_cmd_buffer_t* cmd,
                                           lgx_gfx_renderpass_t* renderpass,
                                           lgx_gfx_swapchain_t* swapchain,
                                           uint32_t image_index,
                                           float clear_r, float clear_g,
                                           float clear_b, float clear_a);

/** End the current render pass. */
lgx_result_t lgx_gfx_cmd_end_renderpass(lgx_gfx_cmd_buffer_t* cmd);

/** Bind a graphics pipeline. */
lgx_result_t lgx_gfx_cmd_bind_pipeline(lgx_gfx_cmd_buffer_t* cmd,
                                        lgx_gfx_pipeline_t* pipeline);

/** Bind a vertex buffer. */
lgx_result_t lgx_gfx_cmd_bind_vertex_buffer(lgx_gfx_cmd_buffer_t* cmd,
                                             lgx_gfx_buffer_t* buffer);

/** Bind an index buffer. */
lgx_result_t lgx_gfx_cmd_bind_index_buffer(lgx_gfx_cmd_buffer_t* cmd,
                                            lgx_gfx_buffer_t* buffer);

/** Draw vertices (non-indexed). */
lgx_result_t lgx_gfx_cmd_draw(lgx_gfx_cmd_buffer_t* cmd,
                               uint32_t vertex_count,
                               uint32_t instance_count,
                               uint32_t first_vertex,
                               uint32_t first_instance);

/** Draw indexed vertices. */
lgx_result_t lgx_gfx_cmd_draw_indexed(lgx_gfx_cmd_buffer_t* cmd,
                                       uint32_t index_count,
                                       uint32_t instance_count,
                                       uint32_t first_index,
                                       int32_t vertex_offset,
                                       uint32_t first_instance);

/** Set viewport dynamically. */
lgx_result_t lgx_gfx_cmd_set_viewport(lgx_gfx_cmd_buffer_t* cmd,
                                       float x, float y,
                                       float width, float height,
                                       float min_depth, float max_depth);

/** Set scissor rect dynamically. */
lgx_result_t lgx_gfx_cmd_set_scissor(lgx_gfx_cmd_buffer_t* cmd,
                                      int32_t x, int32_t y,
                                      uint32_t width, uint32_t height);

/* ═══════════════════════════════════════════════════════════════════════════
 * Resource API (Buffers & Images)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a GPU buffer.
 *
 * @param device     GPU device
 * @param size       Buffer size in bytes
 * @param usage      Usage flags (LGX_GFX_BUFFER_*)
 * @param host_visible  If true, buffer is CPU-mappable (for uploads)
 * @return Opaque buffer handle, or NULL on failure
 */
lgx_gfx_buffer_t* lgx_gfx_buffer_create(lgx_gfx_device_t* device,
                                          size_t size,
                                          uint32_t usage,
                                          bool host_visible);

/** Destroy a buffer. Safe with NULL. */
void lgx_gfx_buffer_destroy(lgx_gfx_buffer_t* buffer);

/**
 * Map a host-visible buffer for CPU access. Returns pointer to mapped memory.
 * Unmap with lgx_gfx_buffer_unmap().
 */
void* lgx_gfx_buffer_map(lgx_gfx_buffer_t* buffer);

/** Unmap a previously mapped buffer. */
void lgx_gfx_buffer_unmap(lgx_gfx_buffer_t* buffer);

/**
 * Upload data to a buffer. If buffer is not host-visible, uses an internal
 * staging buffer + transfer queue.
 */
lgx_result_t lgx_gfx_buffer_upload(lgx_gfx_buffer_t* buffer,
                                    const void* data, size_t size,
                                    size_t offset);

/**
 * Create a 2D GPU image.
 *
 * @param device  GPU device
 * @param width   Image width
 * @param height  Image height
 * @param format  Pixel format
 * @param usage   Usage flags (LGX_GFX_IMAGE_*)
 * @return Opaque image handle, or NULL on failure
 */
lgx_gfx_image_t* lgx_gfx_image_create(lgx_gfx_device_t* device,
                                        uint32_t width, uint32_t height,
                                        lgx_gfx_format_t format,
                                        uint32_t usage);

/** Destroy an image. Safe with NULL. */
void lgx_gfx_image_destroy(lgx_gfx_image_t* image);

/** Upload pixel data to an image (handles layout transitions). */
lgx_result_t lgx_gfx_image_upload(lgx_gfx_image_t* image,
                                   const void* pixels,
                                   size_t data_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Shader API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Load a SPIR-V shader module from memory.
 *
 * @param device      GPU device
 * @param spirv_code  Pointer to SPIR-V bytecode
 * @param code_size   Size of SPIR-V data in bytes
 * @param stage       Shader stage (vertex, fragment)
 * @param entry_point Entry function name (typically "main")
 * @return Opaque shader handle, or NULL on failure
 */
lgx_gfx_shader_t* lgx_gfx_shader_create(lgx_gfx_device_t* device,
                                          const uint32_t* spirv_code,
                                          size_t code_size,
                                          lgx_gfx_shader_stage_t stage,
                                          const char* entry_point);

/** Load a SPIR-V shader module from a file. */
lgx_gfx_shader_t* lgx_gfx_shader_load(lgx_gfx_device_t* device,
                                        const char* filepath,
                                        lgx_gfx_shader_stage_t stage);

/** Destroy a shader module. Safe with NULL. */
void lgx_gfx_shader_destroy(lgx_gfx_shader_t* shader);

/* ═══════════════════════════════════════════════════════════════════════════
 * Pipeline API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a graphics pipeline.
 *
 * @param device  GPU device
 * @param config  Pipeline configuration
 * @return Opaque pipeline handle, or NULL on failure
 */
lgx_gfx_pipeline_t* lgx_gfx_pipeline_create(lgx_gfx_device_t* device,
                                              const lgx_gfx_pipeline_config_t* config);

/** Destroy a pipeline. Safe with NULL. */
void lgx_gfx_pipeline_destroy(lgx_gfx_pipeline_t* pipeline);

/* ═══════════════════════════════════════════════════════════════════════════
 * Render Pass API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a render pass.
 *
 * @param device  GPU device
 * @param config  Render pass configuration
 * @return Opaque render pass handle, or NULL on failure
 */
lgx_gfx_renderpass_t* lgx_gfx_renderpass_create(lgx_gfx_device_t* device,
                                                  const lgx_gfx_renderpass_config_t* config);

/** Destroy a render pass. Safe with NULL. */
void lgx_gfx_renderpass_destroy(lgx_gfx_renderpass_t* renderpass);

/* ═══════════════════════════════════════════════════════════════════════════
 * Shader Cache API
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Initialize the shader/pipeline cache. Call after device creation. */
lgx_result_t lgx_gfx_shader_cache_init(lgx_gfx_device_t* device,
                                        const lgx_gfx_shader_cache_config_t* config);

/** Flush the shader cache to disk. */
lgx_result_t lgx_gfx_shader_cache_flush(lgx_gfx_device_t* device);

/** Destroy the shader cache. */
void lgx_gfx_shader_cache_destroy(lgx_gfx_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * Statistics & Debug
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Get graphics module statistics. */
lgx_result_t lgx_gfx_get_stats(lgx_gfx_device_t* device,
                                lgx_gfx_stats_t* stats);

/** Reset per-frame statistics (call at frame start). */
void lgx_gfx_reset_frame_stats(lgx_gfx_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * Descriptor Set API
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create a descriptor set with bindings for uniform buffers, storage buffers,
 * and image samplers.
 */
lgx_gfx_descriptor_set_t* lgx_gfx_descriptor_set_create(
    lgx_gfx_device_t* device, const lgx_gfx_descriptor_set_config_t* config);

/** Destroy a descriptor set. Safe with NULL. */
void lgx_gfx_descriptor_set_destroy(lgx_gfx_descriptor_set_t* desc_set);

/** Bind a descriptor set during command recording. */
lgx_result_t lgx_gfx_cmd_bind_descriptor_set(lgx_gfx_cmd_buffer_t* cmd,
                                              lgx_gfx_pipeline_t* pipeline,
                                              lgx_gfx_descriptor_set_t* desc_set);

#ifdef __cplusplus
}
#endif

#endif /* LGX_GRAPHICS_H */
