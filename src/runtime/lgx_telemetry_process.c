/**
 * LGX Telemetry Process - Separate Process Implementation
 * 
 * Implements a separate telemetry process that collects data from the game
 * via shared memory IPC. This keeps telemetry overhead out of the game process.
 */

#define _GNU_SOURCE
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>

// Shared memory ring buffer size
#define TELEMETRY_SHM_SIZE (1024 * 1024)  // 1MB
#define TELEMETRY_RING_SIZE 8192

// Telemetry event in shared memory
typedef struct {
    uint64_t timestamp_ns;
    uint32_t type;
    uint32_t size;
    union {
        struct { float frame_time_ms; } frame;
        struct { size_t memory_mb; } memory;
        struct { size_t size; } allocation;
    } data;
} telemetry_shm_event_t;

// Shared memory ring buffer (lock-free)
typedef struct {
    atomic_uint_fast64_t write_index;
    atomic_uint_fast64_t read_index;
    atomic_uint_fast64_t dropped_events;
    atomic_bool shutdown_requested;
    telemetry_shm_event_t events[TELEMETRY_RING_SIZE];
} telemetry_shm_t;

// Telemetry process state
struct lgx_telemetry_process {
    pid_t process_pid;
    int shm_fd;
    telemetry_shm_t* shm;
    bool running;
    
    // Aggregated statistics
    uint64_t frame_count;
    double total_frame_time;
    double max_frame_time;
    size_t allocation_count;
};

/**
 * Telemetry process main loop
 */
static int telemetry_process_main(telemetry_shm_t* shm) {
    uint64_t frame_count = 0;
    double total_frame_time = 0.0;
    double max_frame_time = 0.0;
    size_t allocation_count = 0;
    
    fprintf(stderr, "[Telemetry Process] Started (PID: %d)\n", getpid());
    
    while (!atomic_load(&shm->shutdown_requested)) {
        uint64_t read_idx = atomic_load(&shm->read_index);
        uint64_t write_idx = atomic_load(&shm->write_index);
        
        // Check if there are events to process
        if (read_idx == write_idx) {
            // No events, sleep briefly
            usleep(1000);  // 1ms
            continue;
        }
        
        // Process event
        size_t idx = read_idx % TELEMETRY_RING_SIZE;
        telemetry_shm_event_t* event = &shm->events[idx];
        
        // Aggregate based on event type
        switch (event->type) {
            case 0:  // Frame time
                frame_count++;
                total_frame_time += event->data.frame.frame_time_ms;
                if (event->data.frame.frame_time_ms > max_frame_time) {
                    max_frame_time = event->data.frame.frame_time_ms;
                }
                break;
                
            case 1:  // Memory usage
                // Track memory
                break;
                
            case 2:  // Allocation
                allocation_count++;
                break;
        }
        
        // Advance read index
        atomic_store(&shm->read_index, read_idx + 1);
    }
    
    // Log final statistics
    if (frame_count > 0) {
        double avg_frame_time = total_frame_time / frame_count;
        fprintf(stderr, "[Telemetry Process] Final stats: %lu frames, avg %.2f ms, max %.2f ms\n",
                frame_count, avg_frame_time, max_frame_time);
    }
    fprintf(stderr, "[Telemetry Process] Allocations: %zu\n", allocation_count);
    fprintf(stderr, "[Telemetry Process] Dropped events: %lu\n",
            atomic_load(&shm->dropped_events));
    fprintf(stderr, "[Telemetry Process] Shutdown\n");
    
    return 0;
}

/**
 * Initialize telemetry process
 */
lgx_result_t lgx_telemetry_process_init(lgx_telemetry_process_t** process) {
    if (!process) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    lgx_telemetry_process_t* proc = calloc(1, sizeof(lgx_telemetry_process_t));
    if (!proc) {
        return LGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Create shared memory
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/lgx_telemetry_%d", getpid());
    
    proc->shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (proc->shm_fd < 0) {
        free(proc);
        return LGX_ERROR_SYSTEM_ERROR;
    }
    
    // Set size
    if (ftruncate(proc->shm_fd, sizeof(telemetry_shm_t)) < 0) {
        close(proc->shm_fd);
        shm_unlink(shm_name);
        free(proc);
        return LGX_ERROR_SYSTEM_ERROR;
    }
    
    // Map shared memory
    proc->shm = mmap(NULL, sizeof(telemetry_shm_t),
                     PROT_READ | PROT_WRITE, MAP_SHARED,
                     proc->shm_fd, 0);
    if (proc->shm == MAP_FAILED) {
        close(proc->shm_fd);
        shm_unlink(shm_name);
        free(proc);
        return LGX_ERROR_SYSTEM_ERROR;
    }
    
    // Initialize shared memory
    memset(proc->shm, 0, sizeof(telemetry_shm_t));
    atomic_store(&proc->shm->write_index, 0);
    atomic_store(&proc->shm->read_index, 0);
    atomic_store(&proc->shm->dropped_events, 0);
    atomic_store(&proc->shm->shutdown_requested, false);
    
    // Fork telemetry process
    pid_t pid = fork();
    if (pid < 0) {
        munmap(proc->shm, sizeof(telemetry_shm_t));
        close(proc->shm_fd);
        shm_unlink(shm_name);
        free(proc);
        return LGX_ERROR_SYSTEM_ERROR;
    }
    
    if (pid == 0) {
        // Child process: run telemetry loop
        int exit_code = telemetry_process_main(proc->shm);
        exit(exit_code);
    }
    
    // Parent process: store PID and continue
    proc->process_pid = pid;
    proc->running = true;
    
    lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                  "Telemetry process started (PID: %d)", pid);
    
    *process = proc;
    return LGX_SUCCESS;
}

/**
 * Shutdown telemetry process
 */
lgx_result_t lgx_telemetry_process_shutdown(lgx_telemetry_process_t* process) {
    if (!process) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (process->running) {
        // Signal shutdown
        atomic_store(&process->shm->shutdown_requested, true);
        
        // Wait for process to exit (with timeout)
        int status;
        int wait_count = 0;
        while (wait_count < 100) {  // 1 second timeout
            pid_t result = waitpid(process->process_pid, &status, WNOHANG);
            if (result == process->process_pid) {
                break;
            }
            usleep(10000);  // 10ms
            wait_count++;
        }
        
        // Force kill if still running
        if (wait_count >= 100) {
            kill(process->process_pid, SIGKILL);
            waitpid(process->process_pid, &status, 0);
        }
        
        process->running = false;
    }
    
    // Cleanup shared memory
    if (process->shm) {
        munmap(process->shm, sizeof(telemetry_shm_t));
    }
    
    if (process->shm_fd >= 0) {
        close(process->shm_fd);
        
        char shm_name[64];
        snprintf(shm_name, sizeof(shm_name), "/lgx_telemetry_%d", getpid());
        shm_unlink(shm_name);
    }
    
    free(process);
    
    lgx_log_tagged(LGX_SUBSYSTEM_TELEMETRY, LGX_LOG_INFO,
                  "Telemetry process shutdown complete");
    
    return LGX_SUCCESS;
}

/**
 * Write event to shared memory (lock-free)
 */
lgx_result_t lgx_telemetry_process_write_event(lgx_telemetry_process_t* process,
                                               const telemetry_shm_event_t* event) {
    if (!process || !event) {
        return LGX_ERROR_INVALID_PARAM;
    }
    
    if (!process->running) {
        return LGX_ERROR_NOT_INITIALIZED;
    }
    
    // Get current write index
    uint64_t write_idx = atomic_load(&process->shm->write_index);
    uint64_t read_idx = atomic_load(&process->shm->read_index);
    
    // Check if buffer is full
    if (write_idx - read_idx >= TELEMETRY_RING_SIZE) {
        atomic_fetch_add(&process->shm->dropped_events, 1);
        return LGX_SUCCESS;  // Not an error, just dropped
    }
    
    // Write event
    size_t idx = write_idx % TELEMETRY_RING_SIZE;
    process->shm->events[idx] = *event;
    
    // Advance write index (atomic)
    atomic_store(&process->shm->write_index, write_idx + 1);
    
    return LGX_SUCCESS;
}

/**
 * Record frame time (public API)
 */
lgx_result_t lgx_telemetry_process_record_frame_time(lgx_telemetry_process_t* process,
                                                     float frame_time_ms) {
    telemetry_shm_event_t event = {
        .timestamp_ns = lgx_time_now_ns(),
        .type = 0,  // Frame time
        .size = sizeof(float),
        .data.frame = { .frame_time_ms = frame_time_ms }
    };
    
    return lgx_telemetry_process_write_event(process, &event);
}

/**
 * Record memory usage (public API)
 */
lgx_result_t lgx_telemetry_process_record_memory(lgx_telemetry_process_t* process,
                                                 size_t memory_mb) {
    telemetry_shm_event_t event = {
        .timestamp_ns = lgx_time_now_ns(),
        .type = 1,  // Memory
        .size = sizeof(size_t),
        .data.memory = { .memory_mb = memory_mb }
    };
    
    return lgx_telemetry_process_write_event(process, &event);
}

/**
 * Record allocation (public API)
 */
lgx_result_t lgx_telemetry_process_record_allocation(lgx_telemetry_process_t* process,
                                                     size_t size) {
    telemetry_shm_event_t event = {
        .timestamp_ns = lgx_time_now_ns(),
        .type = 2,  // Allocation
        .size = sizeof(size_t),
        .data.allocation = { .size = size }
    };
    
    return lgx_telemetry_process_write_event(process, &event);
}

/**
 * Get dropped event count
 */
uint64_t lgx_telemetry_process_get_dropped_events(lgx_telemetry_process_t* process) {
    if (!process || !process->shm) {
        return 0;
    }
    
    return atomic_load(&process->shm->dropped_events);
}
