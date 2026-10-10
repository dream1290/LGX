#include <lgx_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

/* LGX_HAVE_NUMA comes from tests/phase0/CMakeLists.txt and is 1 only when
 * libnuma was actually found and linked, 0 otherwise. Checking for the header
 * with __has_include is not sufficient: the header can be installed while the
 * library is not linked, which fails at link time on every numa_* call. */
#if defined(__linux__) && LGX_HAVE_NUMA
    #include <numa.h>
    #include <numaif.h>
    #define LGX_NUMA_USABLE 1
#else
    #define LGX_NUMA_USABLE 0
#endif

/* Explicit failure helpers. These deliberately avoid assert(): Release builds
 * define NDEBUG, which compiles assert() out entirely and would leave this
 * test with no way to fail. These macros return non-zero regardless. */
#define NUMA_FAIL(msg) do { \
    printf("❌ FAIL: %s\n", (msg)); \
    return 1; \
} while (0)

#define NUMA_REQUIRE(cond, msg) do { \
    if (!(cond)) { NUMA_FAIL(msg); } \
} while (0)

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
    int allocated;        /* number of slots in ptrs[] that hold a live pointer */
    int alloc_failures;   /* allocations that returned NULL */
} thread_data_t;

static void* allocation_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    void* ptrs[ALLOCS_PER_THREAD];
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) { ptrs[i] = NULL; }
    data->allocated = 0;
    data->alloc_failures = 0;

#if LGX_NUMA_USABLE
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
        
#if LGX_NUMA_USABLE
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

        /* Explicit NULL check rather than assert(): under -DNDEBUG an assert
         * vanishes, and a NULL slot would be dereferenced by the memset below. */
        if (ptrs[i] == NULL) {
            data->alloc_failures++;
            break;  /* ptrs[i..] stay NULL and are not freed below */
        }
        data->allocated = i + 1;
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
    
    data->avg_time_us = (data->allocated > 0)
                        ? (double)total_time / (double)data->allocated / 1000.0
                        : 0.0;
    data->total_time_ns = total_time;
    
    // Free allocations (only the slots that actually hold a pointer)
    for (int i = 0; i < data->allocated; i++) {
#if LGX_NUMA_USABLE
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

static int run_numa_test(const char* test_name, bool numa_aware,
                         double* out_time_ms, int* out_alloc_failures) {
    *out_time_ms = 0.0;
    *out_alloc_failures = 0;
    printf("\n%s\n", test_name);
    printf("========================================\n");
    
#if LGX_NUMA_USABLE
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
    
    /* Aggregate allocation failures across threads so the caller can fail. */
    int total_failures = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_failures += thread_data[i].alloc_failures;
    }
    *out_alloc_failures = total_failures;
    *out_time_ms = test_time_ms;
    return 0;  // 0 = ran to completion; failures reported via out_alloc_failures
}

int main() {
    printf("CSF-2: NUMA-Aware Allocation Benefit Measurement\n");
    printf("=================================================\n");

#if !LGX_NUMA_USABLE
    // Built without libnuma: report SKIP and pass, so CI stays portable.
    // Returning here rather than wrapping the rest of main() in #else keeps
    // run_numa_test()/allocation_thread() referenced, which -Wunused-function
    // (under -Werror) would otherwise reject in this configuration.
    printf("[SKIP] test_csf2_numa_awareness: libnuma not available at build time\n");
    printf("[INFO] NUMA-awareness logic is covered by test_hardware_adaptation\n");
    return 0;
#endif

#if LGX_NUMA_USABLE
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
    NUMA_REQUIRE(config != NULL, "lgx_config_create() returned NULL");

    /* From here on the runtime is live, so failure paths must tear it down
     * before exiting; otherwise a leak sanitizer would report the abandoned
     * runtime instead of the assertion that actually failed. */
#define NUMA_REQUIRE_LIVE(cond, msg) do { \
    if (!(cond)) { \
        printf("❌ FAIL: %s\n", (msg)); \
        lgx_runtime_shutdown(); \
        lgx_config_destroy(config); \
        return 1; \
    } \
} while (0)

    NUMA_REQUIRE_LIVE(lgx_runtime_init(config) == LGX_SUCCESS,
                      "lgx_runtime_init() failed");
    
    // Test 1: Regular allocation (no NUMA awareness)
    double baseline_time = 0.0;
    double numa_time = 0.0;
    int alloc_failures = 0;
    int rc = run_numa_test("TEST 1: Regular allocation (no NUMA awareness)",
                           false, &baseline_time, &alloc_failures);
    NUMA_REQUIRE_LIVE(rc == 0, "TEST 1 (baseline) did not complete");
    NUMA_REQUIRE_LIVE(alloc_failures == 0,
                      "TEST 1 (baseline) had allocation failures");
    printf("Baseline time: %.2f ms\n", baseline_time);
    
    // Test 2: NUMA-aware allocation (may be same as baseline if no NUMA)
    rc = run_numa_test("TEST 2: NUMA-aware allocation", true,
                       &numa_time, &alloc_failures);
    NUMA_REQUIRE_LIVE(rc == 0, "TEST 2 (NUMA-aware) did not complete");
    NUMA_REQUIRE_LIVE(alloc_failures == 0,
                      "TEST 2 (NUMA-aware) had allocation failures");
    printf("NUMA-aware time: %.2f ms\n", numa_time);
    NUMA_REQUIRE_LIVE(baseline_time > 0.0, "Baseline time was not measurable");
    
    // CSF-2 Analysis
    printf("\n" "CSF-2 VALIDATION RESULTS:\n");
    printf("=========================================\n");
    
#if LGX_NUMA_USABLE
    int num_nodes = numa_max_node() + 1;
    
    if (num_nodes < 2) {
        /* INCONCLUSIVE, not a failure: the hardware cannot answer the question,
         * so this is a skip and exits 0. Only the <2-node verdict below fails. */
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
            /* Below-threshold benefit is a real assertion failure, not a run
             * that merely prints ❌ and exits 0. Fail for real. */
            printf("❌ FAIL: CSF-2 validation: NUMA improvement below 5%% threshold\n");
            lgx_runtime_shutdown();
            lgx_config_destroy(config);
            return 1;
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
#if LGX_NUMA_USABLE
    printf("CONDITIONAL - Depends on multi-socket testing\n");
#else
    printf("DEPRIORITIZE - Focus on thread-local caching instead\n");
#endif
    
    return 0;
}