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
bool lgx_hardware_adapter_has_gpu(lgx_hardware_adapter_t* adapter);
const char* lgx_hardware_adapter_get_gpu_vendor(lgx_hardware_adapter_t* adapter);
const char* lgx_hardware_adapter_get_driver_version(lgx_hardware_adapter_t* adapter);

// Huge pages API functions (Day 10 Optimization)
lgx_result_t lgx_hugepages_init(lgx_hardware_adapter_t* hardware_adapter);
lgx_result_t lgx_hugepages_shutdown(void);
void* lgx_hugepages_alloc(size_t size);
void lgx_hugepages_free(void* ptr, size_t size);
void* lgx_hugepages_alloc_selective(size_t size, bool is_long_lived, bool is_hot_path);
bool lgx_hugepages_available(void);
double lgx_hugepages_estimate_tlb_improvement(size_t memory_size);
lgx_hardware_status_t lgx_hardware_adapter_get_status(lgx_hardware_adapter_t* adapter);

// Intent allocator API functions (Task 3.4)
lgx_result_t lgx_intent_allocator_init(void);
lgx_result_t lgx_intent_allocator_shutdown(void);
// lgx_intent_stats_t and lgx_intent_get_stats are now in lgx_runtime.h

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

// Lock-free pool API functions (Day 1-2 Breakthrough Optimization)
#define NUM_SIZE_CLASSES 16

lgx_result_t lgx_lockfree_pool_init(const size_t* size_classes, int num_classes);
lgx_result_t lgx_lockfree_pool_shutdown(void);
void lgx_lockfree_push(int size_class, void* ptr);
void* lgx_lockfree_pop(int size_class);
int lgx_lockfree_pop_batch(int size_class, void** blocks, int max_count);
void lgx_lockfree_push_batch(int size_class, void** blocks, int count);
uint32_t lgx_lockfree_pool_get_count(int size_class);
lgx_result_t lgx_lockfree_pool_prewarm(int size_class, int num_blocks);

// SIMD operations API functions (Day 8-9 Breakthrough Optimization)
void lgx_simd_detect_features(void);
bool lgx_simd_has_avx2(void);
int lgx_simd_find_nonempty_slot(void** slots, int count);
bool lgx_simd_all_null(void** slots, int count);
int lgx_simd_count_nonempty(void** slots, int count);

// Frame arena API functions (Month 1 - Specialized Allocators)
typedef struct {
    uint64_t total_allocations;
    uint64_t total_bytes_allocated;
    uint64_t overflow_count;
    uint64_t peak_usage_bytes;
    uint64_t current_frame;
} frame_arena_stats_t;

lgx_result_t lgx_frame_arena_init(void);
lgx_result_t lgx_frame_arena_shutdown(void);
void* lgx_frame_alloc(size_t size);
lgx_result_t lgx_frame_reset(void);
lgx_result_t lgx_frame_get_stats(frame_arena_stats_t* stats);
bool lgx_frame_arena_is_initialized(void);
uint32_t lgx_frame_get_current_frame(void);
size_t lgx_frame_get_current_usage(void);
size_t lgx_frame_get_peak_usage(void);

// GPU memory pool API functions (Month 2 - Specialized Allocators)
// Forward declare Vulkan types to avoid requiring vulkan.h in this header
typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef uint64_t VkDeviceSize;
typedef uint32_t VkMemoryPropertyFlags;

typedef enum {
    LGX_GPU_DEVICE_LOCAL = 0,
    LGX_GPU_HOST_VISIBLE = 1,
    LGX_GPU_HOST_CACHED  = 2,
    LGX_GPU_MEMORY_TYPE_COUNT = 3
} lgx_gpu_memory_type_t;

typedef struct lgx_gpu_allocation lgx_gpu_allocation_t;

lgx_result_t lgx_gpu_pool_init(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device);
lgx_result_t lgx_gpu_pool_shutdown(void);
bool lgx_gpu_pool_is_initialized(void);
bool lgx_gpu_pool_is_memory_type_available(lgx_gpu_memory_type_t type);
VkDeviceSize lgx_gpu_pool_get_memory_budget(lgx_gpu_memory_type_t type);
VkDeviceSize lgx_gpu_pool_get_memory_used(lgx_gpu_memory_type_t type);

// Buddy allocator API (Task 3.2.2 & 3.2.3)
lgx_gpu_allocation_t* lgx_gpu_alloc(VkDeviceSize size, VkDeviceSize alignment, lgx_gpu_memory_type_t type);
void lgx_gpu_free(lgx_gpu_allocation_t* alloc);
VkDeviceMemory lgx_gpu_get_memory(lgx_gpu_allocation_t* alloc);
VkDeviceSize lgx_gpu_get_offset(lgx_gpu_allocation_t* alloc);
VkDeviceSize lgx_gpu_get_size(lgx_gpu_allocation_t* alloc);
void* lgx_gpu_get_mapped_ptr(lgx_gpu_allocation_t* alloc);
float lgx_gpu_pool_get_fragmentation(lgx_gpu_memory_type_t type);
VkDeviceSize lgx_gpu_pool_get_peak_usage(lgx_gpu_memory_type_t type);

// GPU capability detection API (Task 3.5.2.3)
// GPU capability structure
typedef struct lgx_gpu_capabilities {
    bool supports_device_local;      // Device-only memory (fastest)
    bool supports_host_visible;      // CPU-accessible memory
    bool supports_host_cached;       // Cached host-visible memory
    bool supports_host_coherent;     // Coherent host-visible memory
    bool supports_resizable_bar;     // ReBAR (large host-visible memory)
    size_t max_device_local_mb;      // How much VRAM available
    size_t max_host_visible_mb;      // How much host-visible available
    uint32_t device_local_heap_index;
    uint32_t host_visible_heap_index;
} lgx_gpu_capabilities_t;

// GPU allocation strategy
typedef enum lgx_gpu_allocation_strategy {
    LGX_GPU_STRATEGY_DEVICE_LOCAL = 0,   // Fastest, GPU-only
    LGX_GPU_STRATEGY_HOST_VISIBLE = 1,   // Slower, CPU-accessible
    LGX_GPU_STRATEGY_RESIZABLE_BAR = 2,  // ReBAR: Large host-visible
    LGX_GPU_STRATEGY_FALLBACK = 3,       // Minimum viable
} lgx_gpu_allocation_strategy_t;

lgx_result_t lgx_gpu_detect_capabilities(VkPhysicalDevice physical_device, 
                                         lgx_gpu_capabilities_t* caps);
lgx_gpu_allocation_strategy_t lgx_gpu_select_strategy(const lgx_gpu_capabilities_t* caps);
size_t lgx_gpu_get_recommended_pool_size(lgx_gpu_allocation_strategy_t strategy,
                                         const lgx_gpu_capabilities_t* caps);
VkMemoryPropertyFlags lgx_gpu_get_memory_type_flags(lgx_gpu_allocation_strategy_t strategy);
void lgx_gpu_print_capabilities(const lgx_gpu_capabilities_t* caps);
void lgx_gpu_print_strategy(lgx_gpu_allocation_strategy_t strategy);

// Persistent heap API functions (Month 3 - Specialized Allocators)
typedef struct {
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t active_allocations;
    uint64_t current_bytes;
    uint64_t peak_bytes;
    float fragmentation_ratio;
} lgx_heap_stats_t;

lgx_result_t lgx_persistent_heap_init(void);
lgx_result_t lgx_persistent_heap_shutdown(void);
void* lgx_heap_alloc(size_t size);
void lgx_heap_free(void* ptr);
bool lgx_persistent_heap_is_initialized(void);
lgx_result_t lgx_heap_get_stats(lgx_heap_stats_t* stats);
float lgx_heap_get_fragmentation(void);
uint64_t lgx_heap_defragment(uint64_t time_budget_ns);
float lgx_heap_get_defrag_progress(void);
int lgx_heap_health_check(void);
void lgx_heap_get_error_stats(uint64_t* oom_errors, uint64_t* rate_limit_errors, 
                               uint64_t* validation_errors);

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

// Performance counter API functions (Section 5.2)
lgx_result_t lgx_counters_init(void);
lgx_result_t lgx_counters_shutdown(void);
void lgx_increment_builtin_counter(lgx_counter_t counter);
void lgx_add_to_builtin_counter(lgx_counter_t counter, uint64_t value);
uint64_t lgx_get_custom_counter(lgx_custom_counter_t counter);
const char* lgx_get_custom_counter_name(lgx_custom_counter_t counter);

#ifdef __cplusplus
}
#endif

#endif // LGX_RUNTIME_INTERNAL_H