/* Tests/unit: tracked-allocation usage stats and untracking.
 *
 * Task B8c: track_allocation copied the intent into the manager's own
 * array and get_usage_stats/untrack_allocation compared the array slot
 * ADDRESS against the user pointer -- which can never match. Pre-fix
 * this test fails: usage stats return the "not found" defaults and
 * freeing the pointer leaks the tracking slot.
 *
 * Counter harness on purpose: the unit tree builds with NDEBUG, where
 * assert() bodies compile out and their side-effecting calls disappear
 * (same -Werror unused-variable trap that moved the other tests onto
 * this harness).
 */
#include "lgx_runtime.h"
#include "lgx/lgx_runtime_internal.h"
#include <stdio.h>

static int checks_passed = 0;
static int checks_failed = 0;

static void check(int condition, const char* what) {
    if (condition) {
        checks_passed++;
        printf("  [ok] %s\n", what);
    } else {
        checks_failed++;
        printf("  [FAIL] %s\n", what);
    }
}

int main(void) {
    lgx_runtime_config_t* config = lgx_config_create();
    check(config != NULL, "runtime config create");

    lgx_hardware_adapter_t* hw = NULL;
    check(lgx_hardware_adapter_init(&hw) == LGX_SUCCESS, "hardware adapter init");

    lgx_memory_manager_t* manager = NULL;
    check(lgx_memory_manager_init(&manager, config, hw) == LGX_SUCCESS,
          "memory manager init");

    lgx_allocation_intent_base_t intent = {
        .struct_size = sizeof(lgx_allocation_intent_base_t),
        .size = 64,
        .access_pattern = LGX_ACCESS_SEQUENTIAL,
        .lifetime = LGX_LIFETIME_SESSION,
        .hint = LGX_HINT_BACKGROUND,
        .validation_policy = LGX_INTENT_TRUST
    };

    void* ptr = lgx_memory_manager_alloc(manager, 64, &intent);
    check(ptr != NULL, "tracked allocation");

    lgx_allocation_usage_t usage = { .struct_size = sizeof(lgx_allocation_usage_t) };
    check(lgx_memory_manager_get_usage_stats(manager, ptr, &usage) == LGX_SUCCESS,
          "get usage stats");

    /* Pre-fix: the search never matches, so these are the "not found"
       defaults (access_count 0, pattern UNKNOWN, confidence 0.0). */
    check(usage.access_count > 0, "usage stats access_count > 0");
    check(usage.observed_pattern == intent.access_pattern,
          "usage stats observed_pattern mirrors intent");
    check(usage.pattern_confidence > 0.0f, "usage stats pattern_confidence > 0");

    lgx_memory_manager_free(manager, ptr);

    check(lgx_memory_manager_shutdown(manager) == LGX_SUCCESS, "manager shutdown");
    lgx_hardware_adapter_shutdown(hw);
    lgx_config_destroy(config);

    printf("test_memory_manager_usage_stats: %d/%d checks passed\n",
           checks_passed, checks_passed + checks_failed);
    return (checks_failed == 0) ? 0 : 1;
}
