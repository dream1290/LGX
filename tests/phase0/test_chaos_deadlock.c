/*
 * Chaos testing self-deadlock (Task B4)
 *
 * Pre-fix: lgx_runtime_enable_chaos_testing succeeds, then the first
 * decision call (anything that reads a rate and calls chaos_random)
 * deadlocks on the non-recursive mutex. ctest's 20s timeout turns the
 * hang into a failure.
 *
 * Runtime calls are unconditional; only the result checks use the
 * counter harness, so the hang reproduces in all build types.
 */
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while (0)

int main(void) {
    printf("\n[TEST] chaos_deadlock\n");

    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "config created");
    TEST_ASSERT(lgx_runtime_init(config) == LGX_SUCCESS, "runtime init succeeds");
    lgx_config_destroy(config);

    lgx_chaos_config_t chaos = {0};
    chaos.struct_size = sizeof(lgx_chaos_config_t);
    chaos.inject_memory_pressure = true;
    chaos.failure_rate = 1.0;
    chaos.inject_latency_spikes = true;
    chaos.latency_spike_rate = 1.0;
    chaos.max_latency_spike_ns = 0;
    chaos.random_seed = 42;
    TEST_ASSERT(lgx_runtime_enable_chaos_testing(&chaos) == LGX_SUCCESS,
                "chaos testing enabled");

    /* First decision: deadlocks pre-fix. failure_rate=1.0 makes the
       outcome deterministic. */
    bool fail = lgx_chaos_should_fail_allocation();
    TEST_ASSERT(fail == true, "decision call returns (deadlock fixed)");
    TEST_ASSERT(fail == true, "failure_rate=1.0 => fail_allocation is true");

    /* Second decision exercises the latency path's RNG call too. */
    (void)lgx_chaos_should_inject_latency(&(uint64_t){0});

    TEST_ASSERT(lgx_runtime_disable_chaos_testing() == LGX_SUCCESS,
                "chaos testing disabled");
    lgx_runtime_shutdown();

    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("test_chaos_deadlock: OK\n");
        return 0;
    }
    return 1;
}
