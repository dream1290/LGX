/*
 * Compile-and-link gate: the public header must be usable from C++.
 *
 * The platform's architecture principle is "C API with C++ wrappers", so
 * every C++ engine binding includes <lgx_runtime.h>. If that header has
 * unbalanced linkage blocks or C++-incompatible declarations, this file
 * fails to compile or fails to link and the release is blocked.
 *
 * Three linkage regions exist in the header and all three are exercised here:
 *   1. extern "C"        -- the main declaration block (config, log, timing)
 *   2. extern "C++"      -- the static inline convenience allocators
 *   3. extern "C" (tail) -- telemetry / chaos / trace declarations plus the
 *                            static inline lgx_trace_scope_cleanup helper
 *
 * Reaching a symbol at run time is what proves the *linkage* is right, not
 * just the brace balance: a region that accidentally fell out of extern "C"
 * still compiles but produces mangled references that do not resolve against
 * liblgx_runtime.so.
 */

#include <lgx_runtime.h>

#include <cstddef>
#include <cstdio>

/* ---------------------------------------------------------------------------
 * ABI contract that C++ consumers are entitled to rely on.
 * ------------------------------------------------------------------------ */

static_assert(sizeof(lgx_allocation_intent_base_t) > 0,
              "intent struct must be a complete type in C++");
static_assert(LGX_ABI_VERSION >= 1, "ABI version must be at least 1");
static_assert(sizeof(void*) > 0, "void* must be addressable in C++");

/* The convenience allocators are inline: they must be *defined* in the header,
 * so every inline function we reference has to yield an address we can take. */
static_assert(sizeof(&lgx_alloc_frame) > 0, "lgx_alloc_frame must be inline in the header");
static_assert(sizeof(&lgx_trace_scope_cleanup) > 0,
              "lgx_trace_scope_cleanup must be inline in the header");

int main(void) {
    int failures = 0;

    /* --- Region 1: the main extern "C" declaration block ------------------ */
    lgx_runtime_config_t* cfg = lgx_config_create();
    if (cfg == nullptr) {
        std::printf("FAIL: lgx_config_create returned NULL\n");
        ++failures;
    } else {
        lgx_config_set_memory_pool_size(cfg, 64u * 1024u * 1024u);
        lgx_config_destroy(cfg);
    }

    /* Variadic printf-style call: also checks the <cstdarg> ABI is reachable. */
    lgx_log(LGX_LOG_INFO, "C++ header gate reached (abi=%d)", LGX_ABI_VERSION);

    /* Return-by-value struct across the C ABI (no sret/layout surprises). */
    lgx_version_t version = lgx_runtime_get_version();
    if (version.struct_size < sizeof(lgx_version_t)) {
        std::printf("FAIL: version struct_size %zu < %zu\n",
                    version.struct_size, sizeof(lgx_version_t));
        ++failures;
    }
    const uint32_t version_int =
        (version.major << 16) | (version.minor << 8) | version.patch;
    if (version_int != static_cast<uint32_t>(LGX_VERSION_INT)) {
        std::printf("FAIL: version %u.%u.%u does not match header macros\n",
                    version.major, version.minor, version.patch);
        ++failures;
    }

    const uint64_t t0 = lgx_time_now_ns();
    const uint64_t t1 = lgx_time_now_ns();
    if (t1 < t0) {
        std::printf("FAIL: lgx_time_now_ns went backwards\n");
        ++failures;
    }

    const char* result_text = lgx_result_to_string(LGX_SUCCESS);
    if (result_text == nullptr) {
        std::printf("FAIL: lgx_result_to_string returned NULL\n");
        ++failures;
    }

    /* --- Region 2: the extern "C++" convenience allocators ---------------- */
    /* Not calling these: the allocator needs a fully initialised runtime and
     * this gate must not depend on allocator state. Taking their addresses is
     * enough to force the inline bodies to be emitted and their linkage to be
     * resolved by the compiler. */
    void* (*frame_alloc)(size_t) = &lgx_alloc_frame;
    void* (*level_alloc)(size_t) = &lgx_alloc_level;
    void* (*persistent_alloc)(size_t) = &lgx_alloc_persistent;
    void* (*gpu_shared_alloc)(size_t) = &lgx_alloc_gpu_shared;
    if (frame_alloc == nullptr || level_alloc == nullptr ||
        persistent_alloc == nullptr || gpu_shared_alloc == nullptr) {
        std::printf("FAIL: a convenience allocator has a null address\n");
        ++failures;
    }

    /* The C++ branch of the inline block must agree with the C branch on the
     * intent it builds -- this is the payload of region 2. */
    lgx_allocation_intent_base_t intent;
    intent.struct_size = sizeof(lgx_allocation_intent_base_t);
    intent.size = 4096;
    intent.access_pattern = LGX_ACCESS_SEQUENTIAL;
    intent.lifetime = LGX_LIFETIME_FRAME;
    intent.hint = LGX_HINT_CRITICAL_PATH;
    intent.validation_policy = LGX_INTENT_TRUST;
    if (intent.struct_size == 0 || intent.size != 4096) {
        std::printf("FAIL: intent struct layout is unusable from C++\n");
        ++failures;
    }

    /* --- Region 3: the re-opened extern "C" tail -------------------------- */
    lgx_trace_stats_t trace_stats;
    trace_stats.total_events = 0;
    trace_stats.begin_events = 0;
    trace_stats.end_events = 0;
    trace_stats.instant_events = 0;
    trace_stats.dropped_events = 0;
    trace_stats.buffer_capacity = 0;
    trace_stats.buffer_used = 0;

    if (lgx_trace_init() != LGX_SUCCESS) {
        std::printf("FAIL: lgx_trace_init failed\n");
        ++failures;
    } else {
        /* Struct-returning function from the tail region. */
        if (lgx_trace_get_stats(&trace_stats) != LGX_SUCCESS) {
            std::printf("FAIL: lgx_trace_get_stats failed\n");
            ++failures;
        }
        if (lgx_trace_is_enabled()) {
            /* Tracing defaults to disabled; this is an assertion, not a change. */
            std::printf("FAIL: tracing unexpectedly enabled at init\n");
            ++failures;
        }
        if (lgx_trace_shutdown() != LGX_SUCCESS) {
            std::printf("FAIL: lgx_trace_shutdown failed\n");
            ++failures;
        }
    }

    /* lgx_trace_scope_cleanup is a header-only helper living inside the tail
     * extern "C" region. Calling it proves that region is usable from C++. */
    int trace_scope_dummy = 0;
    lgx_trace_scope_cleanup(&trace_scope_dummy);

    if (failures != 0) {
        std::printf("C++ header gate FAILED (%d problem(s))\n", failures);
        return 1;
    }

    std::printf("C++ header gate OK (abi=%d, version=%u.%u.%u)\n",
                LGX_ABI_VERSION, version.major, version.minor, version.patch);
    return 0;
}
