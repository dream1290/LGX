/**
 * LGX Memory Usage Monitoring (Task 12.2.4)
 * 
 * Tracks and reports memory usage across all allocators.
 * Provides visibility into memory footprint and helps validate
 * the <200MB memory overhead target (Tier 2).
 * 
 * Features:
 * - RSS (Resident Set Size) tracking
 * - Per-allocator usage tracking
 * - Peak usage tracking
 * - Overhead calculation
 * - Real-time monitoring
 */

#define _GNU_SOURCE
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// Memory usage state
typedef struct {
    pthread_mutex_t mutex;
    
    // RSS tracking
    size_t current_rss_bytes;
    size_t peak_rss_bytes;
    size_t baseline_rss_bytes;  // RSS before init
    
    // Per-allocator usage
    size_t frame_arena_bytes;
    size_t frame_arena_peak_bytes;
    size_t gpu_pool_bytes;
    size_t gpu_pool_peak_bytes;
    size_t persistent_heap_bytes;
    size_t persistent_heap_peak_bytes;
    
    // Metadata overhead
    size_t metadata_bytes;
    
    // Telemetry
    size_t telemetry_bytes;
    
    // Monitoring state
    bool monitoring_enabled;
    uint64_t last_update_time;
} memory_monitor_state_t;

static memory_monitor_state_t g_memory_monitor = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .monitoring_enabled = false,
};

/**
 * Read RSS (Resident Set Size) from /proc/self/status
 * 
 * Returns the current RSS in bytes, or 0 on error.
 */
static size_t read_rss_bytes(void) {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) {
        return 0;
    }
    
    char line[256];
    size_t rss_kb = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            // Parse "VmRSS:    12345 kB"
            sscanf(line + 6, "%zu", &rss_kb);
            break;
        }
    }
    
    fclose(f);
    return rss_kb * 1024;  // Convert KB to bytes
}

/**
 * Initialize memory monitoring
 * 
 * Captures baseline RSS before runtime initialization.
 */
lgx_result_t lgx_memory_monitor_init(void) {
    pthread_mutex_lock(&g_memory_monitor.mutex);
    
    // Capture baseline RSS
    g_memory_monitor.baseline_rss_bytes = read_rss_bytes();
    g_memory_monitor.current_rss_bytes = g_memory_monitor.baseline_rss_bytes;
    g_memory_monitor.peak_rss_bytes = g_memory_monitor.baseline_rss_bytes;
    
    // Reset all counters
    g_memory_monitor.frame_arena_bytes = 0;
    g_memory_monitor.frame_arena_peak_bytes = 0;
    g_memory_monitor.gpu_pool_bytes = 0;
    g_memory_monitor.gpu_pool_peak_bytes = 0;
    g_memory_monitor.persistent_heap_bytes = 0;
    g_memory_monitor.persistent_heap_peak_bytes = 0;
    g_memory_monitor.metadata_bytes = 0;
    g_memory_monitor.telemetry_bytes = 0;
    
    g_memory_monitor.monitoring_enabled = true;
    g_memory_monitor.last_update_time = 0;
    
    pthread_mutex_unlock(&g_memory_monitor.mutex);
    
    return LGX_SUCCESS;
}

/**
 * Shutdown memory monitoring
 */
lgx_result_t lgx_memory_monitor_shutdown(void) {
    pthread_mutex_lock(&g_memory_monitor.mutex);
    
    g_memory_monitor.monitoring_enabled = false;
    
    // Print final statistics
    if (g_memory_monitor.peak_rss_bytes > 0) {
        size_t overhead = g_memory_monitor.peak_rss_bytes - g_memory_monitor.baseline_rss_bytes;
        
        printf("[LGX INFO] Memory Usage Summary:\n");
        printf("  Baseline RSS: %.2f MB\n", 
               g_memory_monitor.baseline_rss_bytes / (1024.0 * 1024.0));
        printf("  Peak RSS: %.2f MB\n", 
               g_memory_monitor.peak_rss_bytes / (1024.0 * 1024.0));
        printf("  Runtime Overhead: %.2f MB\n", 
               overhead / (1024.0 * 1024.0));
        printf("  Frame Arena Peak: %.2f MB\n", 
               g_memory_monitor.frame_arena_peak_bytes / (1024.0 * 1024.0));
        printf("  GPU Pool Peak: %.2f MB\n", 
               g_memory_monitor.gpu_pool_peak_bytes / (1024.0 * 1024.0));
        printf("  Persistent Heap Peak: %.2f MB\n", 
               g_memory_monitor.persistent_heap_peak_bytes / (1024.0 * 1024.0));
        
        // Check against targets
        if (overhead < 200 * 1024 * 1024) {
            printf("  ✅ Tier 2 target achieved (<200MB)\n");
        } else if (overhead < 300 * 1024 * 1024) {
            printf("  ✅ Tier 1 target achieved (<300MB)\n");
        } else {
            printf("  ❌ Memory overhead exceeds targets\n");
        }
    }
    
    pthread_mutex_unlock(&g_memory_monitor.mutex);
    
    return LGX_SUCCESS;
}

/**
 * Update memory usage statistics
 * 
 * Should be called periodically (e.g., once per frame) to update RSS.
 */
void lgx_memory_monitor_update(void) {
    if (!g_memory_monitor.monitoring_enabled) {
        return;
    }
    
    pthread_mutex_lock(&g_memory_monitor.mutex);
    
    // Update RSS
    g_memory_monitor.current_rss_bytes = read_rss_bytes();
    
    // Update peak
    if (g_memory_monitor.current_rss_bytes > g_memory_monitor.peak_rss_bytes) {
        g_memory_monitor.peak_rss_bytes = g_memory_monitor.current_rss_bytes;
    }
    
    pthread_mutex_unlock(&g_memory_monitor.mutex);
}

/**
 * Report frame arena usage
 */
void lgx_memory_monitor_report_frame_arena(size_t bytes) {
    if (!g_memory_monitor.monitoring_enabled) {
        return;
    }
    
    pthread_mutex_lock(&g_memory_monitor.mutex);
    
    g_memory_monitor.frame_arena_bytes = bytes;
    
    if (bytes > g_memory_monitor.frame_arena_peak_bytes) {
        g_memory_monitor.frame_arena_peak_bytes = bytes;
    }
    
    pthread_mutex_unlock(&g_memory_monitor.mutex);
}

/**
 * Report GPU pool usage
 */
void lgx_memory_monitor_report_gpu_pool(size_t bytes) {
    if (!g_memory_monitor.monitoring_enabled) {
        return;
    }
    
    pthread_mutex_lock(&g_memory_monitor.mutex);
    
    g_memory_monitor.gpu_pool_bytes = bytes;
    
    if (bytes > g_memory_monitor.gpu_pool_peak_bytes) {
        g_memory_monitor.gpu_pool_peak_bytes = bytes;
    }
    
    pthread_mutex_unlock(&g_memory_monitor.mutex);
}

/**
 * Report persistent heap usage
 */
void lgx_memory_monitor_report_persistent_heap(size_t bytes) {
    if (!g_memory_monitor.monitoring_enabled) {
        return;
    }
    
    pthread_mutex_lock(&g_memory_monitor.mutex);
    
    g_memory_monitor.persistent_heap_bytes = bytes;
    
    if (bytes > g_memory_monitor.persistent_heap_peak_bytes) {
        g_memory_monitor.persistent_heap_peak_bytes = bytes;
    }
    
    pthread_mutex_unlock(&g_memory_monitor.mutex);
}

/**
 * Get current memory usage
 */
lgx_result_t lgx_get_memory_usage(lgx_memory_usage_t* usage) {
    if (!usage) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!g_memory_monitor.monitoring_enabled) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    pthread_mutex_lock(&g_memory_monitor.mutex);
    
    // Update RSS before reporting
    g_memory_monitor.current_rss_bytes = read_rss_bytes();
    
    // Update peak if current is higher
    if (g_memory_monitor.current_rss_bytes > g_memory_monitor.peak_rss_bytes) {
        g_memory_monitor.peak_rss_bytes = g_memory_monitor.current_rss_bytes;
    }
    
    // Fill usage structure
    usage->rss_bytes = g_memory_monitor.current_rss_bytes;
    usage->baseline_rss_bytes = g_memory_monitor.baseline_rss_bytes;
    usage->overhead_bytes = g_memory_monitor.current_rss_bytes - g_memory_monitor.baseline_rss_bytes;
    usage->peak_rss_bytes = g_memory_monitor.peak_rss_bytes;
    usage->peak_overhead_bytes = g_memory_monitor.peak_rss_bytes - g_memory_monitor.baseline_rss_bytes;
    
    usage->frame_arena_bytes = g_memory_monitor.frame_arena_bytes;
    usage->frame_arena_peak_bytes = g_memory_monitor.frame_arena_peak_bytes;
    usage->gpu_pool_bytes = g_memory_monitor.gpu_pool_bytes;
    usage->gpu_pool_peak_bytes = g_memory_monitor.gpu_pool_peak_bytes;
    usage->persistent_heap_bytes = g_memory_monitor.persistent_heap_bytes;
    usage->persistent_heap_peak_bytes = g_memory_monitor.persistent_heap_peak_bytes;
    
    usage->metadata_bytes = g_memory_monitor.metadata_bytes;
    usage->telemetry_bytes = g_memory_monitor.telemetry_bytes;
    
    pthread_mutex_unlock(&g_memory_monitor.mutex);
    
    return LGX_SUCCESS;
}

/**
 * Print memory usage report
 */
void lgx_memory_monitor_print_report(void) {
    lgx_memory_usage_t usage;
    lgx_result_t result = lgx_get_memory_usage(&usage);
    
    if (result != LGX_SUCCESS) {
        printf("[LGX ERROR] Failed to get memory usage\n");
        return;
    }
    
    printf("\n");
    printf("=== LGX Memory Usage Report ===\n");
    printf("\n");
    printf("RSS (Resident Set Size):\n");
    printf("  Baseline:  %8.2f MB\n", usage.baseline_rss_bytes / (1024.0 * 1024.0));
    printf("  Current:   %8.2f MB\n", usage.rss_bytes / (1024.0 * 1024.0));
    printf("  Peak:      %8.2f MB\n", usage.peak_rss_bytes / (1024.0 * 1024.0));
    printf("\n");
    printf("Runtime Overhead:\n");
    printf("  Current:   %8.2f MB\n", usage.overhead_bytes / (1024.0 * 1024.0));
    printf("  Peak:      %8.2f MB\n", usage.peak_overhead_bytes / (1024.0 * 1024.0));
    printf("\n");
    printf("Per-Allocator Usage:\n");
    printf("  Frame Arena:\n");
    printf("    Current: %8.2f MB\n", usage.frame_arena_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.frame_arena_peak_bytes / (1024.0 * 1024.0));
    printf("  GPU Pool:\n");
    printf("    Current: %8.2f MB\n", usage.gpu_pool_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.gpu_pool_peak_bytes / (1024.0 * 1024.0));
    printf("  Persistent Heap:\n");
    printf("    Current: %8.2f MB\n", usage.persistent_heap_bytes / (1024.0 * 1024.0));
    printf("    Peak:    %8.2f MB\n", usage.persistent_heap_peak_bytes / (1024.0 * 1024.0));
    printf("\n");
    printf("Targets:\n");
    
    if (usage.peak_overhead_bytes < 200 * 1024 * 1024) {
        printf("  ✅ Tier 2: <200MB (%.2f MB)\n", usage.peak_overhead_bytes / (1024.0 * 1024.0));
    } else if (usage.peak_overhead_bytes < 300 * 1024 * 1024) {
        printf("  ✅ Tier 1: <300MB (%.2f MB)\n", usage.peak_overhead_bytes / (1024.0 * 1024.0));
        printf("  ❌ Tier 2: <200MB (%.2f MB over)\n", 
               (usage.peak_overhead_bytes - 200 * 1024 * 1024) / (1024.0 * 1024.0));
    } else {
        printf("  ❌ Tier 1: <300MB (%.2f MB over)\n", 
               (usage.peak_overhead_bytes - 300 * 1024 * 1024) / (1024.0 * 1024.0));
        printf("  ❌ Tier 2: <200MB (%.2f MB over)\n", 
               (usage.peak_overhead_bytes - 200 * 1024 * 1024) / (1024.0 * 1024.0));
    }
    
    printf("\n");
    printf("================================\n");
    printf("\n");
}
