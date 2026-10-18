/**
 * LGX Lifetime Probe — Synthetic Test Workload
 *
 * Simulates a game's allocation patterns across a known set of call sites
 * with deterministic lifetime classes.  Used to validate that the probe
 * captures data correctly and that the analysis script classifies it right.
 *
 * Allocation patterns:
 *   Site A (particles):   alloc + free every frame           → sub-frame
 *   Site B (transforms):  alloc frame N, free frame N+1      → 1-frame
 *   Site C (sound bufs):  alloc frame N, free frame N+2      → 2-frame (safe w/ triple-buffer)
 *   Site D (level geom):  alloc at start, free at end        → session-lifetime
 *   Site E (mixed):       80% sub-frame, 20% 5-frame         → mixed (the hard case)
 *   Site F (tiny temp):   many small allocs, freed same frame → sub-frame, high volume
 *
 * Expected results:
 *   Sites A, B, C, D, F should have ≥99.9% purity.
 *   Site E should show ~80/20 split — this is the case where
 *   per-site prediction would misclassify 20% of volume.
 *
 * Build:
 *   gcc -O2 -o test_workload test_workload.c -ldl
 *
 * Run with probe:
 *   LD_PRELOAD=./lgx_lifetime_probe.so ./test_workload
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

/* ── Configuration ──────────────────────────────────────────────── */

#define NUM_FRAMES          600     /* 10 seconds at 60 FPS */
#define FRAME_TIME_US       16667   /* ~16.67 ms */

#define PARTICLES_PER_FRAME 200     /* Site A: per-frame particles */
#define PARTICLE_SIZE       64      /* 64 bytes each */

#define TRANSFORMS_PER_FRAME 50     /* Site B: 1-frame transforms */
#define TRANSFORM_SIZE       128

#define SOUNDS_PER_FRAME     10     /* Site C: 2-frame sound buffers */
#define SOUND_BUF_SIZE       4096

#define LEVEL_OBJECTS        100    /* Site D: session-lifetime geometry */
#define LEVEL_OBJECT_SIZE    1024

#define MIXED_PER_FRAME      40     /* Site E: mixed lifetimes */
#define MIXED_SIZE           256

#define TINY_PER_FRAME       500    /* Site F: tiny temporaries */
#define TINY_SIZE            16

/* ── Frame marker (resolved at runtime if probe is loaded) ──────── */

typedef void (*frame_marker_fn)(void);
static frame_marker_fn mark_frame = NULL;

static void resolve_frame_marker(void) {
    mark_frame = (frame_marker_fn)dlsym(RTLD_DEFAULT,
                                         "lgx_probe_frame_marker");
    if (mark_frame) {
        fprintf(stderr, "[WORKLOAD] Frame marker found — using probe markers\n");
    } else {
        fprintf(stderr, "[WORKLOAD] No probe loaded — skipping frame markers\n");
    }
}

/* ── Deferred-free ring buffer ──────────────────────────────────── 
 * Holds pointers to be freed N frames in the future.
 */

#define DEFER_RING_SIZE  (16 * 1024)

typedef struct {
    void* ptrs[DEFER_RING_SIZE];
    int   head;
    int   count;
} defer_ring_t;

static void defer_push(defer_ring_t* ring, void* ptr) {
    if (!ptr) return;
    int idx = (ring->head + ring->count) % DEFER_RING_SIZE;
    ring->ptrs[idx] = ptr;
    ring->count++;
}

static void* defer_pop(defer_ring_t* ring) {
    if (ring->count == 0) return NULL;
    void* ptr = ring->ptrs[ring->head];
    ring->head = (ring->head + 1) % DEFER_RING_SIZE;
    ring->count--;
    return ptr;
}

static int defer_len(defer_ring_t* ring) {
    return ring->count;
}

/* ── Per-site allocation functions ──────────────────────────────── 
 *
 * IMPORTANT: Each function has its own call to malloc() so that
 * __builtin_return_address(0) in the probe produces a unique
 * return address per "site."  Do NOT factor these into a single
 * helper — that would merge all sites into one return address.
 */

/* Site A: frame-scoped particles (alloc + free same frame) */
static void __attribute__((noinline))
site_a_particles(int count) {
    for (int i = 0; i < count; i++) {
        void* p = malloc(PARTICLE_SIZE);
        if (p) { memset(p, 0xAA, PARTICLE_SIZE); free(p); }
    }
}

/* Site B: 1-frame transforms */
static void* __attribute__((noinline))
site_b_alloc_transform(void) {
    void* p = malloc(TRANSFORM_SIZE);
    if (p) memset(p, 0xBB, TRANSFORM_SIZE);
    return p;
}

/* Site C: 2-frame sound buffers */
static void* __attribute__((noinline))
site_c_alloc_sound(void) {
    void* p = malloc(SOUND_BUF_SIZE);
    if (p) memset(p, 0xCC, SOUND_BUF_SIZE);
    return p;
}

/* Site D: session-lifetime level geometry */
static void* __attribute__((noinline))
site_d_alloc_level_object(void) {
    void* p = malloc(LEVEL_OBJECT_SIZE);
    if (p) memset(p, 0xDD, LEVEL_OBJECT_SIZE);
    return p;
}

/* Site E: mixed — 80% sub-frame, 20% survives 5 frames */
static void* __attribute__((noinline))
site_e_alloc_mixed(void) {
    void* p = malloc(MIXED_SIZE);
    if (p) memset(p, 0xEE, MIXED_SIZE);
    return p;
}

/* Site F: tiny temporaries (sub-frame) */
static void __attribute__((noinline))
site_f_tiny_temps(int count) {
    for (int i = 0; i < count; i++) {
        void* p = malloc(TINY_SIZE);
        if (p) free(p);
    }
}

/* ── Seeded random ──────────────────────────────────────────────── */

static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(void) {
    resolve_frame_marker();

    fprintf(stderr, "[WORKLOAD] Starting: %d frames, ~%.1f sec simulated\n",
            NUM_FRAMES,
            (double)NUM_FRAMES * FRAME_TIME_US / 1e6);

    /* Deferred-free rings for sites with multi-frame lifetimes */
    defer_ring_t ring_b = {0};  /* 1-frame delay */
    defer_ring_t ring_c = {0};  /* 2-frame delay */
    defer_ring_t ring_c2 = {0}; /* 2nd stage for site C */
    defer_ring_t ring_e = {0};  /* 5-frame delay for site E long-lived */
    defer_ring_t ring_e2 = {0};
    defer_ring_t ring_e3 = {0};
    defer_ring_t ring_e4 = {0};
    defer_ring_t ring_e5 = {0};

    /* Site D: allocate level objects at start */
    void* level_ptrs[LEVEL_OBJECTS];
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        level_ptrs[i] = site_d_alloc_level_object();
    }

    uint32_t rng_state = 42;

    struct timespec sleep_ts = {
        .tv_sec = 0,
        .tv_nsec = FRAME_TIME_US * 1000  /* convert μs → ns */
    };

    for (int frame = 0; frame < NUM_FRAMES; frame++) {

        /* ── Frame boundary marker ── */
        if (mark_frame) mark_frame();

        /* ── Site A: particles (sub-frame) ── */
        site_a_particles(PARTICLES_PER_FRAME);

        /* ── Site B: transforms (1-frame lifetime) ── */
        /* Free last frame's transforms */
        void* old_b;
        while ((old_b = defer_pop(&ring_b)) != NULL) {
            free(old_b);
        }
        /* Allocate this frame's transforms */
        for (int i = 0; i < TRANSFORMS_PER_FRAME; i++) {
            defer_push(&ring_b, site_b_alloc_transform());
        }

        /* ── Site C: sound buffers (2-frame lifetime) ── */
        /* Free buffers from 2 frames ago */
        void* old_c;
        while ((old_c = defer_pop(&ring_c2)) != NULL) {
            free(old_c);
        }
        /* Shift pipeline: c → c2 */
        while (defer_len(&ring_c) > 0) {
            defer_push(&ring_c2, defer_pop(&ring_c));
        }
        /* Allocate this frame's sounds */
        for (int i = 0; i < SOUNDS_PER_FRAME; i++) {
            defer_push(&ring_c, site_c_alloc_sound());
        }

        /* ── Site E: mixed lifetimes ── */
        /* Free 5-frame-old allocations */
        void* old_e;
        while ((old_e = defer_pop(&ring_e5)) != NULL) {
            free(old_e);
        }
        /* Shift pipeline: e4→e5, e3→e4, e2→e3, e→e2 */
        while (defer_len(&ring_e4) > 0) defer_push(&ring_e5, defer_pop(&ring_e4));
        while (defer_len(&ring_e3) > 0) defer_push(&ring_e4, defer_pop(&ring_e3));
        while (defer_len(&ring_e2) > 0) defer_push(&ring_e3, defer_pop(&ring_e2));
        while (defer_len(&ring_e)  > 0) defer_push(&ring_e2, defer_pop(&ring_e));

        for (int i = 0; i < MIXED_PER_FRAME; i++) {
            void* p = site_e_alloc_mixed();
            uint32_t r = xorshift32(&rng_state);
            if ((r % 100) < 80) {
                /* 80%: free immediately (sub-frame) */
                free(p);
            } else {
                /* 20%: defer 5 frames */
                defer_push(&ring_e, p);
            }
        }

        /* ── Site F: tiny temporaries (sub-frame) ── */
        site_f_tiny_temps(TINY_PER_FRAME);

        /* ── Simulate frame time ── */
        nanosleep(&sleep_ts, NULL);

        /* Progress */
        if ((frame + 1) % 100 == 0) {
            fprintf(stderr, "[WORKLOAD] Frame %d/%d\n", frame + 1, NUM_FRAMES);
        }
    }

    /* ── Cleanup ── */

    /* Flush deferred rings */
    void* p;
    while ((p = defer_pop(&ring_b))  != NULL) free(p);
    while ((p = defer_pop(&ring_c))  != NULL) free(p);
    while ((p = defer_pop(&ring_c2)) != NULL) free(p);
    while ((p = defer_pop(&ring_e))  != NULL) free(p);
    while ((p = defer_pop(&ring_e2)) != NULL) free(p);
    while ((p = defer_pop(&ring_e3)) != NULL) free(p);
    while ((p = defer_pop(&ring_e4)) != NULL) free(p);
    while ((p = defer_pop(&ring_e5)) != NULL) free(p);

    /* Free level objects */
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        free(level_ptrs[i]);
    }

    fprintf(stderr, "[WORKLOAD] Done.\n");
    fprintf(stderr, "[WORKLOAD] Expected classification:\n");
    fprintf(stderr, "  Site A (particles):   sub-frame, pure\n");
    fprintf(stderr, "  Site B (transforms):  1-frame,   pure\n");
    fprintf(stderr, "  Site C (sound bufs):  2-frame,   pure\n");
    fprintf(stderr, "  Site D (level geom):  session,   pure\n");
    fprintf(stderr, "  Site E (mixed):       ~80%% sub-frame, ~20%% 5-frame\n");
    fprintf(stderr, "  Site F (tiny temps):  sub-frame, pure\n");

    return 0;
}
