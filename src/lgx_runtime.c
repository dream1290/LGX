#define _GNU_SOURCE
#include "lgx_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/sysinfo.h>

// Intent validation constants
#define MAX_TRACKED_ALLOCATIONS 10000
#define INTENT_VALIDATION_SAMPLE_THRESHOLD 100  // Validate after 100 accesses
#define PATTERN_CONFIDENCE_THRESHOLD 0.8        // 80% confidence for pattern detection

// Allocation tracking structure (internal)
typedef struct allocation_tracking {
    void* ptr;
    size_t size;
    lgx_allocation_intent_base_t intent;
    lgx_allocation_usage_t usage;
    bool is_active;
    uint64_t sequential_access_count;
    uint64_t random_access_count;
    void* last_accessed_address;
    pthread_mutex_t access_mutex;
} allocation_tracking_t;

// Configuration structure
struct lgx_runtime_config {
    char* log_path;
    size_t memory_pool_size;
    uint32_t flags;
    size_t frame_arena_size;      // Task 3.4.5.2.1
    size_t frame_arena_max_size;  // Task 3.4.5.2.2
};

// Global runtime state
static struct {
    bool initialized;
    pthread_mutex_t mutex;
    lgx_performance_characteristics_t perf_chars;
    lgx_memory_stats_t memory_stats;
    uint64_t init_start_time;
    uint64_t init_end_time;
    char kernel_version[256];
    char cpu_model[256];
    
    // Intent validation tracking
    allocation_tracking_t tracked_allocations[MAX_TRACKED_ALLOCATIONS];
    size_t tracked_allocation_count;
    pthread_mutex_t tracking_mutex;
    
    // Intent validation statistics
    uint64_t total_intent_validations;
    uint64_t intent_mismatches;
    uint64_t pattern_adaptations;
    
    // Hardware status
    lgx_hardware_status_t hardware_status;
    uint32_t available_capabilities;
} g_runtime = {
    .initialized = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .tracking_mutex = PTHREAD_MUTEX_INITIALIZER,
    .tracked_allocation_count = 0,
    .total_intent_validations = 0,
    .intent_mismatches = 0,
    .pattern_adaptations = 0,
    .available_capabilities = 0
};

// Helper function to get high-resolution timestamp
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Intent validation helper functions
static allocation_tracking_t* find_allocation_tracking(void* ptr) {
    for (size_t i = 0; i < g_runtime.tracked_allocation_count; i++) {
        if (g_runtime.tracked_allocations[i].ptr == ptr && 
            g_runtime.tracked_allocations[i].is_active) {
            return &g_runtime.tracked_allocations[i];
        }
    }
    return NULL;
}

static allocation_tracking_t* add_allocation_tracking(void* ptr, size_t size, 
                                                     const lgx_allocation_intent_base_t* intent) {
    if (g_runtime.tracked_allocation_count >= MAX_TRACKED_ALLOCATIONS) {
        // Find an inactive slot to reuse
        for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
            if (!g_runtime.tracked_allocations[i].is_active) {
                allocation_tracking_t* tracking = &g_runtime.tracked_allocations[i];
                memset(tracking, 0, sizeof(allocation_tracking_t));
                tracking->ptr = ptr;
                tracking->size = size;
                if (intent) {
                    tracking->intent = *intent;
                } else {
                    // Default intent for regular allocations
                    tracking->intent.struct_size = sizeof(lgx_allocation_intent_base_t);
                    tracking->intent.size = size;
                    tracking->intent.access_pattern = LGX_ACCESS_UNKNOWN;
                    tracking->intent.lifetime = LGX_LIFETIME_UNKNOWN;
                    tracking->intent.hint = LGX_HINT_BACKGROUND;
                    tracking->intent.validation_policy = LGX_INTENT_TRUST;
                }
                tracking->is_active = true;
                tracking->usage.struct_size = sizeof(lgx_allocation_usage_t);
                tracking->usage.allocation_timestamp_ns = get_timestamp_ns();
                tracking->usage.observed_pattern = LGX_ACCESS_UNKNOWN;
                tracking->usage.observed_lifetime = LGX_LIFETIME_UNKNOWN;
                pthread_mutex_init(&tracking->access_mutex, NULL);
                return tracking;
            }
        }
        return NULL; // No space available
    }
    
    allocation_tracking_t* tracking = &g_runtime.tracked_allocations[g_runtime.tracked_allocation_count++];
    memset(tracking, 0, sizeof(allocation_tracking_t));
    tracking->ptr = ptr;
    tracking->size = size;
    if (intent) {
        tracking->intent = *intent;
    } else {
        // Default intent for regular allocations
        tracking->intent.struct_size = sizeof(lgx_allocation_intent_base_t);
        tracking->intent.size = size;
        tracking->intent.access_pattern = LGX_ACCESS_UNKNOWN;
        tracking->intent.lifetime = LGX_LIFETIME_UNKNOWN;
        tracking->intent.hint = LGX_HINT_BACKGROUND;
        tracking->intent.validation_policy = LGX_INTENT_TRUST;
    }
    tracking->is_active = true;
    tracking->usage.struct_size = sizeof(lgx_allocation_usage_t);
    tracking->usage.allocation_timestamp_ns = get_timestamp_ns();
    tracking->usage.observed_pattern = LGX_ACCESS_UNKNOWN;
    tracking->usage.observed_lifetime = LGX_LIFETIME_UNKNOWN;
    pthread_mutex_init(&tracking->access_mutex, NULL);
    return tracking;
}

static void remove_allocation_tracking(void* ptr) {
    allocation_tracking_t* tracking = find_allocation_tracking(ptr);
    if (tracking) {
        tracking->is_active = false;
        pthread_mutex_destroy(&tracking->access_mutex);
    }
}

static void analyze_access_pattern(allocation_tracking_t* tracking, void* accessed_address) {
    pthread_mutex_lock(&tracking->access_mutex);
    
    uint64_t now = get_timestamp_ns();
    tracking->usage.access_count++;
    tracking->usage.last_access_timestamp_ns = now;
    
    if (tracking->usage.first_access_timestamp_ns == 0) {
        tracking->usage.first_access_timestamp_ns = now;
    }
    
    // Analyze access pattern
    if (tracking->last_accessed_address != NULL) {
        ptrdiff_t offset = (char*)accessed_address - (char*)tracking->last_accessed_address;
        if (llabs(offset) <= 64) { // Within cache line - likely sequential
            tracking->sequential_access_count++;
        } else {
            tracking->random_access_count++;
        }
    }
    
    tracking->last_accessed_address = accessed_address;
    
    // Update observed pattern based on access statistics
    if (tracking->usage.access_count >= 10) { // Need some samples
        double sequential_ratio = (double)tracking->sequential_access_count / 
                                 (tracking->sequential_access_count + tracking->random_access_count);
        
        if (sequential_ratio > 0.8) {
            tracking->usage.observed_pattern = LGX_ACCESS_SEQUENTIAL;
            tracking->usage.pattern_confidence = sequential_ratio;
        } else if (sequential_ratio < 0.2) {
            tracking->usage.observed_pattern = LGX_ACCESS_RANDOM;
            tracking->usage.pattern_confidence = 1.0 - sequential_ratio;
        } else {
            tracking->usage.observed_pattern = LGX_ACCESS_UNKNOWN;
            tracking->usage.pattern_confidence = 0.5;
        }
    }
    
    pthread_mutex_unlock(&tracking->access_mutex);
}

static bool validate_intent_vs_usage(allocation_tracking_t* tracking) {
    if (tracking->intent.validation_policy == LGX_INTENT_TRUST) {
        return true; // No validation requested
    }
    
    if (tracking->usage.access_count < INTENT_VALIDATION_SAMPLE_THRESHOLD) {
        return true; // Not enough samples yet
    }
    
    bool mismatch = false;
    
    // Check access pattern mismatch
    if (tracking->intent.access_pattern != LGX_ACCESS_UNKNOWN &&
        tracking->usage.observed_pattern != LGX_ACCESS_UNKNOWN &&
        tracking->intent.access_pattern != tracking->usage.observed_pattern &&
        tracking->usage.pattern_confidence > PATTERN_CONFIDENCE_THRESHOLD) {
        mismatch = true;
    }
    
    // Check lifetime mismatch (simplified - just check if still alive after expected lifetime)
    uint64_t current_lifetime_ms = (get_timestamp_ns() - tracking->usage.allocation_timestamp_ns) / 1000000;
    if (tracking->intent.lifetime == LGX_LIFETIME_FRAME && current_lifetime_ms > 33) { // >33ms = multiple frames
        mismatch = true;
    } else if (tracking->intent.lifetime == LGX_LIFETIME_LEVEL && current_lifetime_ms > 300000) { // >5 minutes
        mismatch = true;
    }
    
    if (mismatch) {
        tracking->usage.intent_mismatch_detected = true;
        g_runtime.intent_mismatches++;
        
        if (tracking->intent.validation_policy == LGX_INTENT_VALIDATE_ADAPT) {
            // Adapt the allocation strategy (placeholder for now)
            g_runtime.pattern_adaptations++;
        }
    }
    
    g_runtime.total_intent_validations++;
    return !mismatch;
}

// Helper function to detect system information
static void detect_system_info(void) {
    struct utsname uname_info;
    if (uname(&uname_info) == 0) {
        snprintf(g_runtime.kernel_version, sizeof(g_runtime.kernel_version), 
                "%s %s", uname_info.sysname, uname_info.release);
    } else {
        strcpy(g_runtime.kernel_version, "Unknown");
    }
    
    // Try to read CPU model from /proc/cpuinfo
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    colon += 2; // Skip ": "
                    // Remove newline
                    char* newline = strchr(colon, '\n');
                    if (newline) *newline = '\0';
                    strncpy(g_runtime.cpu_model, colon, sizeof(g_runtime.cpu_model) - 1);
                    g_runtime.cpu_model[sizeof(g_runtime.cpu_model) - 1] = '\0';
                    break;
                }
            }
        }
        fclose(cpuinfo);
    }
    
    if (strlen(g_runtime.cpu_model) == 0) {
        strcpy(g_runtime.cpu_model, "Unknown CPU");
    }
}

// Hardware detection functions
static bool detect_huge_pages(void) {
    // Check if huge pages are available
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) return false;
    
    char line[256];
    bool huge_pages_available = false;
    
    while (fgets(line, sizeof(line), meminfo)) {
        if (strncmp(line, "HugePages_Total:", 16) == 0) {
            int total_pages = 0;
            if (sscanf(line, "HugePages_Total: %d", &total_pages) == 1) {
                huge_pages_available = (total_pages > 0);
            }
            break;
        }
    }
    
    fclose(meminfo);
    return huge_pages_available;
}

static uint32_t detect_numa_topology(void) {
    // Check NUMA topology
    FILE* numa_info = fopen("/sys/devices/system/node/possible", "r");
    if (!numa_info) return 1; // Single node
    
    char line[64];
    uint32_t max_node = 0;
    
    if (fgets(line, sizeof(line), numa_info)) {
        // Parse format like "0-3" or "0"
        char* dash = strchr(line, '-');
        if (dash) {
            max_node = atoi(dash + 1);
        } else {
            max_node = atoi(line);
        }
    }
    
    fclose(numa_info);
    return max_node + 1; // Convert to count
}

static bool detect_gpu_acceleration(char* gpu_vendor, size_t vendor_size) {
    // Try to detect GPU via /proc/driver/nvidia/version or similar
    FILE* nvidia_version = fopen("/proc/driver/nvidia/version", "r");
    if (nvidia_version) {
        strncpy(gpu_vendor, "NVIDIA", vendor_size - 1);
        gpu_vendor[vendor_size - 1] = '\0';
        fclose(nvidia_version);
        return true;
    }
    
    // Check for AMD GPU
    FILE* amd_info = fopen("/sys/class/drm/card0/device/vendor", "r");
    if (amd_info) {
        char vendor_id[16];
        if (fgets(vendor_id, sizeof(vendor_id), amd_info)) {
            if (strstr(vendor_id, "0x1002")) { // AMD vendor ID
                strncpy(gpu_vendor, "AMD", vendor_size - 1);
                gpu_vendor[vendor_size - 1] = '\0';
                fclose(amd_info);
                return true;
            }
        }
        fclose(amd_info);
    }
    
    // Check for Intel GPU
    FILE* intel_info = fopen("/sys/class/drm/card0/device/vendor", "r");
    if (intel_info) {
        char vendor_id[16];
        if (fgets(vendor_id, sizeof(vendor_id), intel_info)) {
            if (strstr(vendor_id, "0x8086")) { // Intel vendor ID
                strncpy(gpu_vendor, "Intel", vendor_size - 1);
                gpu_vendor[vendor_size - 1] = '\0';
                fclose(intel_info);
                return true;
            }
        }
        fclose(intel_info);
    }
    
    strncpy(gpu_vendor, "Unknown", vendor_size - 1);
    gpu_vendor[vendor_size - 1] = '\0';
    return false;
}

static void detect_cpu_features(char* features, size_t features_size) {
    // Read CPU features from /proc/cpuinfo
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) {
        strncpy(features, "Unknown", features_size - 1);
        features[features_size - 1] = '\0';
        return;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), cpuinfo)) {
        if (strncmp(line, "flags", 5) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                colon += 2; // Skip ": "
                char* newline = strchr(colon, '\n');
                if (newline) *newline = '\0';
                
                // Extract key features
                char key_features[256] = "";
                if (strstr(colon, "avx2")) strcat(key_features, "AVX2 ");
                if (strstr(colon, "avx")) strcat(key_features, "AVX ");
                if (strstr(colon, "sse4_2")) strcat(key_features, "SSE4.2 ");
                if (strstr(colon, "popcnt")) strcat(key_features, "POPCNT ");
                if (strstr(colon, "rdrand")) strcat(key_features, "RDRAND ");
                
                if (strlen(key_features) > 0) {
                    key_features[strlen(key_features) - 1] = '\0'; // Remove trailing space
                    strncpy(features, key_features, features_size - 1);
                } else {
                    strncpy(features, "Basic", features_size - 1);
                }
                features[features_size - 1] = '\0';
                break;
            }
        }
    }
    
    fclose(cpuinfo);
}

static lgx_hardware_tier_t classify_hardware_tier(const lgx_hardware_status_t* status) {
    uint32_t optimal_features = LGX_CAP_HUGE_PAGES | LGX_CAP_GPU_ACCELERATION | LGX_CAP_FAST_ALLOCATOR;
    uint32_t compatible_features = LGX_CAP_FAST_ALLOCATOR;
    
    uint32_t available = g_runtime.available_capabilities;
    
    if ((available & optimal_features) == optimal_features) {
        return LGX_HW_TIER_OPTIMAL;
    } else if ((available & compatible_features) == compatible_features) {
        return LGX_HW_TIER_COMPATIBLE;
    } else {
        return LGX_HW_TIER_DEGRADED;
    }
}

static void detect_hardware_capabilities(void) {
    // Initialize hardware status
    memset(&g_runtime.hardware_status, 0, sizeof(g_runtime.hardware_status));
    g_runtime.hardware_status.struct_size = sizeof(lgx_hardware_status_t);
    
    // Detect hardware features
    g_runtime.hardware_status.huge_pages_available = detect_huge_pages();
    g_runtime.hardware_status.numa_node_count = detect_numa_topology();
    g_runtime.hardware_status.numa_topology_detected = (g_runtime.hardware_status.numa_node_count > 1);
    
    char gpu_vendor[64];
    g_runtime.hardware_status.gpu_acceleration_available = detect_gpu_acceleration(gpu_vendor, sizeof(gpu_vendor));
    g_runtime.hardware_status.gpu_vendor = strdup(gpu_vendor);
    
    char cpu_features[256];
    detect_cpu_features(cpu_features, sizeof(cpu_features));
    g_runtime.hardware_status.cpu_features = strdup(cpu_features);
    
    // Set capability flags
    g_runtime.available_capabilities = 0;
    
    if (g_runtime.hardware_status.huge_pages_available) {
        g_runtime.available_capabilities |= LGX_CAP_HUGE_PAGES;
    }
    
    if (g_runtime.hardware_status.numa_topology_detected) {
        g_runtime.available_capabilities |= LGX_CAP_NUMA_AWARENESS;
    }
    
    if (g_runtime.hardware_status.gpu_acceleration_available) {
        g_runtime.available_capabilities |= LGX_CAP_GPU_ACCELERATION;
    }
    
    // Always available in prototype
    g_runtime.available_capabilities |= LGX_CAP_FAST_ALLOCATOR;
    g_runtime.available_capabilities |= LGX_CAP_TELEMETRY;
    
    // Classify hardware tier
    g_runtime.hardware_status.achieved_tier = classify_hardware_tier(&g_runtime.hardware_status);
    
    // Set degradation information based on tier
    uint32_t missing = 0;
    const char* reason = "";
    const char* impact = "";
    const char* remediation = "";
    
    switch (g_runtime.hardware_status.achieved_tier) {
        case LGX_HW_TIER_OPTIMAL:
            reason = "All hardware features available";
            impact = "Optimal performance";
            remediation = "No action needed";
            break;
            
        case LGX_HW_TIER_COMPATIBLE:
            if (!g_runtime.hardware_status.huge_pages_available) {
                missing |= LGX_CAP_HUGE_PAGES;
                reason = "Huge pages not available";
                impact = "5-10% performance penalty";
                remediation = "Enable huge pages: echo 128 > /proc/sys/vm/nr_hugepages";
            }
            if (!g_runtime.hardware_status.gpu_acceleration_available) {
                missing |= LGX_CAP_GPU_ACCELERATION;
                reason = "GPU acceleration not detected";
                impact = "Software fallback for GPU operations";
                remediation = "Install GPU drivers and ensure GPU is accessible";
            }
            break;
            
        case LGX_HW_TIER_DEGRADED:
            missing = (LGX_CAP_HUGE_PAGES | LGX_CAP_GPU_ACCELERATION);
            reason = "Multiple hardware features missing";
            impact = "20-30% performance penalty";
            remediation = "Enable huge pages and install GPU drivers";
            break;
    }
    
    g_runtime.hardware_status.missing_capabilities = missing;
    g_runtime.hardware_status.degradation_reason = strdup(reason);
    g_runtime.hardware_status.performance_impact_estimate = strdup(impact);
    g_runtime.hardware_status.remediation_steps = strdup(remediation);
}

// Configuration API
lgx_runtime_config_t* lgx_config_create(void) {
    lgx_runtime_config_t* config = malloc(sizeof(lgx_runtime_config_t));
    if (!config) return NULL;
    
    memset(config, 0, sizeof(lgx_runtime_config_t));
    config->memory_pool_size = 200 * 1024 * 1024; // 200MB default
    return config;
}

void lgx_config_set_log_path(lgx_runtime_config_t* config, const char* path) {
    if (!config || !path) return;
    
    free(config->log_path);
    config->log_path = strdup(path);
}

void lgx_config_set_memory_pool_size(lgx_runtime_config_t* config, size_t size) {
    if (!config) return;
    config->memory_pool_size = size;
}

void lgx_config_set_flags(lgx_runtime_config_t* config, uint32_t flags) {
    if (!config) return;
    config->flags = flags;
}

void lgx_config_destroy(lgx_runtime_config_t* config) {
    if (!config) return;
    
    free(config->log_path);
    free(config);
}

// Core runtime functions
lgx_result_t lgx_runtime_init(const lgx_runtime_config_t* config) {
    pthread_mutex_lock(&g_runtime.mutex);
    
    if (g_runtime.initialized) {
        pthread_mutex_unlock(&g_runtime.mutex);
        return LGX_ERROR_ALREADY_INITIALIZED;
    }
    
    // Record start time for performance measurement
    g_runtime.init_start_time = get_timestamp_ns();
    
    // Simulate initialization work
    // 1. Detect system information
    detect_system_info();
    
    // 2. Detect hardware capabilities and classify tier
    detect_hardware_capabilities();
    
    // 3. Initialize memory pools (simulated)
    memset(&g_runtime.memory_stats, 0, sizeof(g_runtime.memory_stats));
    g_runtime.memory_stats.struct_size = sizeof(g_runtime.memory_stats);
    
    // 4. Simulate some initialization delay (realistic for library loading, GPU detection)
    usleep(50000); // 50ms simulated initialization time
    
    // 5. Initialize performance characteristics
    memset(&g_runtime.perf_chars, 0, sizeof(g_runtime.perf_chars));
    g_runtime.perf_chars.struct_size = sizeof(g_runtime.perf_chars);
    g_runtime.perf_chars.kernel_version = g_runtime.kernel_version;
    g_runtime.perf_chars.cpu_model = g_runtime.cpu_model;
    g_runtime.perf_chars.real_time_kernel = false; // TODO: detect PREEMPT_RT
    g_runtime.perf_chars.cpu_isolation = false;    // TODO: detect isolcpus
    g_runtime.perf_chars.measurement_conditions = "Normal system load";
    
    // Record end time
    g_runtime.init_end_time = get_timestamp_ns();
    
    // Calculate initialization time statistics (single sample for now)
    uint64_t init_time_ns = g_runtime.init_end_time - g_runtime.init_start_time;
    g_runtime.perf_chars.measured_init_time.p50_ns = init_time_ns;
    g_runtime.perf_chars.measured_init_time.p95_ns = init_time_ns;
    g_runtime.perf_chars.measured_init_time.p99_ns = init_time_ns;
    g_runtime.perf_chars.measured_init_time.p999_ns = init_time_ns;
    g_runtime.perf_chars.measured_init_time.confidence_interval = 1.0; // Single sample
    g_runtime.perf_chars.measured_init_time.sample_size = 1;
    
    g_runtime.initialized = true;
    
    pthread_mutex_unlock(&g_runtime.mutex);
    return LGX_SUCCESS;
}

lgx_result_t lgx_runtime_shutdown(void) {
    pthread_mutex_lock(&g_runtime.mutex);
    
    if (!g_runtime.initialized) {
        pthread_mutex_unlock(&g_runtime.mutex);
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    g_runtime.initialized = false;
    
    pthread_mutex_unlock(&g_runtime.mutex);
    return LGX_SUCCESS;
}

// Version and compatibility
lgx_version_t lgx_runtime_get_version(void) {
    lgx_version_t version;
    version.struct_size = sizeof(lgx_version_t);
    version.major = LGX_VERSION_MAJOR;
    version.minor = LGX_VERSION_MINOR;
    version.patch = LGX_VERSION_PATCH;
    return version;
}

lgx_result_t lgx_runtime_check_compatibility(const lgx_version_t* required_version) {
    if (!required_version) return LGX_ERROR_INVALID_PARAM;
    
    // Check major version compatibility
    if (required_version->major != LGX_VERSION_MAJOR) {
        return LGX_ERROR_INCOMPATIBLE_VERSION;
    }
    
    // Minor version must be <= current version
    if (required_version->minor > LGX_VERSION_MINOR) {
        return LGX_ERROR_INCOMPATIBLE_VERSION;
    }
    
    return LGX_SUCCESS;
}

// Simple memory management (for prototype)
void* lgx_alloc(size_t size) {
    if (!g_runtime.initialized) return NULL;
    if (size == 0) return NULL;
    
    uint64_t start_time = get_timestamp_ns();
    void* ptr = malloc(size);
    uint64_t end_time = get_timestamp_ns();
    
    if (ptr) {
        pthread_mutex_lock(&g_runtime.mutex);
        g_runtime.memory_stats.allocation_count++;
        g_runtime.memory_stats.current_allocated += size;
        g_runtime.memory_stats.total_allocated += size;
        if (g_runtime.memory_stats.current_allocated > g_runtime.memory_stats.peak_allocated) {
            g_runtime.memory_stats.peak_allocated = g_runtime.memory_stats.current_allocated;
        }
        
        // Update allocation time statistics (simple running average for prototype)
        uint64_t alloc_time = end_time - start_time;
        if (g_runtime.perf_chars.measured_alloc_time.sample_size == 0) {
            g_runtime.perf_chars.measured_alloc_time.p50_ns = alloc_time;
            g_runtime.perf_chars.measured_alloc_time.p95_ns = alloc_time;
            g_runtime.perf_chars.measured_alloc_time.p99_ns = alloc_time;
            g_runtime.perf_chars.measured_alloc_time.p999_ns = alloc_time;
        } else {
            // Simple running average (not statistically accurate, but good for prototype)
            g_runtime.perf_chars.measured_alloc_time.p50_ns = 
                (g_runtime.perf_chars.measured_alloc_time.p50_ns + alloc_time) / 2;
        }
        g_runtime.perf_chars.measured_alloc_time.sample_size++;
        g_runtime.perf_chars.measured_alloc_time.confidence_interval = 0.95;
        
        pthread_mutex_unlock(&g_runtime.mutex);
        
        // Add tracking for regular allocations (with default intent)
        pthread_mutex_lock(&g_runtime.tracking_mutex);
        add_allocation_tracking(ptr, size, NULL);
        pthread_mutex_unlock(&g_runtime.tracking_mutex);
    }
    
    return ptr;
}

void* lgx_alloc_aligned(size_t size, size_t alignment) {
    if (!g_runtime.initialized) return NULL;
    if (size == 0) return NULL;
    
    void* ptr;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    
    // Update statistics (simplified)
    pthread_mutex_lock(&g_runtime.mutex);
    g_runtime.memory_stats.allocation_count++;
    g_runtime.memory_stats.current_allocated += size;
    g_runtime.memory_stats.total_allocated += size;
    if (g_runtime.memory_stats.current_allocated > g_runtime.memory_stats.peak_allocated) {
        g_runtime.memory_stats.peak_allocated = g_runtime.memory_stats.current_allocated;
    }
    pthread_mutex_unlock(&g_runtime.mutex);
    
    // Add tracking for aligned allocations
    pthread_mutex_lock(&g_runtime.tracking_mutex);
    add_allocation_tracking(ptr, size, NULL);
    pthread_mutex_unlock(&g_runtime.tracking_mutex);
    
    return ptr;
}

void* lgx_alloc_with_intent(const lgx_allocation_intent_base_t* intent) {
    if (!g_runtime.initialized || !intent) return NULL;
    if (intent->size == 0) return NULL;
    
    // Validate intent structure
    if (intent->struct_size < sizeof(lgx_allocation_intent_base_t)) {
        return NULL;
    }
    
    uint64_t start_time = get_timestamp_ns();
    
    // For prototype, use regular malloc but track the intent
    void* ptr = malloc(intent->size);
    
    uint64_t end_time = get_timestamp_ns();
    
    if (ptr) {
        pthread_mutex_lock(&g_runtime.mutex);
        g_runtime.memory_stats.allocation_count++;
        g_runtime.memory_stats.current_allocated += intent->size;
        g_runtime.memory_stats.total_allocated += intent->size;
        if (g_runtime.memory_stats.current_allocated > g_runtime.memory_stats.peak_allocated) {
            g_runtime.memory_stats.peak_allocated = g_runtime.memory_stats.current_allocated;
        }
        
        // Update allocation time statistics
        uint64_t alloc_time = end_time - start_time;
        if (g_runtime.perf_chars.measured_alloc_time.sample_size == 0) {
            g_runtime.perf_chars.measured_alloc_time.p50_ns = alloc_time;
            g_runtime.perf_chars.measured_alloc_time.p95_ns = alloc_time;
            g_runtime.perf_chars.measured_alloc_time.p99_ns = alloc_time;
            g_runtime.perf_chars.measured_alloc_time.p999_ns = alloc_time;
        } else {
            g_runtime.perf_chars.measured_alloc_time.p50_ns = 
                (g_runtime.perf_chars.measured_alloc_time.p50_ns + alloc_time) / 2;
        }
        g_runtime.perf_chars.measured_alloc_time.sample_size++;
        
        pthread_mutex_unlock(&g_runtime.mutex);
        
        // Add tracking with the provided intent
        pthread_mutex_lock(&g_runtime.tracking_mutex);
        add_allocation_tracking(ptr, intent->size, intent);
        pthread_mutex_unlock(&g_runtime.tracking_mutex);
    }
    
    return ptr;
}

void* lgx_alloc_with_intent_ex(const void* intent, size_t intent_type_id) {
    if (!intent) return NULL;
    
    if (intent_type_id == 0) {
        // Base intent type
        return lgx_alloc_with_intent((const lgx_allocation_intent_base_t*)intent);
    } else if (intent_type_id == 1) {
        // L2 intent type
        const lgx_allocation_intent_l2_t* l2_intent = (const lgx_allocation_intent_l2_t*)intent;
        
        // Validate L2 structure
        if (l2_intent->struct_size < sizeof(lgx_allocation_intent_l2_t)) {
            return NULL;
        }
        
        // For prototype, allocate using base intent but track L2 extensions
        void* ptr = lgx_alloc_with_intent(&l2_intent->base);
        
        if (ptr) {
            // Find the tracking entry and add L2-specific information
            pthread_mutex_lock(&g_runtime.tracking_mutex);
            allocation_tracking_t* tracking = find_allocation_tracking(ptr);
            if (tracking) {
                // Store L2-specific data (for future use)
                // In a real implementation, this would influence allocation strategy
                printf("  L2 Intent: priority=%d, prefetch=%s\n", 
                       l2_intent->priority, 
                       l2_intent->enable_predictive_prefetch ? "enabled" : "disabled");
            }
            pthread_mutex_unlock(&g_runtime.tracking_mutex);
        }
        
        return ptr;
    }
    
    // Unknown intent type
    return NULL;
}

void lgx_free(void* ptr) {
    if (!ptr || !g_runtime.initialized) return;
    
    // Remove tracking before freeing
    pthread_mutex_lock(&g_runtime.tracking_mutex);
    allocation_tracking_t* tracking = find_allocation_tracking(ptr);
    if (tracking) {
        // Update lifetime information
        uint64_t now = get_timestamp_ns();
        tracking->usage.actual_lifetime_ms = (now - tracking->usage.allocation_timestamp_ns) / 1000000;
        
        // Determine observed lifetime category
        if (tracking->usage.actual_lifetime_ms <= 33) {
            tracking->usage.observed_lifetime = LGX_LIFETIME_FRAME;
        } else if (tracking->usage.actual_lifetime_ms <= 300000) { // 5 minutes
            tracking->usage.observed_lifetime = LGX_LIFETIME_LEVEL;
        } else {
            tracking->usage.observed_lifetime = LGX_LIFETIME_SESSION;
        }
        
        // Perform final validation
        validate_intent_vs_usage(tracking);
    }
    remove_allocation_tracking(ptr);
    pthread_mutex_unlock(&g_runtime.tracking_mutex);
    
    // Note: In a real implementation, we'd track allocation sizes
    // For prototype, we'll just update the counter
    pthread_mutex_lock(&g_runtime.mutex);
    g_runtime.memory_stats.deallocation_count++;
    pthread_mutex_unlock(&g_runtime.mutex);
    
    free(ptr);
}

// Intent validation and learning functions
lgx_result_t lgx_alloc_get_usage_stats(void* ptr, lgx_allocation_usage_t* usage) {
    if (!ptr || !usage || !g_runtime.initialized) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_runtime.tracking_mutex);
    allocation_tracking_t* tracking = find_allocation_tracking(ptr);
    if (!tracking) {
        pthread_mutex_unlock(&g_runtime.tracking_mutex);
        return LGX_ERROR_INVALID_PARAM;
    }
    
    *usage = tracking->usage;
    pthread_mutex_unlock(&g_runtime.tracking_mutex);
    
    return LGX_SUCCESS;
}

lgx_result_t lgx_alloc_validate_intent(void* ptr) {
    if (!ptr || !g_runtime.initialized) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_runtime.tracking_mutex);
    allocation_tracking_t* tracking = find_allocation_tracking(ptr);
    if (!tracking) {
        pthread_mutex_unlock(&g_runtime.tracking_mutex);
        return LGX_ERROR_INVALID_PARAM;
    }
    
    bool valid = validate_intent_vs_usage(tracking);
    pthread_mutex_unlock(&g_runtime.tracking_mutex);
    
    return valid ? LGX_SUCCESS : LGX_ERROR_INTENT_VALIDATION_FAILED;
}

lgx_result_t lgx_memory_stats(lgx_memory_stats_t* stats) {
    if (!stats || !g_runtime.initialized) return LGX_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&g_runtime.mutex);
    *stats = g_runtime.memory_stats;
    pthread_mutex_unlock(&g_runtime.mutex);
    
    return LGX_SUCCESS;
}

// Performance measurement and assessment
lgx_result_t lgx_runtime_get_performance_characteristics(
    lgx_performance_characteristics_t* chars
) {
    if (!chars || !g_runtime.initialized) return LGX_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&g_runtime.mutex);
    *chars = g_runtime.perf_chars;
    pthread_mutex_unlock(&g_runtime.mutex);
    
    return LGX_SUCCESS;
}

lgx_performance_targets_t lgx_runtime_get_performance_targets(void) {
    lgx_performance_targets_t targets;
    targets.struct_size = sizeof(lgx_performance_targets_t);
    
    // Initialization time targets
    targets.init_time_tier1_ns = 1000000000ULL;  // 1000ms
    targets.init_time_tier2_ns = 500000000ULL;   // 500ms
    targets.init_time_tier3_ns = 100000000ULL;   // 100ms
    
    // Memory usage targets
    targets.memory_tier1_bytes = 300 * 1024 * 1024;  // 300MB
    targets.memory_tier2_bytes = 200 * 1024 * 1024;  // 200MB
    targets.memory_tier3_bytes = 100 * 1024 * 1024;  // 100MB
    
    // Allocation latency targets
    targets.alloc_latency_tier1_ns = 5000ULL;    // 5μs
    targets.alloc_latency_tier2_ns = 1000ULL;    // 1μs
    targets.alloc_latency_tier3_ns = 500ULL;     // 500ns
    
    return targets;
}

static lgx_performance_tier_t assess_metric_tier(uint64_t measured, uint64_t tier1, uint64_t tier2, uint64_t tier3) {
    if (measured <= tier3) return LGX_PERF_TIER_3;
    if (measured <= tier2) return LGX_PERF_TIER_2;
    if (measured <= tier1) return LGX_PERF_TIER_1;
    return LGX_PERF_TIER_1; // Default to tier 1 if exceeds all targets
}

static double calculate_margin_percent(uint64_t measured, uint64_t target) {
    if (target == 0) return 0.0;
    return ((double)target - (double)measured) / (double)target * 100.0;
}

lgx_result_t lgx_runtime_assess_performance(lgx_performance_assessment_t* assessment) {
    if (!assessment || !g_runtime.initialized) return LGX_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&g_runtime.mutex);
    
    assessment->struct_size = sizeof(lgx_performance_assessment_t);
    
    // Get current targets
    lgx_performance_targets_t targets = lgx_runtime_get_performance_targets();
    
    // Get measured values
    uint64_t init_time = g_runtime.init_end_time - g_runtime.init_start_time;
    size_t memory_usage = g_runtime.memory_stats.peak_allocated;
    uint64_t alloc_latency = g_runtime.perf_chars.measured_alloc_time.p50_ns;
    
    // Store measured values
    assessment->measured_init_time_ns = init_time;
    assessment->measured_memory_bytes = memory_usage;
    assessment->measured_alloc_latency_ns = alloc_latency;
    
    // Assess each metric
    assessment->init_time_tier = assess_metric_tier(
        init_time, 
        targets.init_time_tier1_ns, 
        targets.init_time_tier2_ns, 
        targets.init_time_tier3_ns
    );
    
    assessment->memory_usage_tier = assess_metric_tier(
        memory_usage, 
        targets.memory_tier1_bytes, 
        targets.memory_tier2_bytes, 
        targets.memory_tier3_bytes
    );
    
    assessment->alloc_latency_tier = assess_metric_tier(
        alloc_latency, 
        targets.alloc_latency_tier1_ns, 
        targets.alloc_latency_tier2_ns, 
        targets.alloc_latency_tier3_ns
    );
    
    // Overall tier is the minimum (worst) of all metrics
    assessment->overall_tier = assessment->init_time_tier;
    if (assessment->memory_usage_tier < assessment->overall_tier) {
        assessment->overall_tier = assessment->memory_usage_tier;
    }
    if (assessment->alloc_latency_tier < assessment->overall_tier) {
        assessment->overall_tier = assessment->alloc_latency_tier;
    }
    
    // Calculate margins (positive = exceeds target, negative = misses target)
    switch (assessment->init_time_tier) {
        case LGX_PERF_TIER_3:
            assessment->init_time_margin_percent = calculate_margin_percent(init_time, targets.init_time_tier3_ns);
            break;
        case LGX_PERF_TIER_2:
            assessment->init_time_margin_percent = calculate_margin_percent(init_time, targets.init_time_tier2_ns);
            break;
        case LGX_PERF_TIER_1:
        default:
            assessment->init_time_margin_percent = calculate_margin_percent(init_time, targets.init_time_tier1_ns);
            break;
    }
    
    switch (assessment->memory_usage_tier) {
        case LGX_PERF_TIER_3:
            assessment->memory_margin_percent = calculate_margin_percent(memory_usage, targets.memory_tier3_bytes);
            break;
        case LGX_PERF_TIER_2:
            assessment->memory_margin_percent = calculate_margin_percent(memory_usage, targets.memory_tier2_bytes);
            break;
        case LGX_PERF_TIER_1:
        default:
            assessment->memory_margin_percent = calculate_margin_percent(memory_usage, targets.memory_tier1_bytes);
            break;
    }
    
    switch (assessment->alloc_latency_tier) {
        case LGX_PERF_TIER_3:
            assessment->alloc_latency_margin_percent = calculate_margin_percent(alloc_latency, targets.alloc_latency_tier3_ns);
            break;
        case LGX_PERF_TIER_2:
            assessment->alloc_latency_margin_percent = calculate_margin_percent(alloc_latency, targets.alloc_latency_tier2_ns);
            break;
        case LGX_PERF_TIER_1:
        default:
            assessment->alloc_latency_margin_percent = calculate_margin_percent(alloc_latency, targets.alloc_latency_tier1_ns);
            break;
    }
    
    // Identify bottleneck and provide recommendations
    if (assessment->init_time_tier == assessment->overall_tier && 
        assessment->init_time_tier < LGX_PERF_TIER_3) {
        assessment->bottleneck_component = "Initialization Time";
        assessment->improvement_suggestion = "Optimize library loading and parallel initialization";
    } else if (assessment->memory_usage_tier == assessment->overall_tier && 
               assessment->memory_usage_tier < LGX_PERF_TIER_3) {
        assessment->bottleneck_component = "Memory Usage";
        assessment->improvement_suggestion = "Implement memory pool optimization and lazy initialization";
    } else if (assessment->alloc_latency_tier == assessment->overall_tier && 
               assessment->alloc_latency_tier < LGX_PERF_TIER_3) {
        assessment->bottleneck_component = "Allocation Latency";
        assessment->improvement_suggestion = "Implement lock-free allocator and thread-local caching";
    } else {
        assessment->bottleneck_component = "None";
        assessment->improvement_suggestion = "All metrics meet Tier 3 targets";
    }
    
    pthread_mutex_unlock(&g_runtime.mutex);
    return LGX_SUCCESS;
}

const char* lgx_performance_tier_to_string(lgx_performance_tier_t tier) {
    switch (tier) {
        case LGX_PERF_TIER_1: return "Tier 1 (MVP)";
        case LGX_PERF_TIER_2: return "Tier 2 (Competitive)";
        case LGX_PERF_TIER_3: return "Tier 3 (Best-in-class)";
        default: return "Unknown";
    }
}

// Timing services
uint64_t lgx_time_now_ns(void) {
    return get_timestamp_ns();
}

void lgx_time_sleep_ms(uint32_t milliseconds) {
    usleep(milliseconds * 1000);
}

// Error handling
const char* lgx_result_to_string(lgx_result_t result) {
    switch (result) {
        case LGX_SUCCESS: return "Success";
        case LGX_ERROR_INVALID_PARAM: return "Invalid parameter";
        case LGX_ERROR_NOT_INITIALIZED: return "Runtime not initialized";
        case LGX_ERROR_ALREADY_INITIALIZED: return "Runtime already initialized";
        case LGX_ERROR_INCOMPATIBLE_VERSION: return "Incompatible version";
        case LGX_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case LGX_ERROR_IO_ERROR: return "I/O error";
        case LGX_ERROR_NOT_SUPPORTED: return "Not supported";
        case LGX_ERROR_LIBRARY_VERSION_MISMATCH: return "Library version mismatch";
        case LGX_ERROR_GPU_UNAVAILABLE: return "GPU unavailable";
        case LGX_ERROR_RESOURCE_LIMIT_EXCEEDED: return "Resource limit exceeded";
        case LGX_ERROR_HARDWARE_DEGRADED: return "Hardware degraded";
        case LGX_ERROR_INTENT_VALIDATION_FAILED: return "Intent validation failed";
        default: return "Unknown error";
    }
}

// Hardware adaptation API
bool lgx_runtime_has_capability(lgx_capability_t cap) {
    if (!g_runtime.initialized) return false;
    return (g_runtime.available_capabilities & cap) != 0;
}

lgx_result_t lgx_runtime_query_capabilities(uint32_t* capabilities) {
    if (!capabilities || !g_runtime.initialized) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    *capabilities = g_runtime.available_capabilities;
    return LGX_SUCCESS;
}

lgx_hardware_status_t lgx_runtime_get_hardware_status(void) {
    if (!g_runtime.initialized) {
        lgx_hardware_status_t empty_status = {0};
        empty_status.struct_size = sizeof(lgx_hardware_status_t);
        empty_status.achieved_tier = LGX_HW_TIER_DEGRADED;
        empty_status.degradation_reason = "Runtime not initialized";
        return empty_status;
    }
    
    return g_runtime.hardware_status;
}

lgx_result_t lgx_runtime_health_check(lgx_hardware_status_t* status) {
    if (!status || !g_runtime.initialized) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    *status = g_runtime.hardware_status;
    return LGX_SUCCESS;
}