/**
 * LGX Hardware Adapter - Phase 1 Implementation
 * 
 * Detects hardware capabilities and provides hardware tier classification.
 * Handles graceful degradation when hardware features are unavailable.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <errno.h>

// Hardware adapter state
struct lgx_hardware_adapter {
    // Hardware detection results
    bool huge_pages_available;
    bool numa_available;
    long cpu_count;
    
    // GPU information
    bool gpu_available;
    char gpu_vendor[64];
    char driver_version[64];
    
    // Hardware tier classification
    lgx_hardware_tier_t tier;
    uint32_t missing_capabilities;
    char degradation_reason[256];
    char performance_impact[128];
    char remediation_steps[512];
};

// Forward declarations
static void detect_huge_pages(lgx_hardware_adapter_t* adapter);
static void detect_numa(lgx_hardware_adapter_t* adapter);
static void detect_gpu(lgx_hardware_adapter_t* adapter);
static void classify_hardware_tier(lgx_hardware_adapter_t* adapter);

/**
 * Initialize hardware adapter
 */
lgx_result_t lgx_hardware_adapter_init(lgx_hardware_adapter_t** adapter) {
    if (!adapter) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_hardware_adapter_t* hw = calloc(1, sizeof(lgx_hardware_adapter_t));
    if (!hw) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Detect CPU count
    hw->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (hw->cpu_count <= 0) {
        hw->cpu_count = 1; // Fallback
    }
    
    // Detect hardware features
    detect_huge_pages(hw);
    detect_numa(hw);
    detect_gpu(hw);
    
    // Classify hardware tier
    classify_hardware_tier(hw);
    
    *adapter = hw;
    return LGX_SUCCESS;
}

/**
 * Shutdown hardware adapter
 */
lgx_result_t lgx_hardware_adapter_shutdown(lgx_hardware_adapter_t* adapter) {
    if (!adapter) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    free(adapter);
    return LGX_SUCCESS;
}

/**
 * Check if huge pages are available
 */
bool lgx_hardware_adapter_has_huge_pages(lgx_hardware_adapter_t* adapter) {
    if (!adapter) {
        return false;
    }
    return adapter->huge_pages_available;
}

/**
 * Check if NUMA is available
 */
bool lgx_hardware_adapter_has_numa(lgx_hardware_adapter_t* adapter) {
    if (!adapter) {
        return false;
    }
    return adapter->numa_available;
}

/**
 * Get CPU count
 */
long lgx_hardware_adapter_get_cpu_count(lgx_hardware_adapter_t* adapter) {
    if (!adapter) {
        return 1;
    }
    return adapter->cpu_count;
}

/**
 * Get hardware status
 */
lgx_hardware_status_t lgx_hardware_adapter_get_status(lgx_hardware_adapter_t* adapter) {
    lgx_hardware_status_t status;
    status.struct_size = sizeof(lgx_hardware_status_t);
    
    if (!adapter) {
        status.achieved_tier = LGX_HW_TIER_DEGRADED;
        status.missing_capabilities = 0xFFFFFFFF;
        status.degradation_reason = "Hardware adapter not initialized";
        status.performance_impact_estimate = "Unknown";
        status.remediation_steps = "Initialize hardware adapter";
        return status;
    }
    
    status.achieved_tier = adapter->tier;
    status.missing_capabilities = adapter->missing_capabilities;
    status.degradation_reason = adapter->degradation_reason;
    status.performance_impact_estimate = adapter->performance_impact;
    status.remediation_steps = adapter->remediation_steps;
    
    return status;
}

/**
 * Check if GPU is available
 */
bool lgx_hardware_adapter_has_gpu(lgx_hardware_adapter_t* adapter) {
    if (!adapter) {
        return false;
    }
    return adapter->gpu_available;
}

/**
 * Get GPU vendor name
 */
const char* lgx_hardware_adapter_get_gpu_vendor(lgx_hardware_adapter_t* adapter) {
    if (!adapter) {
        return "Unknown";
    }
    return adapter->gpu_vendor;
}

/**
 * Get GPU driver version
 */
const char* lgx_hardware_adapter_get_driver_version(lgx_hardware_adapter_t* adapter) {
    if (!adapter) {
        return "Unknown";
    }
    return adapter->driver_version;
}

// Private implementation functions

static void detect_huge_pages(lgx_hardware_adapter_t* adapter) {
    // Check if huge pages are available by reading /proc/meminfo
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) {
        adapter->huge_pages_available = false;
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), meminfo)) {
        if (strncmp(line, "HugePages_Total:", 16) == 0) {
            int total_pages = 0;
            if (sscanf(line, "HugePages_Total: %d", &total_pages) == 1) {
                adapter->huge_pages_available = (total_pages > 0);
            }
            break;
        }
    }
    
    fclose(meminfo);
}

static void detect_numa(lgx_hardware_adapter_t* adapter) {
    // Check if NUMA is available by checking /sys/devices/system/node
    if (access("/sys/devices/system/node/node1", F_OK) == 0) {
        adapter->numa_available = true;
    } else {
        adapter->numa_available = false;
    }
}

static void detect_gpu(lgx_hardware_adapter_t* adapter) {
    // Simple GPU detection - check for common GPU device files
    adapter->gpu_available = false;
    strcpy(adapter->gpu_vendor, "Unknown");
    strcpy(adapter->driver_version, "Unknown");
    
    // Check for NVIDIA
    if (access("/dev/nvidia0", F_OK) == 0) {
        adapter->gpu_available = true;
        strcpy(adapter->gpu_vendor, "NVIDIA");
        
        // Get NVIDIA driver version from /proc/driver/nvidia/version
        FILE* nvidia_version = fopen("/proc/driver/nvidia/version", "r");
        if (nvidia_version) {
            char line[256];
            if (fgets(line, sizeof(line), nvidia_version)) {
                // Parse line like: "NVRM version: NVIDIA UNIX x86_64 Kernel Module  535.154.05  ..."
                char* version_start = strstr(line, "Kernel Module");
                if (version_start) {
                    version_start += strlen("Kernel Module");
                    // Skip whitespace
                    while (*version_start == ' ' || *version_start == '\t') {
                        version_start++;
                    }
                    // Copy version number
                    int i = 0;
                    while (version_start[i] && version_start[i] != ' ' && i < 63) {
                        adapter->driver_version[i] = version_start[i];
                        i++;
                    }
                    adapter->driver_version[i] = '\0';
                }
            }
            fclose(nvidia_version);
        }
        return;
    }
    
    // Check for AMD/Intel via DRM
    if (access("/dev/dri/card0", F_OK) == 0) {
        adapter->gpu_available = true;
        
        // Try to determine vendor from sysfs
        FILE* vendor_file = fopen("/sys/class/drm/card0/device/vendor", "r");
        if (vendor_file) {
            char vendor_id[16];
            if (fgets(vendor_id, sizeof(vendor_id), vendor_file)) {
                // Vendor IDs: 0x1002 = AMD, 0x8086 = Intel, 0x10de = NVIDIA
                if (strstr(vendor_id, "0x1002")) {
                    strcpy(adapter->gpu_vendor, "AMD");
                } else if (strstr(vendor_id, "0x8086")) {
                    strcpy(adapter->gpu_vendor, "Intel");
                } else if (strstr(vendor_id, "0x10de")) {
                    strcpy(adapter->gpu_vendor, "NVIDIA");
                } else {
                    strcpy(adapter->gpu_vendor, "Unknown");
                }
            }
            fclose(vendor_file);
        } else {
            strcpy(adapter->gpu_vendor, "AMD/Intel");
        }
        
        // Get driver version from kernel module version
        // For AMD: check amdgpu module
        FILE* amdgpu_version = fopen("/sys/module/amdgpu/version", "r");
        if (amdgpu_version) {
            if (fgets(adapter->driver_version, sizeof(adapter->driver_version), amdgpu_version)) {
                // Remove trailing newline
                adapter->driver_version[strcspn(adapter->driver_version, "\n")] = '\0';
            }
            fclose(amdgpu_version);
            return;
        }
        
        // For Intel: check i915 module
        FILE* i915_version = fopen("/sys/module/i915/version", "r");
        if (i915_version) {
            if (fgets(adapter->driver_version, sizeof(adapter->driver_version), i915_version)) {
                adapter->driver_version[strcspn(adapter->driver_version, "\n")] = '\0';
            }
            fclose(i915_version);
            return;
        }
        
        // Fallback: try to get kernel version as driver version
        FILE* kernel_version = fopen("/proc/version", "r");
        if (kernel_version) {
            char line[256];
            if (fgets(line, sizeof(line), kernel_version)) {
                // Parse "Linux version 6.5.0-..."
                char* version_start = strstr(line, "version ");
                if (version_start) {
                    version_start += strlen("version ");
                    int i = 0;
                    while (version_start[i] && version_start[i] != ' ' && i < 63) {
                        adapter->driver_version[i] = version_start[i];
                        i++;
                    }
                    adapter->driver_version[i] = '\0';
                }
            }
            fclose(kernel_version);
        }
        
        return;
    }
}

static void classify_hardware_tier(lgx_hardware_adapter_t* adapter) {
    adapter->missing_capabilities = 0;
    strcpy(adapter->degradation_reason, "");
    strcpy(adapter->performance_impact, "");
    strcpy(adapter->remediation_steps, "");
    
    // Start with optimal tier
    adapter->tier = LGX_HW_TIER_OPTIMAL;
    
    // Check for missing features
    if (!adapter->huge_pages_available) {
        adapter->missing_capabilities |= (1 << LGX_CAP_HUGE_PAGES);
        adapter->tier = LGX_HW_TIER_COMPATIBLE;
        
        if (strlen(adapter->degradation_reason) > 0) {
            strcat(adapter->degradation_reason, ", ");
        }
        strcat(adapter->degradation_reason, "Huge pages unavailable");
        
        if (strlen(adapter->performance_impact) > 0) {
            strcat(adapter->performance_impact, ", ");
        }
        strcat(adapter->performance_impact, "3-5% slower allocation");
        
        if (strlen(adapter->remediation_steps) > 0) {
            strcat(adapter->remediation_steps, "; ");
        }
        strcat(adapter->remediation_steps, "Enable huge pages: echo 1024 > /proc/sys/vm/nr_hugepages");
    }
    
    if (!adapter->numa_available && adapter->cpu_count > 4) {
        adapter->missing_capabilities |= (1 << LGX_CAP_NUMA_AWARENESS);
        // NUMA is only important for multi-socket systems
        
        if (strlen(adapter->degradation_reason) > 0) {
            strcat(adapter->degradation_reason, ", ");
        }
        strcat(adapter->degradation_reason, "Single NUMA node");
        
        if (strlen(adapter->performance_impact) > 0) {
            strcat(adapter->performance_impact, ", ");
        }
        strcat(adapter->performance_impact, "No NUMA optimization");
    }
    
    if (!adapter->gpu_available) {
        adapter->missing_capabilities |= (1 << LGX_CAP_DX11_TRANSLATION);
        adapter->missing_capabilities |= (1 << LGX_CAP_DX12_TRANSLATION);
        adapter->tier = LGX_HW_TIER_DEGRADED;
        
        if (strlen(adapter->degradation_reason) > 0) {
            strcat(adapter->degradation_reason, ", ");
        }
        strcat(adapter->degradation_reason, "No GPU detected");
        
        if (strlen(adapter->performance_impact) > 0) {
            strcat(adapter->performance_impact, ", ");
        }
        strcat(adapter->performance_impact, "Software rendering only");
        
        if (strlen(adapter->remediation_steps) > 0) {
            strcat(adapter->remediation_steps, "; ");
        }
        strcat(adapter->remediation_steps, "Install GPU drivers and ensure GPU is properly connected");
    }
    
    // Set default messages if none were set
    if (strlen(adapter->degradation_reason) == 0) {
        strcpy(adapter->degradation_reason, "All hardware features available");
    }
    
    if (strlen(adapter->performance_impact) == 0) {
        strcpy(adapter->performance_impact, "Optimal performance");
    }
    
    if (strlen(adapter->remediation_steps) == 0) {
        strcpy(adapter->remediation_steps, "No action required");
    }
}