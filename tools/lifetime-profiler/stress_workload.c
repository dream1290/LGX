/**
 * LGX Lifetime Probe — Realistic Game Engine Stress Test
 *
 * Simulates allocation patterns from a real game engine at scale,
 * using multiple threads, varied sizes, and realistic subsystem patterns.
 * More controlled and reproducible than profiling an actual game, while
 * exercising the same malloc/free patterns engines produce.
 *
 * Subsystems (each with its own noinline allocation function):
 *   1. Renderer     - VB/IB temp buffers, draw call state, per-frame
 *   2. Physics      - contact manifolds, broadphase pairs, per-frame
 *   3. Particles    - particle structs, emitter state, per-frame
 *   4. Audio        - mix buffers, voice state, 2-5 frame lifetime
 *   5. Scripting    - GC'd objects, strings, session lifetime
 *   6. Networking   - packet buffers, alloc+free within handler
 *   7. Animation    - bone matrices, blend state, per-frame
 *   8. UI           - widget scratch, layout cache, mixed lifetime
 *   9. Resource     - mesh/texture metadata, level lifetime
 *  10. StringPool   - small string allocs, very high volume, sub-frame
 *
 * This exercises 10 distinct return addresses, 3 threads, ~200K allocs/sec
 *
 * Build:
 *   gcc -O2 -o stress_workload stress_workload.c -lpthread -ldl
 *
 * Run:
 *   LD_PRELOAD=./lgx_lifetime_probe.so ./stress_workload
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ── Configuration ────────────────────────────────────────────── */

#define NUM_FRAMES 1200 /* 20 seconds at 60 FPS */
#define FRAME_TIME_US 16667
#define NUM_THREADS 3 /* main + 2 worker threads */

/* Deferred-free ring (reusable) */
#define RING_CAP 32768
typedef struct {
  void *ptrs[RING_CAP];
  int head, count;
} ring_t;

static inline void ring_push(ring_t *r, void *p) {
  if (!p || r->count >= RING_CAP) {
    free(p);
    return;
  }
  r->ptrs[(r->head + r->count) % RING_CAP] = p;
  r->count++;
}
static inline void *ring_pop(ring_t *r) {
  if (r->count == 0)
    return NULL;
  void *p = r->ptrs[r->head];
  r->head = (r->head + 1) % RING_CAP;
  r->count--;
  return p;
}
static inline void ring_flush(ring_t *r) {
  void *p;
  while ((p = ring_pop(r)))
    free(p);
}

/* XorShift RNG */
static __thread uint32_t rng_state = 0;
static uint32_t xrand(void) {
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

/* Frame marker */
typedef void (*frame_fn)(void);
static frame_fn mark_frame = NULL;

/* ── Subsystem allocation functions ───────────────────────────
 *
 * Each is __attribute__((noinline)) so that __builtin_return_address(0)
 * in the probe produces a unique return address per subsystem.
 * This is the property the gate experiment tests.
 */

/* 1. Renderer: temp draw-call buffers, 48-512 bytes, sub-frame */
static void *__attribute__((noinline)) sub_renderer_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x11, sz);
  return p;
}

/* 2. Physics: contact manifolds, 96 bytes, sub-frame */
static void *__attribute__((noinline)) sub_physics_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x22, sz);
  return p;
}

/* 3. Particles: particle structs, 64 bytes, sub-frame */
static void *__attribute__((noinline)) sub_particle_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x33, sz);
  return p;
}

/* 4. Audio: mix buffers, 2048-8192 bytes, 2-5 frame lifetime */
static void *__attribute__((noinline)) sub_audio_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x44, sz);
  return p;
}

/* 5. Scripting: GC'd objects, 32-256 bytes, session lifetime */
static void *__attribute__((noinline)) sub_script_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x55, sz);
  return p;
}

/* 6. Networking: packet buffers, 256-1500 bytes, sub-frame */
static void *__attribute__((noinline)) sub_network_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x66, sz);
  return p;
}

/* 7. Animation: bone matrices, 768 bytes, 1-frame lifetime */
static void *__attribute__((noinline)) sub_anim_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x77, sz);
  return p;
}

/* 8. UI: scratch buffers, mixed sizes, MIXED lifetime (problem site) */
static void *__attribute__((noinline)) sub_ui_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x88, sz);
  return p;
}

/* 9. Resource: mesh metadata, 2048-16384 bytes, level lifetime */
static void *__attribute__((noinline)) sub_resource_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0x99, sz);
  return p;
}

/* 10. StringPool: tiny strings, 8-64 bytes, sub-frame */
static void *__attribute__((noinline)) sub_string_alloc(size_t sz) {
  void *p = malloc(sz);
  if (p)
    memset(p, 0xAA, sz);
  return p;
}

/* ── Per-thread game loop ─────────────────────────────────────── */

typedef struct {
  int thread_id;
  int start_frame;
  int end_frame;
} thread_args_t;

static void *game_thread(void *arg) {
  thread_args_t *args = (thread_args_t *)arg;
  rng_state = 42 + args->thread_id * 1337;

  /* Per-subsystem deferred-free rings */
  ring_t defer_audio_1 = {0}, defer_audio_2 = {0}, defer_audio_3 = {0},
         defer_audio_4 = {0};
  ring_t defer_anim = {0};
  ring_t defer_ui_long = {0}, defer_ui_l2 = {0}, defer_ui_l3 = {0},
         defer_ui_l4 = {0}, defer_ui_l5 = {0};

/* Resource allocations (level lifetime) */
#define MAX_RESOURCES 200
  void *resources[MAX_RESOURCES];
  int num_resources = 0;

/* Script allocations (session lifetime) */
#define MAX_SCRIPTS 500
  void *scripts[MAX_SCRIPTS];
  int num_scripts = 0;

  /* Pre-allocate resources at "level load" */
  for (int i = 0; i < MAX_RESOURCES; i++) {
    size_t sz = 2048 + (xrand() % 14336); /* 2KB-16KB */
    resources[i] = sub_resource_alloc(sz);
    num_resources++;
  }

  /* Pre-allocate script objects */
  for (int i = 0; i < MAX_SCRIPTS; i++) {
    size_t sz = 32 + (xrand() % 224); /* 32-256 bytes */
    scripts[i] = sub_script_alloc(sz);
    num_scripts++;
  }

  struct timespec sleep_ts = {.tv_sec = 0, .tv_nsec = FRAME_TIME_US * 1000};

  for (int frame = args->start_frame; frame < args->end_frame; frame++) {

    /* Only thread 0 marks frames */
    if (args->thread_id == 0 && mark_frame)
      mark_frame();

    /* ── 1. Renderer: 100 temp buffers, sub-frame ── */
    for (int i = 0; i < 100; i++) {
      size_t sz = 48 + (xrand() % 464); /* 48-512 bytes */
      void *p = sub_renderer_alloc(sz);
      free(p);
    }

    /* ── 2. Physics: 60 contact manifolds, sub-frame ── */
    for (int i = 0; i < 60; i++) {
      void *p = sub_physics_alloc(96);
      free(p);
    }

    /* ── 3. Particles: 150 particles, sub-frame ── */
    for (int i = 0; i < 150; i++) {
      void *p = sub_particle_alloc(64);
      free(p);
    }

    /* ── 4. Audio: 8 mix buffers, 3-frame lifetime ── */
    /* Free 3-frame-old audio */
    ring_flush(&defer_audio_4);
    /* Shift pipeline */
    {
      ring_t tmp = defer_audio_4;
      defer_audio_4 = defer_audio_3;
      defer_audio_3 = defer_audio_2;
      defer_audio_2 = defer_audio_1;
      defer_audio_1 = tmp;
    }
    defer_audio_1.head = 0;
    defer_audio_1.count = 0;
    for (int i = 0; i < 8; i++) {
      size_t sz = 2048 + (xrand() % 6144); /* 2-8 KB */
      ring_push(&defer_audio_1, sub_audio_alloc(sz));
    }

    /* ── 6. Networking: 20 packets, sub-frame ── */
    for (int i = 0; i < 20; i++) {
      size_t sz = 256 + (xrand() % 1244); /* 256-1500 bytes */
      void *p = sub_network_alloc(sz);
      free(p);
    }

    /* ── 7. Animation: 30 bone matrices, 1-frame lifetime ── */
    ring_flush(&defer_anim);
    for (int i = 0; i < 30; i++) {
      ring_push(&defer_anim, sub_anim_alloc(768));
    }

    /* ── 8. UI: 20 scratch buffers, MIXED
     *    70% sub-frame, 30% survives 4 frames
     *    This is the deliberately impure site. */
    ring_flush(&defer_ui_l5);
    {
      ring_t tmp = defer_ui_l5;
      defer_ui_l5 = defer_ui_l4;
      defer_ui_l4 = defer_ui_l3;
      defer_ui_l3 = defer_ui_l2;
      defer_ui_l2 = defer_ui_long;
      defer_ui_long = tmp;
    }
    defer_ui_long.head = 0;
    defer_ui_long.count = 0;
    for (int i = 0; i < 20; i++) {
      size_t sz = 64 + (xrand() % 960); /* 64-1024 bytes */
      void *p = sub_ui_alloc(sz);
      if ((xrand() % 100) < 70) {
        free(p); /* 70%: sub-frame */
      } else {
        ring_push(&defer_ui_long, p); /* 30%: 4-frame */
      }
    }

    /* ── 10. StringPool: 300 tiny strings, sub-frame ── */
    for (int i = 0; i < 300; i++) {
      size_t sz = 8 + (xrand() % 56); /* 8-64 bytes */
      void *p = sub_string_alloc(sz);
      free(p);
    }

    /* Simulate frame time */
    nanosleep(&sleep_ts, NULL);

    if (args->thread_id == 0 && (frame + 1) % 200 == 0) {
      fprintf(stderr, "[STRESS] Frame %d/%d\n", frame + 1, NUM_FRAMES);
    }
  }

  /* Cleanup */
  ring_flush(&defer_audio_1);
  ring_flush(&defer_audio_2);
  ring_flush(&defer_audio_3);
  ring_flush(&defer_audio_4);
  ring_flush(&defer_anim);
  ring_flush(&defer_ui_long);
  ring_flush(&defer_ui_l2);
  ring_flush(&defer_ui_l3);
  ring_flush(&defer_ui_l4);
  ring_flush(&defer_ui_l5);
  for (int i = 0; i < num_resources; i++)
    free(resources[i]);
  for (int i = 0; i < num_scripts; i++)
    free(scripts[i]);

  return NULL;
}

/* ── Main ─────────────────────────────────────────────────────── */

int main(void) {
  mark_frame =
      (frame_fn)(uintptr_t)dlsym(RTLD_DEFAULT, "lgx_probe_frame_marker");
  if (mark_frame)
    fprintf(stderr, "[STRESS] Frame marker found\n");

  fprintf(
      stderr, "[STRESS] Starting: %d frames, %d threads, ~%.0f allocs/frame\n",
      NUM_FRAMES, NUM_THREADS,
      (100.0 + 60 + 150 + 8 + 20 + 30 + 20 + 300)); /* per-thread per-frame */

  /* Split frames across threads.
   * Thread 0 gets all frames (marks frame boundaries).
   * Threads 1-2 run the same loop concurrently (simulate
   * worker threads doing physics/particles/etc). */
  pthread_t threads[NUM_THREADS];
  thread_args_t args[NUM_THREADS];

  for (int t = 0; t < NUM_THREADS; t++) {
    args[t].thread_id = t;
    args[t].start_frame = 0;
    args[t].end_frame = NUM_FRAMES;
  }

  /* Launch worker threads */
  for (int t = 1; t < NUM_THREADS; t++) {
    pthread_create(&threads[t], NULL, game_thread, &args[t]);
  }

  /* Main thread runs too */
  game_thread(&args[0]);

  /* Join workers */
  for (int t = 1; t < NUM_THREADS; t++) {
    pthread_join(threads[t], NULL);
  }

  fprintf(stderr, "[STRESS] Done. Total allocs: ~%d\n",
          NUM_FRAMES * NUM_THREADS *
              (100 + 60 + 150 + 8 + 20 + 30 + 20 + 300 + 200 + 500));

  fprintf(stderr, "[STRESS] Expected per-site purity:\n");
  fprintf(stderr, "  Renderer(100/f):    sub-frame, pure\n");
  fprintf(stderr, "  Physics(60/f):      sub-frame, pure\n");
  fprintf(stderr, "  Particles(150/f):   sub-frame, pure\n");
  fprintf(stderr, "  Audio(8/f):         3-frame,   pure\n");
  fprintf(stderr, "  Network(20/f):      sub-frame, pure\n");
  fprintf(stderr, "  Animation(30/f):    1-frame,   pure\n");
  fprintf(stderr, "  UI(20/f):           MIXED ~70/30 (problem site)\n");
  fprintf(stderr, "  Strings(300/f):     sub-frame, pure\n");
  fprintf(stderr, "  Resources(200):     long-lived, pure\n");
  fprintf(stderr, "  Scripts(500):       long-lived, pure\n");

  return 0;
}
