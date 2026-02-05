/**
 * LGX Capability Detector - Phase 1 Implementation
 * 
 * Detects and reports available LGX capabilities based on hardware and software.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Capability detector state
struct lgx_capability_detector {
    lgx_hardware_adapter_t* hardware_adapter;
    
    // Capability flags
    uint32_t available_capabilities;
    
    // Capability versions
    uint32_t dx11_translation_version;
    uint32_t dx12_translation_version;
    uint32_t security_module_version;
};

/**
 * Initialize capability detector
 */
lgx_result_t lgx_capability_detector_init(lgx_capability_detector_t** detector,
                                         lgx_hardware_adapter_t* hardware_adapter) {
    if (!detector || !hardware_adapter) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_capability_detector_t* cap = calloc(1, sizeof(lgx_capability_detector_t));
    if (!cap) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    cap->hardware_adapter = hardware_adapter;
    
    // Detect available capabilities based on hardware
    cap->available_capabilities = 0;
    
    // Check for DX translation capabilities (requires GPU)
    lgx_hardware_status_t hw_status = lgx_hardware_adapter_get_status(hardware_adapter);
    if (hw_status.achieved_tier != LGX_HW_TIER_DEGRADED) {
        cap->available_capabilities |= (1 << LGX_CAP_DX11_TRANSLATION);
        cap->available_capabilities |= (1 << LGX_CAP_DX12_TRANSLATION);
        cap->dx11_translation_version = 1;
        cap->dx12_translation_version = 1;
    }
    
    // Security module is always available (software-based)
    cap->available_capabilities |= (1 << LGX_CAP_SECURITY_MODULE);
    cap->security_module_version = 1;
    
    // NUMA awareness depends on hardware
    if (lgx_hardware_adapter_has_numa(hardware_adapter)) {
        cap->available_capabilities |= (1 << LGX_CAP_NUMA_AWARENESS);
    }
    
    // Huge pages depend on hardware
    if (lgx_hardware_adapter_has_huge_pages(hardware_adapter)) {
        cap->available_capabilities |= (1 << LGX_CAP_HUGE_PAGES);
    }
    
    *detector = cap;
    return LGX_SUCCESS;
}

/**
 * Shutdown capability detector
 */
lgx_result_t lgx_capability_detector_shutdown(lgx_capability_detector_t* detector) {
    if (!detector) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    free(detector);
    return LGX_SUCCESS;
}

/**
 * Check if capability is available (by name)
 */
bool lgx_capability_detector_has_capability(lgx_capability_detector_t* detector, 
                                           const char* capability_name) {
    if (!detector || !capability_name) {
        return false;
    }
    
    // Map capability names to enum values
    if (strcmp(capability_name, "dx11_translation") == 0) {
        return (detector->available_capabilities & (1 << LGX_CAP_DX11_TRANSLATION)) != 0;
    } else if (strcmp(capability_name, "dx12_translation") == 0) {
        return (detector->available_capabilities & (1 << LGX_CAP_DX12_TRANSLATION)) != 0;
    } else if (strcmp(capability_name, "security_module") == 0) {
        return (detector->available_capabilities & (1 << LGX_CAP_SECURITY_MODULE)) != 0;
    } else if (strcmp(capability_name, "numa_awareness") == 0) {
        return (detector->available_capabilities & (1 << LGX_CAP_NUMA_AWARENESS)) != 0;
    } else if (strcmp(capability_name, "huge_pages") == 0) {
        return (detector->available_capabilities & (1 << LGX_CAP_HUGE_PAGES)) != 0;
    }
    
    return false;
}

/**
 * Check if capability is available (by enum)
 */
bool lgx_capability_detector_has_capability_enum(lgx_capability_detector_t* detector, 
                                                lgx_capability_t capability) {
    if (!detector) {
        return false;
    }
    
    return (detector->available_capabilities & (1 << capability)) != 0;
}

/**
 * Get capability version
 */
uint32_t lgx_capability_detector_get_capability_version(lgx_capability_detector_t* detector,
                                                       const char* capability_name) {
    if (!detector || !capability_name) {
        return 0;
    }
    
    if (strcmp(capability_name, "dx11_translation") == 0) {
        return detector->dx11_translation_version;
    } else if (strcmp(capability_name, "dx12_translation") == 0) {
        return detector->dx12_translation_version;
    } else if (strcmp(capability_name, "security_module") == 0) {
        return detector->security_module_version;
    }
    
    return 0;
}

/**
 * Query all capabilities
 */
lgx_result_t lgx_capability_detector_query_capabilities(lgx_capability_detector_t* detector,
                                                       uint32_t* capabilities) {
    if (!detector || !capabilities) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    *capabilities = detector->available_capabilities;
    return LGX_SUCCESS;
}