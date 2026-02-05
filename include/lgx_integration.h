/**
 * LGX Runtime Integration - Integration Contracts for Other Components
 * 
 * This header defines the integration interfaces that other LGX components
 * use to interact with the Runtime Core.
 */

#ifndef LGX_INTEGRATION_H
#define LGX_INTEGRATION_H

#include "lgx_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct lgx_translate_context lgx_translate_context_t;
typedef struct lgx_security_hooks lgx_security_hooks_t;
typedef struct lgx_shader_cache_config lgx_shader_cache_config_t;
typedef struct lgx_error_context lgx_error_context_t;

// Component enumeration
typedef enum lgx_component {
    LGX_COMPONENT_DX11_TRANSLATION,
    LGX_COMPONENT_DX12_TRANSLATION,
    LGX_COMPONENT_SECURITY_MODULE,
    LGX_COMPONENT_SHADER_CACHE,
    LGX_COMPONENT_TELEMETRY,
    LGX_COMPONENT_COUNT
} lgx_component_t;

// Translation Layer Integration
typedef struct lgx_translate_context {
    size_t struct_size;
    
    // Memory allocation functions
    void* (*alloc)(size_t size);
    void (*free)(void* ptr);
    void* (*alloc_aligned)(size_t size, size_t alignment);
    
    // Configuration
    const char* cache_directory;
    size_t max_cache_size_mb;
    
    // Lifecycle notifications
    void (*notify_suspend)(void);
    void (*notify_resume)(void);
    
    // GPU device handle (opaque)
    void* gpu_device_handle;
    
    // Performance hints
    bool prefer_async_compilation;
    uint32_t max_concurrent_compiles;
} lgx_translate_context_t;

// Security Module Integration
typedef struct lgx_security_hooks {
    size_t struct_size;
    
    // Memory allocation hooks
    void (*on_alloc)(void* ptr, size_t size, void* user_data);
    void (*on_free)(void* ptr, void* user_data);
    
    // API call monitoring
    void (*on_api_call)(const char* func_name, void* user_data);
    
    // Error monitoring
    void (*on_error)(const lgx_error_context_t* error, void* user_data);
    
    // Lifecycle hooks
    void (*on_suspend)(void* user_data);
    void (*on_resume)(void* user_data);
    void (*on_shutdown)(void* user_data);
    
    // Security events
    void (*on_security_violation)(const char* violation_type, 
                                  const char* details, void* user_data);
} lgx_security_hooks_t;

// Shader Manager Integration
typedef struct lgx_shader_cache_config {
    size_t struct_size;
    
    // Cache configuration
    const char* cache_directory;
    size_t max_cache_size_mb;
    bool enable_compression;
    
    // Cache events
    void (*on_cache_miss)(const char* shader_hash, void* user_data);
    void (*on_cache_hit)(const char* shader_hash, void* user_data);
    void (*on_cache_eviction)(const char* shader_hash, void* user_data);
    
    // Compilation settings
    uint32_t max_concurrent_compiles;
    bool enable_async_compilation;
} lgx_shader_cache_config_t;

// Plugin architecture
typedef struct lgx_plugin_interface {
    size_t struct_size;
    
    // Plugin metadata
    const char* name;
    const char* version;
    uint32_t api_version;
    
    // Plugin lifecycle
    lgx_result_t (*init)(void* config);
    lgx_result_t (*shutdown)(void);
    lgx_result_t (*suspend)(void);
    lgx_result_t (*resume)(void);
    
    // Plugin capabilities
    uint32_t capabilities;
    bool (*has_capability)(uint32_t capability);
} lgx_plugin_interface_t;

// Integration API functions

/**
 * Translation Layer Integration
 */
lgx_translate_context_t* lgx_runtime_get_translate_context(void);
void* lgx_translate_alloc(size_t size);
void lgx_translate_free(void* ptr);
void* lgx_translate_alloc_aligned(size_t size, size_t alignment);

/**
 * Security Module Integration
 */
lgx_result_t lgx_runtime_register_security_hooks(const lgx_security_hooks_t* hooks, 
                                                 void* user_data);
lgx_result_t lgx_runtime_unregister_security_hooks(void);

/**
 * Shader Manager Integration
 */
lgx_result_t lgx_runtime_configure_shader_cache(const lgx_shader_cache_config_t* config);

/**
 * Plugin Architecture
 */
bool lgx_runtime_is_component_loaded(lgx_component_t component);
lgx_result_t lgx_runtime_load_component(lgx_component_t component);
lgx_result_t lgx_runtime_unload_component(lgx_component_t component);

lgx_result_t lgx_runtime_register_plugin(const lgx_plugin_interface_t* plugin);
lgx_result_t lgx_runtime_unregister_plugin(const char* plugin_name);

/**
 * Component Communication
 */
lgx_result_t lgx_runtime_send_message(lgx_component_t target, 
                                     const char* message_type,
                                     const void* data, size_t data_size);

lgx_result_t lgx_runtime_register_message_handler(lgx_component_t component,
                                                  const char* message_type,
                                                  void (*handler)(const void* data, 
                                                                 size_t data_size,
                                                                 void* user_data),
                                                  void* user_data);

/**
 * Resource Sharing
 */
typedef struct lgx_shared_resource {
    size_t struct_size;
    const char* name;
    void* data;
    size_t size;
    uint32_t access_flags;  // Read/write permissions
} lgx_shared_resource_t;

lgx_result_t lgx_runtime_share_resource(const lgx_shared_resource_t* resource);
lgx_result_t lgx_runtime_get_shared_resource(const char* name, 
                                            lgx_shared_resource_t* resource);
lgx_result_t lgx_runtime_unshare_resource(const char* name);

/**
 * Event System
 */
typedef enum lgx_event_type {
    LGX_EVENT_INIT_COMPLETE,
    LGX_EVENT_SHUTDOWN_BEGIN,
    LGX_EVENT_SUSPEND,
    LGX_EVENT_RESUME,
    LGX_EVENT_MEMORY_PRESSURE,
    LGX_EVENT_GPU_RESET,
    LGX_EVENT_ERROR,
    LGX_EVENT_CUSTOM
} lgx_event_type_t;

typedef struct lgx_event {
    size_t struct_size;
    lgx_event_type_t type;
    uint64_t timestamp_ns;
    const void* data;
    size_t data_size;
    const char* source_component;
} lgx_event_t;

typedef void (*lgx_event_handler_t)(const lgx_event_t* event, void* user_data);

lgx_result_t lgx_runtime_register_event_handler(lgx_event_type_t event_type,
                                               lgx_event_handler_t handler,
                                               void* user_data);

lgx_result_t lgx_runtime_unregister_event_handler(lgx_event_type_t event_type,
                                                 lgx_event_handler_t handler);

lgx_result_t lgx_runtime_emit_event(const lgx_event_t* event);

#ifdef __cplusplus
}
#endif

#endif // LGX_INTEGRATION_H