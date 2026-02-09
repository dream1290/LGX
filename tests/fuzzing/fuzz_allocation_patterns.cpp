/**
 * libFuzzer Harness for LGX Runtime Allocation Patterns
 * 
 * This fuzzer tests allocation patterns and memory management by generating
 * random sequences of allocations, frees, and memory operations.
 * 
 * Build with libFuzzer:
 *   clang++ -fsanitize=fuzzer,address -g -O1 \
 *     -I../../include -L../../build -llgx_runtime \
 *     fuzz_allocation_patterns.cpp -o fuzz_allocation_patterns
 * 
 * Run:
 *   ./fuzz_allocation_patterns -max_total_time=300
 */

#include "lgx_runtime.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <algorithm>

// Maximum number of concurrent allocations to track
#define MAX_ALLOCATIONS 1000

// Allocation tracking structure
struct TrackedAllocation {
    void* ptr;
    size_t size;
    uint8_t allocator_type; // 0=heap, 1=frame, 2=gpu
};

// Global state (reused across fuzzing iterations)
static bool runtime_initialized = false;
static lgx_runtime_config_t* global_config = nullptr;
static std::vector<TrackedAllocation> active_allocations;

// Initialize runtime once
static void init_runtime_once() {
    if (!runtime_initialized) {
        global_config = lgx_config_create();
        if (global_config) {
            lgx_result_t result = lgx_runtime_init(global_config);
            if (result == LGX_SUCCESS) {
                runtime_initialized = true;
            }
        }
    }
}

// Cleanup all active allocations
static void cleanup_allocations() {
    for (auto& alloc : active_allocations) {
        if (alloc.ptr) {
            switch (alloc.allocator_type) {
                case 0: // heap
                    lgx_free(alloc.ptr);
                    break;
                case 1: // frame (no explicit free needed)
                    break;
                case 2: // gpu
                    if (lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
                        lgx_gpu_free((lgx_gpu_allocation_t*)alloc.ptr, 0);
                    }
                    break;
            }
        }
    }
    active_allocations.clear();
}

// Fuzz allocation operation
static void fuzz_allocate(const uint8_t* data, size_t& offset, size_t size) {
    if (offset + 9 > size) return;
    if (active_allocations.size() >= MAX_ALLOCATIONS) return;
    
    // Extract allocation parameters
    uint8_t allocator_type = data[offset++] % 3;
    size_t alloc_size = 0;
    memcpy(&alloc_size, &data[offset], sizeof(uint64_t));
    offset += 8;
    
    // Clamp allocation size to reasonable range
    alloc_size = (alloc_size % (1024 * 1024)) + 1; // 1 byte to 1MB
    
    void* ptr = nullptr;
    
    switch (allocator_type) {
        case 0: // Heap allocation
            ptr = lgx_alloc(alloc_size);
            break;
            
        case 1: // Frame allocation
            ptr = lgx_frame_alloc(alloc_size);
            break;
            
        case 2: // GPU allocation
            if (lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
                ptr = lgx_gpu_alloc(alloc_size, 256, 0);
            }
            break;
    }
    
    if (ptr) {
        TrackedAllocation alloc;
        alloc.ptr = ptr;
        alloc.size = alloc_size;
        alloc.allocator_type = allocator_type;
        active_allocations.push_back(alloc);
        
        // Write to allocated memory to test for corruption
        if (allocator_type != 2) { // Skip GPU memory (may not be host-visible)
            memset(ptr, 0xAA, std::min(alloc_size, size_t(4096)));
        }
    }
}

// Fuzz free operation
static void fuzz_free(const uint8_t* data, size_t& offset, size_t size) {
    if (offset + 2 > size) return;
    if (active_allocations.empty()) return;
    
    // Select allocation to free
    uint16_t index = 0;
    memcpy(&index, &data[offset], sizeof(uint16_t));
    offset += 2;
    index %= active_allocations.size();
    
    TrackedAllocation& alloc = active_allocations[index];
    
    if (alloc.ptr) {
        switch (alloc.allocator_type) {
            case 0: // heap
                lgx_free(alloc.ptr);
                break;
            case 1: // frame (no explicit free)
                break;
            case 2: // gpu
                if (lgx_runtime_has_capability(LGX_CAP_GPU_ACCELERATION)) {
                    lgx_gpu_free((lgx_gpu_allocation_t*)alloc.ptr, 0);
                }
                break;
        }
        alloc.ptr = nullptr;
    }
    
    // Remove from tracking
    active_allocations.erase(active_allocations.begin() + index);
}

// Fuzz reallocation pattern
static void fuzz_realloc(const uint8_t* data, size_t& offset, size_t size) {
    if (offset + 10 > size) return;
    if (active_allocations.empty()) return;
    
    // Select allocation to reallocate
    uint16_t index = 0;
    memcpy(&index, &data[offset], sizeof(uint16_t));
    offset += 2;
    index %= active_allocations.size();
    
    // Get new size
    size_t new_size = 0;
    memcpy(&new_size, &data[offset], sizeof(uint64_t));
    offset += 8;
    new_size = (new_size % (1024 * 1024)) + 1;
    
    TrackedAllocation& alloc = active_allocations[index];
    
    if (alloc.ptr && alloc.allocator_type == 0) { // Only heap supports realloc
        // Free old allocation
        lgx_free(alloc.ptr);
        
        // Allocate new one
        void* new_ptr = lgx_alloc(new_size);
        if (new_ptr) {
            alloc.ptr = new_ptr;
            alloc.size = new_size;
            memset(new_ptr, 0xBB, std::min(new_size, size_t(4096)));
        } else {
            alloc.ptr = nullptr;
        }
    }
}

// Fuzz frame reset
static void fuzz_frame_reset(const uint8_t* data, size_t& offset, size_t size) {
    (void)data;
    (void)offset;
    (void)size;
    
    // Reset frame arena
    lgx_frame_reset();
    
    // Remove frame allocations from tracking
    active_allocations.erase(
        std::remove_if(active_allocations.begin(), active_allocations.end(),
            [](const TrackedAllocation& a) { return a.allocator_type == 1; }),
        active_allocations.end()
    );
}

// Fuzz memory operations
static void fuzz_memory_ops(const uint8_t* data, size_t& offset, size_t size) {
    if (offset + 2 > size) return;
    if (active_allocations.empty()) return;
    
    // Select allocation
    uint16_t index = 0;
    memcpy(&index, &data[offset], sizeof(uint16_t));
    offset += 2;
    index %= active_allocations.size();
    
    TrackedAllocation& alloc = active_allocations[index];
    
    if (alloc.ptr && alloc.allocator_type != 2) {
        // Read from memory
        volatile uint8_t dummy = 0;
        size_t read_size = std::min(alloc.size, size_t(256));
        for (size_t i = 0; i < read_size; i++) {
            dummy += ((uint8_t*)alloc.ptr)[i];
        }
        (void)dummy;
    }
}

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Initialize runtime once
    init_runtime_once();
    if (!runtime_initialized) {
        return 0;
    }
    
    // Process fuzzing input as a sequence of operations
    size_t offset = 0;
    
    while (offset < size) {
        if (offset >= size) break;
        
        uint8_t operation = data[offset++];
        
        switch (operation % 5) {
            case 0: // Allocate
                fuzz_allocate(data, offset, size);
                break;
                
            case 1: // Free
                fuzz_free(data, offset, size);
                break;
                
            case 2: // Realloc
                fuzz_realloc(data, offset, size);
                break;
                
            case 3: // Frame reset
                fuzz_frame_reset(data, offset, size);
                break;
                
            case 4: // Memory operations
                fuzz_memory_ops(data, offset, size);
                break;
        }
    }
    
    // Cleanup all allocations
    cleanup_allocations();
    
    return 0;
}
