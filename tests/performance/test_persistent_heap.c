/**
 * Persistent heap allocator latency test (rehomed from tests/phase0, Task 3.3.3.4)
 *
 * Validates that P99 latency stays under 20 microseconds for all three
 * allocation regimes: small (segregated fit), large (buddy allocator) and
 * mixed. A missed threshold in ANY of the three sections fails the test,
 * not just the last one measured.
 */

#include "lgx_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define NUM_SAMPLES 10000
#define WARMUP_SAMPLES 1000
#define P99_LIMIT_NS 20000ULL /* 20 microseconds */

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int compare_uint64(const void* a, const void* b) {
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

static uint64_t percentile(uint64_t* samples, int count, double p) {
    int index = (int)((double)count * p);
    if (index >= count) index = count - 1;
    return samples[index];
}

typedef struct {
    const char* name;
    size_t (*sampler)(void); /* per-iteration size, or 0-callback for fixed sizes */
} size_policy_t;

static size_t sample_small(void) { return 64 + (rand() % 4000); }      /* 64B - 4KB */
static size_t sample_large(void) { return (8 + (rand() % 1000)) * 1024; } /* 8KB - 1MB */
static size_t sample_mixed(void) { return 16 + (rand() % (1024 * 1024)); } /* 16B - 1MB */

static const size_policy_t SECTION_POLICIES[] = {
    { "small (64B-4KB, segregated fit)", sample_small },
    { "large (8KB-1MB, buddy)", sample_large },
    { "mixed (16B-1MB)", sample_mixed },
};

/**
 * Measure one section and print its percentiles. Returns 0 when the
 * section's P99 satisfies the limit, 1 when it misses, so the caller can
 * aggregate over all sections instead of trusting the last one.
 */
static int run_section(const char* name, size_t (*sampler)(void), uint64_t* latencies) {
    size_t warmup_hit = 0;
    for (int i = 0; i < WARMUP_SAMPLES; i++) {
        void* ptr = lgx_heap_alloc(sampler());
        if (ptr) warmup_hit++;
        lgx_heap_free(ptr);
    }
    if (warmup_hit < WARMUP_SAMPLES) {
        printf("[FAIL] %s: warmup allocations failed (%zu of %d succeeded)\n",
               name, warmup_hit, WARMUP_SAMPLES);
        return 1;
    }

    for (int i = 0; i < NUM_SAMPLES; i++) {
        size_t size = sampler();
        uint64_t start = get_time_ns();
        void* ptr = lgx_heap_alloc(size);
        uint64_t end = get_time_ns();
        if (!ptr) {
            printf("[FAIL] %s: allocation of %zu bytes returned NULL\n", name, size);
            return 1;
        }
        latencies[i] = end - start;
        lgx_heap_free(ptr);
    }

    qsort(latencies, NUM_SAMPLES, sizeof(uint64_t), compare_uint64);
    uint64_t p50 = percentile(latencies, NUM_SAMPLES, 0.50);
    uint64_t p95 = percentile(latencies, NUM_SAMPLES, 0.95);
    uint64_t p99 = percentile(latencies, NUM_SAMPLES, 0.99);
    uint64_t p999 = percentile(latencies, NUM_SAMPLES, 0.999);

    printf("  %s\n", name);
    printf("    P50: %.3f us   P95: %.3f us   P99: %.3f us   P999: %.3f us\n",
           p50 / 1000.0, p95 / 1000.0, p99 / 1000.0, p999 / 1000.0);
    if (p99 < P99_LIMIT_NS) {
        printf("    PASS: P99 %.3f us < %.1f us\n", p99 / 1000.0, P99_LIMIT_NS / 1000.0);
        return 0;
    }
    printf("    FAIL: P99 %.3f us >= %.1f us\n", p99 / 1000.0, P99_LIMIT_NS / 1000.0);
    return 1;
}

int main(void) {
    int failures = 0;

    printf("=== Persistent Heap Latency Test ===\n");
    printf("P99 limit %.1f us in every section\n\n", P99_LIMIT_NS / 1000.0);

    /* Seeded for reproducible sampling: only the drawn sizes vary, not the
       conclusions. */
    srand(20260719u);

    uint64_t* latencies = malloc(sizeof(uint64_t) * NUM_SAMPLES);
    if (!latencies) {
        fprintf(stderr, "Failed to allocate latency array\n");
        return 2;
    }

    lgx_persistent_heap_init();

    for (unsigned i = 0; i < sizeof(SECTION_POLICIES) / sizeof(SECTION_POLICIES[0]); i++) {
        failures += run_section(SECTION_POLICIES[i].name, SECTION_POLICIES[i].sampler, latencies);
    }

    lgx_persistent_heap_shutdown();
    free(latencies);

    printf("\n=== Summary ===\n");
    if (failures == 0) {
        printf("PASS: P99 under limit in all %zu sections\n",
               sizeof(SECTION_POLICIES) / sizeof(SECTION_POLICIES[0]));
        return 0;
    }
    printf("FAIL: %d of %zu section(s) missed the P99 limit\n",
           failures, sizeof(SECTION_POLICIES) / sizeof(SECTION_POLICIES[0]));
    return 1;
}
