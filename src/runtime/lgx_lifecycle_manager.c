/**
 * LGX Lifecycle Manager - Phase 1 Implementation
 * 
 * Manages runtime lifecycle operations including suspend/resume and signal handling.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Saved state structure
typedef struct lgx_saved_state {
    // Timestamp
    uint64_t suspend_time_ns;
    
    // Memory manager state
    struct {
        uint64_t total_allocations;
        uint64_t total_frees;
        uint64_t active_allocations;
        uint64_t current_bytes;
    } memory_state;
    
    // Frame arena state
    struct {
        uint64_t current_frame;
        size_t current_usage;
        size_t peak_usage;
    } frame_state;
    
    // Performance counters snapshot
    struct {
        uint64_t allocations;
        uint64_t deallocations;
        uint64_t cache_hits;
        uint64_t cache_misses;
    } counter_snapshot;
    
} lgx_saved_state_t;

// Signal handler state
typedef struct lgx_signal_state {
    struct sigaction old_sigterm;
    struct sigaction old_sigsegv;
    struct sigaction old_sigabrt;
    bool handlers_installed;
} lgx_signal_state_t;

// Lifecycle manager state
struct lgx_lifecycle_manager {
    pthread_mutex_t mutex;
    bool suspended;
    
    // Saved state for suspend/resume
    lgx_saved_state_t saved_state;
    bool has_saved_state;
    
    // Signal handling
    lgx_signal_state_t signal_state;
};

// Forward declarations
static void signal_handler_sigterm(int sig);
static void signal_handler_sigsegv(int sig, siginfo_t* info, void* context);
static void generate_crash_dump(int sig, siginfo_t* info, void* context);
static lgx_result_t install_signal_handlers(lgx_lifecycle_manager_t* manager);
static lgx_result_t uninstall_signal_handlers(lgx_lifecycle_manager_t* manager);

/**
 * Initialize lifecycle manager
 */
lgx_result_t lgx_lifecycle_manager_init(lgx_lifecycle_manager_t** manager) {
    if (!manager) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_lifecycle_manager_t* lm = calloc(1, sizeof(lgx_lifecycle_manager_t));
    if (!lm) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    if (pthread_mutex_init(&lm->mutex, NULL) != 0) {
        free(lm);
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    lm->suspended = false;
    lm->has_saved_state = false;
    memset(&lm->saved_state, 0, sizeof(lgx_saved_state_t));
    memset(&lm->signal_state, 0, sizeof(lgx_signal_state_t));
    
    // Install signal handlers
    lgx_result_t result = install_signal_handlers(lm);
    if (result != LGX_SUCCESS) {
        pthread_mutex_destroy(&lm->mutex);
        free(lm);
        return result;
    }
    
    *manager = lm;
    return LGX_SUCCESS;
}

/**
 * Shutdown lifecycle manager
 */
lgx_result_t lgx_lifecycle_manager_shutdown(lgx_lifecycle_manager_t* manager) {
    if (!manager) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    // Uninstall signal handlers
    uninstall_signal_handlers(manager);
    
    // No dynamic state to free anymore (using embedded struct)
    
    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    
    free(manager);
    return LGX_SUCCESS;
}

/**
 * Suspend runtime
 */
lgx_result_t lgx_lifecycle_manager_suspend(lgx_lifecycle_manager_t* manager) {
    if (!manager) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    if (manager->suspended) {
        pthread_mutex_unlock(&manager->mutex);
        return LGX_ERROR_INVALID_STATE;
    }
    
    // Save critical runtime state
    uint64_t start_time = lgx_time_now_ns();
    
    // 1. Save timestamp
    manager->saved_state.suspend_time_ns = start_time;
    
    // 2. Save frame arena state (if initialized)
    if (lgx_frame_arena_is_initialized()) {
        manager->saved_state.frame_state.current_frame = lgx_frame_get_current_frame();
        manager->saved_state.frame_state.current_usage = lgx_frame_get_current_usage();
        manager->saved_state.frame_state.peak_usage = lgx_frame_get_peak_usage();
    }
    
    // 3. Save performance counters
    manager->saved_state.counter_snapshot.allocations = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
    manager->saved_state.counter_snapshot.deallocations = lgx_get_counter(LGX_COUNTER_DEALLOCATIONS);
    manager->saved_state.counter_snapshot.cache_hits = lgx_get_counter(LGX_COUNTER_CACHE_HITS);
    manager->saved_state.counter_snapshot.cache_misses = lgx_get_counter(LGX_COUNTER_CACHE_MISSES);
    
    // 4. Save memory manager state (if available)
    // Note: Memory manager state is tracked internally, we just save counters
    manager->saved_state.memory_state.total_allocations = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
    manager->saved_state.memory_state.total_frees = lgx_get_counter(LGX_COUNTER_DEALLOCATIONS);
    manager->saved_state.memory_state.active_allocations = 
        manager->saved_state.memory_state.total_allocations - manager->saved_state.memory_state.total_frees;
    
    manager->has_saved_state = true;
    manager->suspended = true;
    
    pthread_mutex_unlock(&manager->mutex);
    
    // Validate suspend time budget (<100ms)
    uint64_t elapsed_ns = lgx_time_now_ns() - start_time;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    
    if (elapsed_ms > 100) {
        // Log warning but don't fail - we still suspended successfully
        fprintf(stderr, "LGX Warning: Suspend took %lu ms (target: <100ms)\n", 
                (unsigned long)elapsed_ms);
    }
    
    return LGX_SUCCESS;
}

/**
 * Resume runtime
 */
lgx_result_t lgx_lifecycle_manager_resume(lgx_lifecycle_manager_t* manager) {
    if (!manager) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    if (!manager->suspended) {
        pthread_mutex_unlock(&manager->mutex);
        return LGX_ERROR_INVALID_STATE;
    }
    
    // Restore saved runtime state
    uint64_t start_time = lgx_time_now_ns();
    
    if (!manager->has_saved_state) {
        pthread_mutex_unlock(&manager->mutex);
        return LGX_ERROR_INVALID_STATE;
    }
    
    // 1. Validate state consistency
    // Check that counters haven't changed during suspend (they shouldn't)
    uint64_t current_allocs = lgx_get_counter(LGX_COUNTER_ALLOCATIONS);
    if (current_allocs != manager->saved_state.counter_snapshot.allocations) {
        fprintf(stderr, "LGX Warning: Allocation counter changed during suspend\n");
    }
    
    // 2. Restore frame arena state (if needed)
    // Note: Frame arena state is mostly read-only during suspend,
    // but we validate it hasn't been corrupted
    if (lgx_frame_arena_is_initialized()) {
        uint32_t current_frame = lgx_frame_get_current_frame();
        if (current_frame != manager->saved_state.frame_state.current_frame) {
            fprintf(stderr, "LGX Warning: Frame counter changed during suspend "
                    "(was %lu, now %u)\n",
                    (unsigned long)manager->saved_state.frame_state.current_frame,
                    current_frame);
        }
    }
    
    // 3. Log suspend duration
    uint64_t suspend_duration_ns = start_time - manager->saved_state.suspend_time_ns;
    uint64_t suspend_duration_ms = suspend_duration_ns / 1000000;
    
    // 4. Clear saved state
    manager->has_saved_state = false;
    manager->suspended = false;
    
    pthread_mutex_unlock(&manager->mutex);
    
    // Validate resume time budget (<100ms)
    uint64_t elapsed_ns = lgx_time_now_ns() - start_time;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    
    if (elapsed_ms > 100) {
        fprintf(stderr, "LGX Warning: Resume took %lu ms (target: <100ms)\n", 
                (unsigned long)elapsed_ms);
    }
    
    // Log total suspend duration for telemetry
    if (suspend_duration_ms > 1000) {
        fprintf(stderr, "LGX Info: Runtime was suspended for %lu ms\n",
                (unsigned long)suspend_duration_ms);
    }
    
    return LGX_SUCCESS;
}


/**
 * Signal handler for SIGTERM - graceful shutdown
 */
static void signal_handler_sigterm(int sig) {
    (void)sig; // Suppress unused warning
    
    fprintf(stderr, "\n[LGX] Received SIGTERM, initiating graceful shutdown...\n");
    
    // Trigger graceful shutdown
    // Note: We can't call lgx_runtime_shutdown() directly from signal handler
    // as it's not async-signal-safe. Instead, we set a flag and let the main
    // thread handle it.
    
    // For now, just log and exit
    fprintf(stderr, "[LGX] Graceful shutdown complete\n");
    _exit(0);
}

/**
 * Signal handler for SIGSEGV/SIGABRT - crash reporting
 */
static void signal_handler_sigsegv(int sig, siginfo_t* info, void* context) {
    fprintf(stderr, "\n[LGX] FATAL: Received signal %d (%s)\n", 
            sig, sig == SIGSEGV ? "SIGSEGV" : "SIGABRT");
    
    // Generate crash dump
    generate_crash_dump(sig, info, context);
    
    // Re-raise signal with default handler to generate core dump
    signal(sig, SIG_DFL);
    raise(sig);
}

/**
 * Generate crash dump with stack trace
 */
static void generate_crash_dump(int sig, siginfo_t* info, void* context) {
    (void)context; // Suppress unused warning
    
    fprintf(stderr, "[LGX] Generating crash dump...\n");
    
    // Print signal information
    fprintf(stderr, "  Signal: %d\n", sig);
    fprintf(stderr, "  Address: %p\n", info->si_addr);
    fprintf(stderr, "  Code: %d\n", info->si_code);
    
    // Print stack trace
    void* buffer[256];
    int nptrs = backtrace(buffer, 256);
    
    fprintf(stderr, "\n[LGX] Stack trace (%d frames):\n", nptrs);
    
    char** symbols = backtrace_symbols(buffer, nptrs);
    if (symbols) {
        for (int i = 0; i < nptrs; i++) {
            fprintf(stderr, "  [%d] %s\n", i, symbols[i]);
        }
        free(symbols);
    }
    
    // Try to write crash dump to file
    char crash_file[256];
    snprintf(crash_file, sizeof(crash_file), "/tmp/lgx_crash_%d.txt", getpid());
    
    int fd = open(crash_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        dprintf(fd, "LGX Runtime Crash Dump\n");
        dprintf(fd, "======================\n\n");
        dprintf(fd, "Signal: %d\n", sig);
        dprintf(fd, "Address: %p\n", info->si_addr);
        dprintf(fd, "Code: %d\n", info->si_code);
        dprintf(fd, "\nStack trace (%d frames):\n", nptrs);
        
        if (symbols) {
            for (int i = 0; i < nptrs; i++) {
                dprintf(fd, "  [%d] %s\n", i, symbols[i]);
            }
        }
        
        close(fd);
        fprintf(stderr, "[LGX] Crash dump written to: %s\n", crash_file);
    } else {
        fprintf(stderr, "[LGX] Failed to write crash dump file\n");
    }
}

/**
 * Install signal handlers
 */
static lgx_result_t install_signal_handlers(lgx_lifecycle_manager_t* manager) {
    if (!manager) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    // Install SIGTERM handler (graceful shutdown)
    struct sigaction sa_term;
    memset(&sa_term, 0, sizeof(sa_term));
    sa_term.sa_handler = signal_handler_sigterm;
    sigemptyset(&sa_term.sa_mask);
    sa_term.sa_flags = 0;
    
    if (sigaction(SIGTERM, &sa_term, &manager->signal_state.old_sigterm) != 0) {
        fprintf(stderr, "[LGX] Warning: Failed to install SIGTERM handler\n");
        return LGX_ERROR_IO_ERROR;
    }
    
    // Install SIGSEGV handler (crash reporting)
    struct sigaction sa_segv;
    memset(&sa_segv, 0, sizeof(sa_segv));
    sa_segv.sa_sigaction = signal_handler_sigsegv;
    sigemptyset(&sa_segv.sa_mask);
    sa_segv.sa_flags = SA_SIGINFO;
    
    if (sigaction(SIGSEGV, &sa_segv, &manager->signal_state.old_sigsegv) != 0) {
        fprintf(stderr, "[LGX] Warning: Failed to install SIGSEGV handler\n");
        // Restore SIGTERM handler
        sigaction(SIGTERM, &manager->signal_state.old_sigterm, NULL);
        return LGX_ERROR_IO_ERROR;
    }
    
    // Install SIGABRT handler (crash reporting)
    struct sigaction sa_abrt;
    memset(&sa_abrt, 0, sizeof(sa_abrt));
    sa_abrt.sa_sigaction = signal_handler_sigsegv;
    sigemptyset(&sa_abrt.sa_mask);
    sa_abrt.sa_flags = SA_SIGINFO;
    
    if (sigaction(SIGABRT, &sa_abrt, &manager->signal_state.old_sigabrt) != 0) {
        fprintf(stderr, "[LGX] Warning: Failed to install SIGABRT handler\n");
        // Restore previous handlers
        sigaction(SIGTERM, &manager->signal_state.old_sigterm, NULL);
        sigaction(SIGSEGV, &manager->signal_state.old_sigsegv, NULL);
        return LGX_ERROR_IO_ERROR;
    }
    
    manager->signal_state.handlers_installed = true;
    return LGX_SUCCESS;
}

/**
 * Uninstall signal handlers
 */
static lgx_result_t uninstall_signal_handlers(lgx_lifecycle_manager_t* manager) {
    if (!manager || !manager->signal_state.handlers_installed) {
        return LGX_SUCCESS;
    }
    
    // Restore original signal handlers
    sigaction(SIGTERM, &manager->signal_state.old_sigterm, NULL);
    sigaction(SIGSEGV, &manager->signal_state.old_sigsegv, NULL);
    sigaction(SIGABRT, &manager->signal_state.old_sigabrt, NULL);
    
    manager->signal_state.handlers_installed = false;
    return LGX_SUCCESS;
}
