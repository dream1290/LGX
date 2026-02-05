/**
 * LGX Runtime Internal Header
 * 
 * Internal structures and interfaces for the LGX Runtime Core.
 * This header is NOT part of the public API.
 */

#ifndef LGX_RUNTIME_INTERNAL_H
#define LGX_RUNTIME_INTERNAL_H

#include "lgx_runtime.h"
#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>  // For ssize_t
#include <limits.h>     // For PATH_MAX

#ifdef __cplusplus
extern "C" {
#endif

// Health status enumeration
typedef enum {
    LGX_HEALTH_GOOD = 0,
    LGX_HEALTH_WARNING = 1,
    LGX_HEALTH_CRITICAL = 2
} lgx_health_level_t;

// Health status structure
typedef struct {
    size_t struct_size;
    lgx_health_level_t overall_health;
    size_t memory_usage_mb;
    float cpu_usage_percent;
    uint32_t degraded_features;
} lgx_health_status_t;

// Error callback type (internal - different from public API)
typedef void (*lgx_internal_error_callback_t)(lgx_result_t error, const char* message, 
                                              const char* recovery_guidance, void* user_data);

// Runtime configuration structure (internal definition)
struct lgx_runtime_config {
    size_t memory_pool_size;
    const char* log_path;
    uint32_t flags;
};

// Forward declarations for subsystem handles
typedef struct lgx_memory_manager lgx_memory_manager_t;
typedef struct lgx_hardware_adapter lgx_hardware_adapter_t;
typedef struct lgx_capability_detector lgx_capability_detector_t;
typedef struct lgx_lifecycle_manager lgx_lifecycle_manager_t;
typedef struct lgx_platform_services lgx_platform_services_t;
typedef struct lgx_telemetry lgx_telemetry_t;
typedef struct lgx_error_handler lgx_error_handler_t;
typedef struct lgx_health_monitor lgx_health_monitor_t;

// Configuration flags
#define LGX_CONFIG_ENABLE_TELEMETRY     (1 << 0)
#define LGX_CONFIG_ENABLE_DEBUG_LOGGING (1 << 1)
#define LGX_CONFIG_STRICT_VALIDATION    (1 << 2)

// Global runtime state
typedef struct lgx_runtime_state {
    bool initialized;
    pthread_mutex_t mutex;
    
    // Configuration
    const lgx_runtime_config_t* config;
    
    // Subsystem handles
    lgx_memory_manager_t* memory_manager;
    lgx_hardware_adapter_t* hardware_adapter;
    lgx_capability_detector_t* capability_detector;
    lgx_lifecycle_manager_t* lifecycle_manager;
    lgx_platform_services_t* platform_services;
    lgx_telemetry_t* telemetry;
    lgx_error_handler_t* error_handler;
    lgx_health_monitor_t* health_monitor;
} lgx_runtime_state_t;

// Internal API functions
lgx_runtime_state_t* lgx_runtime_get_state(void);
bool lgx_runtime_is_initialized(void);

// Subsystem initialization functions
lgx_result_t lgx_memory_manager_init(lgx_memory_manager_t** manager, 
                                    const lgx_runtime_config_t* config,
                                    lgx_hardware_adapter_t* hardware_adapter);
lgx_result_t lgx_memory_manager_shutdown(lgx_memory_manager_t* manager);

// Memory manager API functions
void* lgx_memory_manager_alloc(lgx_memory_manager_t* manager, size_t size, 
                              const lgx_allocation_intent_base_t* intent);
void lgx_memory_manager_free(lgx_memory_manager_t* manager, void* ptr);
void* lgx_memory_manager_alloc_aligned(lgx_memory_manager_t* manager, size_t size, 
                                      size_t alignment, const lgx_allocation_intent_base_t* intent);
void* lgx_memory_manager_alloc_with_intent_ex(lgx_memory_manager_t* manager, 
                                             const void* intent, size_t intent_type_id);
lgx_result_t lgx_memory_manager_get_stats(lgx_memory_manager_t* manager, lgx_memory_stats_t* stats);
lgx_result_t lgx_memory_manager_get_usage_stats(lgx_memory_manager_t* manager, void* ptr, 
                                               lgx_allocation_usage_t* usage);
lgx_result_t lgx_memory_manager_validate_intent(lgx_memory_manager_t* manager, void* ptr);

// Hardware adapter API functions
bool lgx_hardware_adapter_has_huge_pages(lgx_hardware_adapter_t* adapter);
bool lgx_hardware_adapter_has_numa(lgx_hardware_adapter_t* adapter);
long lgx_hardware_adapter_get_cpu_count(lgx_hardware_adapter_t* adapter);
lgx_hardware_status_t lgx_hardware_adapter_get_status(lgx_hardware_adapter_t* adapter);

// Capability detector API functions
bool lgx_capability_detector_has_capability(lgx_capability_detector_t* detector, 
                                           const char* capability_name);
bool lgx_capability_detector_has_capability_enum(lgx_capability_detector_t* detector, 
                                                lgx_capability_t capability);
uint32_t lgx_capability_detector_get_capability_version(lgx_capability_detector_t* detector,
                                                       const char* capability_name);
lgx_result_t lgx_capability_detector_query_capabilities(lgx_capability_detector_t* detector,
                                                       uint32_t* capabilities);

// Lifecycle manager API functions
lgx_result_t lgx_lifecycle_manager_suspend(lgx_lifecycle_manager_t* manager);
lgx_result_t lgx_lifecycle_manager_resume(lgx_lifecycle_manager_t* manager);

// Platform services API functions
int lgx_platform_services_fs_open(lgx_platform_services_t* services, 
                                  const char* path, int flags);
ssize_t lgx_platform_services_fs_read(lgx_platform_services_t* services,
                                     int fd, void* buffer, size_t size);
ssize_t lgx_platform_services_fs_write(lgx_platform_services_t* services,
                                      int fd, const void* buffer, size_t size);
int lgx_platform_services_fs_close(lgx_platform_services_t* services, int fd);
uint64_t lgx_platform_services_time_now_ns(lgx_platform_services_t* services);
lgx_result_t lgx_platform_services_time_sleep_ms(lgx_platform_services_t* services,
                                                 uint32_t milliseconds);

// Error handler API functions
lgx_result_t lgx_error_handler_set_error(lgx_error_handler_t* handler,
                                        lgx_result_t error,
                                        const char* function,
                                        const char* file,
                                        int line);
lgx_result_t lgx_error_handler_get_last_error(lgx_error_handler_t* handler);
lgx_result_t lgx_error_handler_get_last_error_ex(lgx_error_handler_t* handler,
                                                char* message_buffer, size_t message_size,
                                                char* recovery_buffer, size_t recovery_size);

// Health monitor API functions
lgx_result_t lgx_health_monitor_check(lgx_health_monitor_t* monitor,
                                     lgx_health_status_t* status);
lgx_health_status_t lgx_health_monitor_get_status(lgx_health_monitor_t* monitor);

// Telemetry API functions
lgx_result_t lgx_telemetry_enable(lgx_telemetry_t* telemetry, bool user_consent);
lgx_result_t lgx_telemetry_record_frame_time(lgx_telemetry_t* telemetry, float frame_time_ms);
lgx_result_t lgx_telemetry_record_memory_usage(lgx_telemetry_t* telemetry, 
                                              size_t memory_usage_mb, size_t pool_usage_mb);
lgx_result_t lgx_telemetry_export(lgx_telemetry_t* telemetry, const char* output_path);

// Utility functions
uint64_t lgx_time_now_ns(void);

lgx_result_t lgx_hardware_adapter_init(lgx_hardware_adapter_t** adapter);
lgx_result_t lgx_hardware_adapter_shutdown(lgx_hardware_adapter_t* adapter);

lgx_result_t lgx_capability_detector_init(lgx_capability_detector_t** detector,
                                         lgx_hardware_adapter_t* hardware_adapter);
lgx_result_t lgx_capability_detector_shutdown(lgx_capability_detector_t* detector);

lgx_result_t lgx_lifecycle_manager_init(lgx_lifecycle_manager_t** manager);
lgx_result_t lgx_lifecycle_manager_shutdown(lgx_lifecycle_manager_t* manager);

lgx_result_t lgx_platform_services_init(lgx_platform_services_t** services,
                                       const lgx_runtime_config_t* config);
lgx_result_t lgx_platform_services_shutdown(lgx_platform_services_t* services);

lgx_result_t lgx_telemetry_init(lgx_telemetry_t** telemetry,
                               const lgx_runtime_config_t* config);
lgx_result_t lgx_telemetry_shutdown(lgx_telemetry_t* telemetry);

lgx_result_t lgx_error_handler_init(lgx_error_handler_t** handler);
lgx_result_t lgx_error_handler_shutdown(lgx_error_handler_t* handler);

lgx_result_t lgx_health_monitor_init(lgx_health_monitor_t** monitor,
                                    lgx_runtime_state_t* runtime_state);
lgx_result_t lgx_health_monitor_shutdown(lgx_health_monitor_t* monitor);

// Platform services global setter
void lgx_set_platform_services(lgx_platform_services_t* services);

#ifdef __cplusplus
}
#endif

#endif // LGX_RUNTIME_INTERNAL_H