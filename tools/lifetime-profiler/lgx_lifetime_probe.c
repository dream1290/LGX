/**
 * LGX Lifetime Probe — LD_PRELOAD Allocation Profiler
 *
 * Hooks malloc/free/calloc/realloc via LD_PRELOAD.  For each event, logs:
 *   { type, timestamp_ns, return_address, ptr, size, tid }
 * into a lock-free mmap'd buffer.  On process exit (or SIGUSR1), writes
 * the buffer to a binary file for offline analysis.
 *
 * Design constraints:
 *   - Cannot call malloc/free internally (infinite recursion).
 *     All internal storage is mmap(MAP_ANONYMOUS).
 *   - Uses __builtin_return_address(0) only (~2 ns, within noise).
 *   - Lock-free append via atomic_fetch_add on a shared counter.
 *   - Thread-safe, no mutexes in the hot path.
 *
 * Environment variables:
 *   LGX_PROBE_LOG_ENTRIES  Max log entries (default 8M ≈ 384 MB)
 *   LGX_PROBE_OUTPUT       Output file path (default /tmp/lgx_probe_<pid>.bin)
 *   LGX_PROBE_QUIET        If set, suppress stderr status messages
 *
 * Build:
 *   gcc -shared -fPIC -O2 -o lgx_lifetime_probe.so lgx_lifetime_probe.c \
 *       -ldl -lpthread
 *
 * Usage:
 *   LD_PRELOAD=./lgx_lifetime_probe.so ./my_game
 *   python3 analyze_lifetimes.py /tmp/lgx_probe_<pid>.bin
 *
 * Frame markers:
 *   If the target links against this shim (or dlopen's it), it can call
 *   lgx_probe_frame_marker() at frame boundaries.  Otherwise the analysis
 *   script infers frames from wall-clock time.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

/* ── Log entry format ────────────────────────────────────────────── */

#define PROBE_MAGIC     "LGXPROBE"
#define PROBE_VERSION   1

enum probe_entry_type {
    ENTRY_ALLOC        = 0,
    ENTRY_FREE         = 1,
    ENTRY_CALLOC       = 2,
    ENTRY_REALLOC_OLD  = 3,   /* free side of realloc */
    ENTRY_REALLOC_NEW  = 4,   /* alloc side of realloc */
    ENTRY_FRAME_MARKER = 5,
};

/* 48 bytes, cache-friendly */
typedef struct __attribute__((packed)) {
    uint64_t timestamp_ns;
    uint64_t return_addr;
    uint64_t ptr;
    uint64_t size;
    uint32_t type;
    uint32_t tid;
} probe_entry_t;

_Static_assert(sizeof(probe_entry_t) == 40, "entry must be 40 bytes");

typedef struct {
    char     magic[8];
    uint32_t version;
    uint32_t entry_size;
    uint64_t entry_count;
    uint64_t start_time_ns;
} probe_header_t;

/* ── Globals ─────────────────────────────────────────────────────── */

/* Real libc functions, resolved via dlsym(RTLD_NEXT) */
static void* (*real_malloc)(size_t)                    = NULL;
static void  (*real_free)(void*)                       = NULL;
static void* (*real_calloc)(size_t, size_t)            = NULL;
static void* (*real_realloc)(void*, size_t)            = NULL;

/* Log buffer (mmap'd) */
static probe_entry_t* g_log_buffer  = NULL;
static uint64_t       g_log_capacity = 0;
static atomic_uint_fast64_t g_log_count = 0;

/* State */
static uint64_t  g_start_time_ns  = 0;
static bool      g_initialized    = false;
static bool      g_quiet          = false;
static atomic_uint_fast64_t g_frame_counter = 0;

/* Re-entrancy guard (per-thread) */
static __thread int tls_in_hook = 0;

/* ── Bootstrap buffer ────────────────────────────────────────────
 * dlsym() itself may call malloc/calloc.  Before we've resolved the
 * real functions, we satisfy those tiny allocations from a static
 * buffer.  Once init completes, these pointers are effectively leaked
 * (a few KB, harmless for a profiling session).
 */
#define BOOTSTRAP_SIZE  (128 * 1024)
static char   bootstrap_buf[BOOTSTRAP_SIZE]
              __attribute__((aligned(16)));
static size_t bootstrap_offset = 0;
static int    bootstrap_active = 0;   /* non-zero during dlsym init */

static void* bootstrap_alloc(size_t size) {
    size = (size + 15) & ~(size_t)15;   /* 16-byte align */
    if (bootstrap_offset + size > BOOTSTRAP_SIZE) return NULL;
    void* p = bootstrap_buf + bootstrap_offset;
    bootstrap_offset += size;
    return p;
}

static bool is_bootstrap_ptr(void* p) {
    return (char*)p >= bootstrap_buf &&
           (char*)p <  bootstrap_buf + BOOTSTRAP_SIZE;
}

/* ── Timing ──────────────────────────────────────────────────────── */

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline uint32_t get_tid(void) {
    return (uint32_t)syscall(SYS_gettid);
}

/* ── Log append (lock-free) ──────────────────────────────────────── */

static inline void log_event(uint32_t type, uint64_t ret_addr,
                             uint64_t ptr, uint64_t size) {
    if (!g_log_buffer) return;

    uint64_t idx = atomic_fetch_add_explicit(
        &g_log_count, 1, memory_order_relaxed);

    if (idx >= g_log_capacity) {
        /* Buffer full — silently stop recording.
         * Decrement is best-effort; doesn't need to be exact. */
        return;
    }

    probe_entry_t* e = &g_log_buffer[idx];
    e->timestamp_ns = now_ns();
    e->return_addr  = ret_addr;
    e->ptr          = ptr;
    e->size         = size;
    e->type         = type;
    e->tid          = get_tid();
}

/* ── Dump log to file ────────────────────────────────────────────── */

static void dump_log(void) {
    if (!g_log_buffer) return;

    uint64_t count = atomic_load(&g_log_count);
    if (count > g_log_capacity) count = g_log_capacity;
    if (count == 0) return;

    /* Determine output path */
    const char* path = getenv("LGX_PROBE_OUTPUT");
    char default_path[256];
    if (!path || !path[0]) {
        snprintf(default_path, sizeof(default_path),
                 "/tmp/lgx_probe_%d.bin", getpid());
        path = default_path;
    }

    /* Write using raw syscalls (no malloc) */
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        if (!g_quiet)
            fprintf(stderr, "[LGX-PROBE] ERROR: Cannot open %s: %s\n",
                    path, strerror(errno));
        return;
    }

    /* Header */
    probe_header_t hdr;
    memcpy(hdr.magic, PROBE_MAGIC, 8);
    hdr.version       = PROBE_VERSION;
    hdr.entry_size    = sizeof(probe_entry_t);
    hdr.entry_count   = count;
    hdr.start_time_ns = g_start_time_ns;

    ssize_t ret = write(fd, &hdr, sizeof(hdr));
    (void)ret;

    /* Entries — write in chunks to avoid huge single write */
    const size_t CHUNK = 4096;
    size_t written = 0;
    while (written < count) {
        size_t batch = count - written;
        if (batch > CHUNK) batch = CHUNK;
        ret = write(fd, &g_log_buffer[written], batch * sizeof(probe_entry_t));
        (void)ret;
        written += batch;
    }

    close(fd);

    if (!g_quiet) {
        fprintf(stderr,
                "[LGX-PROBE] Wrote %lu events to %s (%.1f MB)\n",
                (unsigned long)count, path,
                (double)(sizeof(hdr) + count * sizeof(probe_entry_t))
                    / (1024.0 * 1024.0));
    }
}

/* ── Signal handler for manual flush ─────────────────────────────── */

static void sigusr1_handler(int sig) {
    (void)sig;
    dump_log();
}

/* ── Initialization ──────────────────────────────────────────────── */

static void probe_init(void) {
    if (g_initialized) return;

    g_start_time_ns = now_ns();
    g_quiet = (getenv("LGX_PROBE_QUIET") != NULL);

    /* Resolve real libc functions.
     * dlsym returns void*; casting to function pointers is standard
     * POSIX practice but technically non-conforming ISO C. */
    bootstrap_active = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    real_malloc  = dlsym(RTLD_NEXT, "malloc");
    real_free    = dlsym(RTLD_NEXT, "free");
    real_calloc  = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
#pragma GCC diagnostic pop
    bootstrap_active = 0;

    if (!real_malloc || !real_free) {
        fprintf(stderr, "[LGX-PROBE] FATAL: Cannot resolve malloc/free\n");
        _exit(1);
    }

    /* Determine log capacity */
    const char* cap_str = getenv("LGX_PROBE_LOG_ENTRIES");
    g_log_capacity = cap_str ? (uint64_t)atoll(cap_str) : (8ULL * 1024 * 1024);
    if (g_log_capacity < 1024) g_log_capacity = 1024;

    /* Allocate log buffer via mmap (NOT malloc) */
    size_t log_bytes = g_log_capacity * sizeof(probe_entry_t);
    g_log_buffer = mmap(NULL, log_bytes,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                        -1, 0);
    if (g_log_buffer == MAP_FAILED) {
        fprintf(stderr,
                "[LGX-PROBE] FATAL: Cannot mmap %zu bytes for log: %s\n",
                log_bytes, strerror(errno));
        g_log_buffer = NULL;
        _exit(1);
    }

    /* Register dump on exit */
    atexit(dump_log);

    /* SIGUSR1 → manual flush while running */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    g_initialized = true;

    if (!g_quiet) {
        fprintf(stderr,
                "[LGX-PROBE] Initialized: %lu entry slots (%.1f MB)\n",
                (unsigned long)g_log_capacity,
                (double)log_bytes / (1024.0 * 1024.0));
    }
}

/* Use constructor to init early, but also lazy-init in hooks */
__attribute__((constructor))
static void probe_constructor(void) {
    probe_init();
}

/* ── Hooked functions ────────────────────────────────────────────── */

void* malloc(size_t size) {
    /* Bootstrap: dlsym itself needs malloc */
    if (bootstrap_active || !real_malloc) {
        if (!real_malloc) probe_init();
        if (bootstrap_active || !real_malloc)
            return bootstrap_alloc(size);
    }

    /* Re-entrancy guard */
    if (tls_in_hook) return real_malloc(size);
    tls_in_hook = 1;

    void* ptr = real_malloc(size);

    log_event(ENTRY_ALLOC,
              (uint64_t)__builtin_return_address(0),
              (uint64_t)ptr,
              size);

    tls_in_hook = 0;
    return ptr;
}

void free(void* ptr) {
    if (!ptr) return;

    /* Bootstrap pointers are never freed */
    if (is_bootstrap_ptr(ptr)) return;

    if (!real_free) { probe_init(); if (!real_free) return; }

    /* Re-entrancy guard */
    if (tls_in_hook) { real_free(ptr); return; }
    tls_in_hook = 1;

    log_event(ENTRY_FREE,
              (uint64_t)__builtin_return_address(0),
              (uint64_t)ptr,
              0);

    real_free(ptr);
    tls_in_hook = 0;
}

void* calloc(size_t nmemb, size_t size) {
    /* Bootstrap: dlsym may call calloc */
    if (bootstrap_active || !real_calloc) {
        if (!real_calloc) probe_init();
        if (bootstrap_active || !real_calloc) {
            size_t total = nmemb * size;
            void* p = bootstrap_alloc(total);
            if (p) memset(p, 0, total);
            return p;
        }
    }

    if (tls_in_hook) return real_calloc(nmemb, size);
    tls_in_hook = 1;

    void* ptr = real_calloc(nmemb, size);

    log_event(ENTRY_CALLOC,
              (uint64_t)__builtin_return_address(0),
              (uint64_t)ptr,
              nmemb * size);

    tls_in_hook = 0;
    return ptr;
}

void* realloc(void* old_ptr, size_t new_size) {
    if (!old_ptr) return malloc(new_size);
    if (new_size == 0) { free(old_ptr); return NULL; }

    /* Bootstrap pointers: fallback */
    if (is_bootstrap_ptr(old_ptr)) {
        void* new_ptr = malloc(new_size);
        if (new_ptr) {
            size_t old_max = BOOTSTRAP_SIZE -
                (size_t)((char*)old_ptr - bootstrap_buf);
            size_t copy = new_size < old_max ? new_size : old_max;
            memcpy(new_ptr, old_ptr, copy);
        }
        return new_ptr;
    }

    if (!real_realloc) { probe_init(); if (!real_realloc) return NULL; }

    if (tls_in_hook) return real_realloc(old_ptr, new_size);
    tls_in_hook = 1;

    uint64_t ret_addr = (uint64_t)__builtin_return_address(0);

    /* Log the free side */
    log_event(ENTRY_REALLOC_OLD, ret_addr, (uint64_t)old_ptr, 0);

    void* new_ptr = real_realloc(old_ptr, new_size);

    /* Log the alloc side */
    log_event(ENTRY_REALLOC_NEW, ret_addr, (uint64_t)new_ptr, new_size);

    tls_in_hook = 0;
    return new_ptr;
}

/* ── Public API: frame marker ────────────────────────────────────── */

/**
 * Call this at frame boundaries (e.g., after vkQueuePresentKHR).
 * If the target can't call this directly, the analysis script
 * will infer frames from wall-clock intervals.
 */
void lgx_probe_frame_marker(void) {
    uint64_t frame = atomic_fetch_add_explicit(
        &g_frame_counter, 1, memory_order_relaxed);

    log_event(ENTRY_FRAME_MARKER, 0, 0, frame);
}
