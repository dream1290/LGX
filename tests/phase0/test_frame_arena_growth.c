/*
 * Frame arena chained growth (Task B3)
 *
 * Pre-fix: growing the arena mid-frame munmaps the old mapping, so the
 * first write to a pointer obtained before the growth segfaults (ASan:
 * heap-use-after-free). Post-fix the old chunk stays mapped until reset.
 *
 * Sizing note (plan ruling): FRAME_ARENA_MIN_SIZE is 16 MB (compile-time
 * clamp in lgx_frame_arena_init), so the plan's 256 KB initial size is
 * clamped to 16 MB and growth never fires. Sizes here are the smallest
 * that force two growths: 16 MB initial -> 9 MB + 10 MB -> 24 MB.
 *
 * Runtime checks, not assert(): under NDEBUG the assert forms would
 * discard their side effects (reads, allocations) and the test would
 * verify nothing — the phase0 counter harness keeps them unconditional.
 */
#include "lgx_runtime.h"
#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define INITIAL (16ull * 1024 * 1024)
#define A1_SZ   (9ull * 1024 * 1024)
#define A2_SZ   (10ull * 1024 * 1024)
#define A3_SZ   (24ull * 1024 * 1024)

int main(void) {
    printf("\n[TEST] frame_arena_growth\n");

    lgx_runtime_config_t* config = lgx_config_create();
    TEST_ASSERT(config != NULL, "config created");
    /* Smallest growable initial size (16 MB floor) and a max that allows
       two doublings (16 -> 32 -> 64 MB). */
    config->frame_arena_size = INITIAL;
    config->frame_arena_max_size = 128 * 1024 * 1024;
    TEST_ASSERT(lgx_runtime_init(config) == LGX_SUCCESS, "runtime init succeeds");
    lgx_config_destroy(config);

    TEST_ASSERT(lgx_frame_reset() == LGX_SUCCESS, "reset advances to a fresh frame");

    void* before = lgx_alloc_frame(A1_SZ);
    TEST_ASSERT(before != NULL, "first allocation (9 MB) succeeds");
    if (tests_failed == 0) {
        memset(before, 0x11, A1_SZ);

        /* Forces growth: 9 MB + 10 MB > 16 MB initial capacity. */
        void* after = lgx_alloc_frame(A2_SZ);
        TEST_ASSERT(after != NULL, "second allocation (10 MB) forces growth");
        if (tests_failed == 0) {
            memset(after, 0x22, A2_SZ);

            /* Pre-fix: these accesses hit unmapped memory. Post-fix: intact. */
            TEST_ASSERT(((unsigned char*)before)[0] == 0x11,
                        "pre-growth pointer survives the growth (first byte)");
            TEST_ASSERT(((unsigned char*)before)[A1_SZ - 1] == 0x11,
                        "pre-growth pointer survives the growth (last byte)");
            memset(before, 0x33, 1024);
            TEST_ASSERT(((unsigned char*)before)[0] == 0x33,
                        "pre-growth pointer is writable after the growth");

            /* More growth than one doubling: force a second chunk (32 -> 64 MB). */
            void* third = lgx_alloc_frame(A3_SZ);
            if (third) {
                memset(third, 0x44, A3_SZ);
                TEST_ASSERT(((unsigned char*)third)[0] == 0x44,
                            "second-chunk allocation is usable");
            }
            TEST_ASSERT(third != NULL, "third allocation (24 MB) forces second growth");

            /* Statistics must count everything used this frame. */
            frame_arena_stats_t stats;
            if (lgx_frame_get_stats(&stats) == LGX_SUCCESS) {
                TEST_ASSERT(stats.peak_usage_bytes >= 400 * 1024,
                            "peak usage counts retired bytes + current chunk");
            } else {
                TEST_ASSERT(false, "stats fetch succeeds");
            }
        }
    }

    /* Reset frees retired chunks; the arena must be reusable. */
    TEST_ASSERT(lgx_frame_reset() == LGX_SUCCESS, "reset after growth succeeds");
    void* next = lgx_alloc_frame(64);
    TEST_ASSERT(next != NULL, "arena allocates again after reset");
    if (next) {
        memset(next, 0x55, 64);
    }

    lgx_runtime_shutdown();

    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("test_frame_arena_growth: OK\n");
        return 0;
    }
    return 1;
}
