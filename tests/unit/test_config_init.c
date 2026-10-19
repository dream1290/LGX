#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ %s\n", message); \
        } \
    } while(0)

int main(void) {
    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "Config creation succeeds");

    if (config != NULL) {
        /* calloc: every field that lgx_runtime_init reads unguarded or
           behind a "> 0" check must start at zero. */
        TEST_ASSERT(config->frame_arena_size == 0, "frame_arena_size starts at zero");
        TEST_ASSERT(config->frame_arena_max_size == 0, "frame_arena_max_size starts at zero");
    }

    lgx_config_destroy(config);

    if (tests_failed == 0) {
        printf("test_config_init: OK\n");
    }
    return tests_failed == 0 ? 0 : 1;
}
