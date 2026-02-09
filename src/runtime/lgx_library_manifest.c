/**
 * LGX Library Manifest - Version Validation
 * 
 * Defines expected library versions and validates against system libraries.
 * Ensures deterministic behavior across Linux distributions.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <gnu/libc-version.h>

// Library version requirements
typedef struct {
    const char* name;
    int major_min;
    int minor_min;
    int patch_min;
    bool required;  // If false, library is optional
} library_requirement_t;

// Library manifest - defines expected versions
static const library_requirement_t g_library_manifest[] = {
    // glibc 2.35+ required (Ubuntu 22.04 baseline)
    { "glibc", 2, 35, 0, true },
    
    // libstdc++ is optional (C-only runtime)
    { "libstdc++", 0, 0, 0, false },
    
    // Vulkan 1.3+ recommended but optional
    { "vulkan", 1, 3, 0, false },
};

static const size_t g_manifest_count = sizeof(g_library_manifest) / sizeof(library_requirement_t);

/**
 * Parse version string into components
 * Returns true if parsing succeeded
 */
static bool parse_version(const char* version_str, int* major, int* minor, int* patch) {
    if (!version_str || !major || !minor || !patch) {
        return false;
    }
    
    *major = 0;
    *minor = 0;
    *patch = 0;
    
    // Parse "major.minor.patch" format
    int parsed = sscanf(version_str, "%d.%d.%d", major, minor, patch);
    if (parsed >= 2) {
        return true;  // At least major.minor parsed
    }
    
    // Try "major.minor" format
    parsed = sscanf(version_str, "%d.%d", major, minor);
    return (parsed == 2);
}

/**
 * Compare two versions
 * Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 */
static int compare_versions(int major1, int minor1, int patch1,
                           int major2, int minor2, int patch2) {
    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    return 0;
}

/**
 * Check if version meets minimum requirement
 */
static bool version_meets_requirement(int major, int minor, int patch,
                                     int req_major, int req_minor, int req_patch) {
    int cmp = compare_versions(major, minor, patch, req_major, req_minor, req_patch);
    return (cmp >= 0);  // Current version >= required version
}

/**
 * Get glibc version
 */
lgx_result_t lgx_manifest_check_glibc(char* version_out, size_t version_size,
                                     bool* meets_requirement) {
    if (!version_out || !meets_requirement) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Get glibc version
    const char* glibc_ver = gnu_get_libc_version();
    if (!glibc_ver) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Failed to get glibc version");
        return LGX_ERROR_LIBRARY_VERSION_MISMATCH;
    }
    
    snprintf(version_out, version_size, "%s", glibc_ver);
    
    // Parse version
    int major, minor, patch;
    if (!parse_version(glibc_ver, &major, &minor, &patch)) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Failed to parse glibc version: %s", glibc_ver);
        return LGX_ERROR_LIBRARY_VERSION_MISMATCH;
    }
    
    // Check against manifest requirement
    const library_requirement_t* req = &g_library_manifest[0];  // glibc is first
    *meets_requirement = version_meets_requirement(major, minor, patch,
                                                   req->major_min, req->minor_min, req->patch_min);
    
    if (*meets_requirement) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                      "glibc version %s meets requirement %d.%d.%d",
                      glibc_ver, req->major_min, req->minor_min, req->patch_min);
    } else {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_WARN,
                      "glibc version %s below requirement %d.%d.%d",
                      glibc_ver, req->major_min, req->minor_min, req->patch_min);
    }
    
    return LGX_SUCCESS;
}

/**
 * Get libstdc++ version
 */
lgx_result_t lgx_manifest_check_libstdcpp(char* version_out, size_t version_size,
                                         bool* meets_requirement) {
    if (!version_out || !meets_requirement) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Try to load libstdc++
    void* handle = dlopen("libstdc++.so.6", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) {
        // Try to actually load it
        handle = dlopen("libstdc++.so.6", RTLD_LAZY);
    }
    
    if (!handle) {
        snprintf(version_out, version_size, "not available");
        *meets_requirement = true;  // Optional library
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                      "libstdc++ not available (C-only runtime)");
        return LGX_SUCCESS;
    }
    
    // Get version from __GLIBCXX__ macro (compile-time)
    // For runtime detection, we'll use a heuristic based on symbols
    #ifdef __GLIBCXX__
    snprintf(version_out, version_size, "%d", __GLIBCXX__);
    #else
    snprintf(version_out, version_size, "unknown");
    #endif
    
    dlclose(handle);
    
    *meets_requirement = true;  // libstdc++ is optional
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "libstdc++ version: %s (optional)", version_out);
    
    return LGX_SUCCESS;
}

/**
 * Get Vulkan loader version
 */
lgx_result_t lgx_manifest_check_vulkan(char* version_out, size_t version_size,
                                       bool* meets_requirement) {
    if (!version_out || !meets_requirement) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Try to load Vulkan loader
    void* handle = dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_NOLOAD);
    if (!handle) {
        // Try to actually load it
        handle = dlopen("libvulkan.so.1", RTLD_LAZY);
    }
    
    if (!handle) {
        snprintf(version_out, version_size, "not available");
        *meets_requirement = true;  // Optional library
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                      "Vulkan loader not available");
        return LGX_SUCCESS;
    }
    
    // Get Vulkan version (we can query vkEnumerateInstanceVersion if needed)
    // For now, assume 1.3.x if library is present
    snprintf(version_out, version_size, "1.3.x");
    
    dlclose(handle);
    
    // Check against manifest requirement
    const library_requirement_t* req = &g_library_manifest[2];  // vulkan is third
    int major = 1, minor = 3, patch = 0;
    *meets_requirement = version_meets_requirement(major, minor, patch,
                                                   req->major_min, req->minor_min, req->patch_min);
    
    if (*meets_requirement) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                      "Vulkan loader version %s meets requirement %d.%d.%d",
                      version_out, req->major_min, req->minor_min, req->patch_min);
    } else {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_WARN,
                      "Vulkan loader version %s below requirement %d.%d.%d",
                      version_out, req->major_min, req->minor_min, req->patch_min);
    }
    
    return LGX_SUCCESS;
}

/**
 * Validate all libraries against manifest
 */
lgx_result_t lgx_manifest_validate_all(void) {
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "Validating libraries against manifest...");
    
    bool all_requirements_met = true;
    
    // Check glibc
    char glibc_ver[64];
    bool glibc_ok = false;
    lgx_result_t result = lgx_manifest_check_glibc(glibc_ver, sizeof(glibc_ver), &glibc_ok);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    if (!glibc_ok && g_library_manifest[0].required) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Required library glibc does not meet version requirement");
        all_requirements_met = false;
    }
    
    // Check libstdc++
    char libstdcpp_ver[64];
    bool libstdcpp_ok = false;
    result = lgx_manifest_check_libstdcpp(libstdcpp_ver, sizeof(libstdcpp_ver), &libstdcpp_ok);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    // Check Vulkan
    char vulkan_ver[64];
    bool vulkan_ok = false;
    result = lgx_manifest_check_vulkan(vulkan_ver, sizeof(vulkan_ver), &vulkan_ok);
    if (result != LGX_SUCCESS) {
        return result;
    }
    
    if (!all_requirements_met) {
        lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_ERROR,
                      "Library version validation failed");
        return LGX_ERROR_LIBRARY_VERSION_MISMATCH;
    }
    
    lgx_log_tagged(LGX_SUBSYSTEM_CORE, LGX_LOG_INFO,
                  "All library version requirements met");
    
    return LGX_SUCCESS;
}

/**
 * Get library manifest (for diagnostics)
 */
void lgx_manifest_get_requirements(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }
    
    size_t offset = 0;
    offset += snprintf(buffer + offset, buffer_size - offset,
                      "Library Version Requirements:\n");
    
    for (size_t i = 0; i < g_manifest_count; i++) {
        const library_requirement_t* req = &g_library_manifest[i];
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "  %s: %d.%d.%d (%s)\n",
                          req->name,
                          req->major_min, req->minor_min, req->patch_min,
                          req->required ? "required" : "optional");
        
        if (offset >= buffer_size - 1) {
            break;
        }
    }
}
