#include "../include/lgx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

// Check if NUMA is available at compile time
#ifdef __has_include
  #if __has_include(<numa.h>)
    #include <numa.h>
    #include <numaif.h>
    #define NUMA_AVAILABLE 1
  #else
    #define NUMA_AVAILABLE 0
  #endif
#else
  #define NUMA_AVAILABLE 0
#endif

#define NUM_THREADS 8
#define ALLOCS_PER_THREAD 10000
#define ALLOCATION_SIZE 4096

typedef struct {
    int thread_id;
    int numa_node;
    bool numa_aware;
    double avg_time_us;
    uint64_t total_time_ns;
    pthread_barrier_t* start_barrier;
} thread_data_t;

static void* allocation_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    void* ptrs[ALLOCS_PER_THREAD];
    
#if NUMA_AVAILABLE
    // Bind thread to NUMA node if NUMA-aware
    if (data->numa_aware && data->numa_node >= 0) {
        struct bitmask* mask = numa_allocate_nodemask();
        numa_bitmask_setbit(mask, data->numa_node);
        numa_bind(mask);
        numa_free_nodemask(mask);
    }
#endif
    
    // Wait for all threads to be ready
    pthread_barrier_wait(data->start_barrier);
    
    uint64_t total_time = 0;
    
    // Perform allocations with timing
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        uint64_t start_time = lgx_time_now_ns();
        
#if NUMA_AVAILABLE
        if (data->numa_aware && data->numa_node >= 0) {
            ptrs[i] = numa_alloc_onnode(ALLOCATION_SIZE, data->numa_node);
            if (ptrs[i] == NULL) {
                ptrs[i] = malloc(ALLOCATION_SIZE);
            }
        } else {
            ptrs[i] = malloc(ALLOCATION_SIZE);
        }
#else
        ptrs[i] = malloc(ALLOCATION_SIZE);
#endif
        
        uint64_t end_time = lgx_time_now_ns();
        
        assert(ptrs[i] != NULL);
        total_time += (end_time - start_time);
        
        // Simulate memory access (this is where NUMA locality matters)
        memset(ptrs[i], i & 0xFF, ALLOCATION_SIZE);
        
        // Read back to ensure memory is actually accessed
        volatile char* mem = (volatile char*)ptrs[i];
        volatile char sum = 0;
        for (int j = 0; j < ALLOCATION_SIZE; j += 64) {  // Cache line stride
            sum += mem[j];
        }
    }
    
    data->avg_time_us = (double)total_time / (double)ALLOCS_PER_THREAD / 1000.0;
    data->total_time_ns = total_time;
    
    // Free allocations
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
#if NUMA_AVAILABLE
        if (data->numa_aware && data->numa_node >= 0) {
            numa_free(ptrs[i], ALLOCATION_SIZE);
        } else {
            free(ptrs[i]);
        }
#else
        free(ptrs[i]);
#endif
    }
    
    return NULL;
}

static double run_numa_test(const char* test_name, bool numa_aware) {
    printf("\n%s\n", test_name);
    printf("========================================\n");
    
#if NUMA_AVAILABLE
    int num_nodes = numa_max_node() + 1;
    printf("NUMA nodes available: %d\n", num_nodes);
    
    if (num_nodes < 2) {
        printf("⚠️  Single NUMA node system\n");
    }
#else
    printf("⚠️  NUMA libraries not available at compile time\n");
    int num_nodes = 1;
#endif
    
    // Prepare threads
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    pthread_barrier_t start_barrier;
    
    pthread_barrier_init(&start_barrier, NULL, NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].numa_aware = numa_aware;
        thread_data[i].start_barrier = &start_barrier;
        
        if (numa_aware && num_nodes > 1) {
            thread_data[i].numa_node = i % num_nodes;
        } else {
            thread_data[i].numa_node = -1;
        }
    }
    
    // Run test
    uint64_t test_start_time = lgx_time_now_ns();
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, allocation_thread, &thread_data[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    uint64_t test_end_time = lgx_time_now_ns();
    
    // Calculate statistics
    uint64_t total_time = 0;
    double total_avg = 0.0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_time += thread_data[i].total_time_ns;
        total_avg += thread_data[i].avg_time_us;
    }
    
    double test_time_ms = (test_end_time - test_start_time) / 1000000.0;
    double avg_alloc_time = total_avg / NUM_THREADS;
    
    printf("Results:\n");
    printf("  Total test time: %.2f ms\n", test_time_ms);
    printf("  Average allocation time: %.2f μs\n", avg_alloc_time);
    
    pthread_barrier_destroy(&start_barrier);
    
    return test_time_ms;  // Return total test time for comparison
}

int main() {
    printf("CSF-2: NUMA-Aware Allocation Benefit Measurement\n");
    printf("=================================================\n");
    
#if NUMA_AVAILABLE
    // Check NUMA availability
    if (numa_available() == -1) {
        printf("❌ NUMA not available on this system\n");
        printf("CSF-2 RESULT: SKIP - Cannot validate NUMA benefits\n");
        printf("RECOMMENDATION: Deprioritize NUMA features for Phase 1\n");
        return 0;
    }
    
    printf("✅ NUMA support detected\n");
    printf("NUMA nodes: %d\n", numa_max_node() + 1);
#else
    printf("⚠️  NUMA libraries not available at compile time\n");
    printf("This is common on single-socket development systems\n");
#endif
    
    // Initialize LGX runtime for timing
    lgx_runtime_config_t* config = lgx_config_create();
    lgx_runtime_init(config);
    
    // Test 1: Regular allocation (no NUMA awareness)
    double baseline_time = run_numa_test("TEST 1: Regular allocation (no NUMA awareness)", false);
    printf("Baseline time: %.2f ms\n", baseline_time);
    
    // Test 2: NUMA-aware allocation (may be same as baseline if no NUMA)
    double numa_time = run_numa_test("TEST 2: NUMA-aware allocation", true);
    printf("NUMA-aware time: %.2f ms\n", numa_time);
    
    // CSF-2 Analysis
    printf("\n" "CSF-2 VALIDATION RESULTS:\n");
    printf("=========================================\n");
    
#if NUMA_AVAILABLE
    int num_nodes = numa_max_node() + 1;
    
    if (num_nodes < 2) {
        printf("❌ CSF-2 CANNOT BE VALIDATED: Single NUMA node system\n");
        printf("   Target: >10%% improvement on multi-socket systems\n");
        printf("   Result: Cannot measure NUMA benefits on single-node system\n");
        printf("   Status: INCONCLUSIVE - Need multi-socket hardware for validation\n");
        printf("\n");
        printf("RECOMMENDATION FOR PHASE 1:\n");
        printf("1. Implement basic NUMA detection (numa_available(), numa_max_node())\n");
        printf("2. Add NUMA node binding for threads (numa_bind())\n");
        printf("3. Defer NUMA-aware allocation until multi-socket testing available\n");
        printf("4. Focus on thread-local caching first (CSF-1 validated)\n");
    } else {
        // Calculate improvement
        double improvement = ((baseline_time - numa_time) / baseline_time) * 100.0;
        
        printf("Target: >10%% performance improvement on NUMA systems\n");
        printf("Go/No-Go Threshold: <5%% improvement = deprioritize\n");
        printf("\n");
        printf("Baseline time: %.2f ms\n", baseline_time);
        printf("NUMA-aware time: %.2f ms\n", numa_time);
        printf("Measured improvement: %.1f%%\n", improvement);
        
        if (improvement > 10.0) {
            printf("✅ CSF-2 PASSED: %.1f%% > 10%% target\n", improvement);
            printf("   Status: EXCELLENT - NUMA awareness provides significant benefit\n");
            printf("   Recommendation: Prioritize NUMA features in Phase 1\n");
        } else if (improvement > 5.0) {
            printf("⚠️  CSF-2 MARGINAL: %.1f%% (missed 10%% target but >5%% threshold)\n", improvement);
            printf("   Status: ACCEPTABLE - Moderate NUMA benefit\n");
            printf("   Recommendation: Include basic NUMA support in Phase 1\n");
        } else {
            printf("❌ CSF-2 FAILED: %.1f%% < 5%% threshold\n", improvement);
            printf("   Status: DEPRIORITIZE - NUMA benefits too small\n");
            printf("   Recommendation: Skip NUMA features in Phase 1\n");
        }
    }
#else
    printf("❌ CSF-2 CANNOT BE VALIDATED: NUMA libraries not available\n");
    printf("   This is typical for single-socket development systems\n");
    printf("   Status: INCONCLUSIVE - Need NUMA-capable system for validation\n");
    printf("\n");
    printf("PRACTICAL RECOMMENDATION FOR PHASE 1:\n");
    printf("1. DEPRIORITIZE NUMA features initially\n");
    printf("2. Focus on thread-local caching (CSF-1 validated successfully)\n");
    printf("3. Add basic NUMA detection infrastructure for future use\n");
    printf("4. Plan NUMA validation on multi-socket systems later\n");
    printf("\n");
    printf("NUMA Implementation Priority: LOW\n");
    printf("- Most gaming systems are single-socket\n");
    printf("- Thread-local caching provides bigger benefits\n");
    printf("- NUMA mainly benefits server/workstation workloads\n");
#endif
    
    // Cleanup
    lgx_runtime_shutdown();
    lgx_config_destroy(config);
    
    printf("\n" "CSF-2 DECISION: ");
#if NUMA_AVAILABLE
    printf("CONDITIONAL - Depends on multi-socket testing\n");
#else
    printf("DEPRIORITIZE - Focus on thread-local caching instead\n");
#endif
    
    return 0;
}