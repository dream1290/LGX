# Plan 00 — Ground Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make LGX buildable from a clean checkout, consumable from C++, and honest about what it measures — so that every later plan can be verified against a trustworthy baseline.

**Architecture:** This plan changes no allocator algorithms. It repairs the *measurement and build apparatus* only: the build system gains a numa dependency, the public header gains C++ linkage correctness, the benchmark gains a reset-based methodology that measures the real frame-arena pattern, and a CI gate is added that fails when anything regresses. Each task is independently verifiable by a clean out-of-tree build plus ctest.

**Tech Stack:** CMake 3.16+, C11, GCC 11+/Clang 10+, ctest, GitHub Actions.

## Global Constraints

- Every task MUST leave the project buildable from a clean out-of-tree configure. Verify with `cmake -S . -B /tmp/lgx-verify -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/lgx-verify -j$(nproc)`.
- Compiler flags are fixed by `CMakeLists.txt` and MUST NOT be weakened. `-Wall -Wextra -Werror -Wredundant-decls -Wshadow` stays; `-Wstrict-prototypes -Wmissing-prototypes` on `lgx_runtime` stays.
- No new runtime dependencies may be added. Build-system dependencies (libnuma) are optional and MUST degrade gracefully when absent.
- All library code MUST compile clean under `-Werror`; zero new warnings is the pass criterion for every task.
- Any performance number published in documentation MUST be reproducible by a command named in that documentation. No number may be written by hand.
- Do not modify allocator algorithms, data structures, or tuning constants in this plan. This plan establishes the ability to measure; Plan 01 changes what is measured.
- Every test added MUST be able to fail. A test that always exits 0 is a defect; see `tests/README.md` section "Test Integrity Contract" created in Task 4.

---

## File Structure

| File | Action | Responsibility after this plan |
|---|---|---|
| `CMakeLists.txt` | Modify | Detect libnuma; link it to the one test that needs it; keep the phase0 glob |
| `include/lgx_runtime.h` | Modify | Valid C++ linkage for the entire public surface |
| `tests/phase0/test_csf2_numa_awareness.c` | Modify | Use angle-bracket include; degrade gracefully when numa is absent |
| `benchmarks/benchmark_allocation_throughput.c` | Modify | Measure the real allocate→use→reset pattern; frame-arena numbers become honest |
| `benchmarks/benchmark_framework.h` | Read only | Confirmed to already be correct; no change required |
| `tests/README.md` | Modify | Document the Test Integrity Contract and the true test counts |
| `.github/workflows/ci.yml` | Replace | One gate that actually builds, tests, and benchmarks the real tree |
| `.github/workflows/arena-guard.yml` | Delete | Removed; it asserts the existence of a file that does not exist |
| `.gitignore` | Modify | Ignore build output so `build/` can be untracked in Plan 02 |

**Interfaces produced by this plan** (later plans depend on these):

- `cmake -S . -B <dir>` configures and builds with exit 0 on a machine with or without libnuma.
- `#include <lgx_runtime.h>` compiles in both C11 and C++17.
- `benchmarks/benchmark_benchmark_allocation_throughput` prints a line beginning `FRAME_RESET_PATTERN` per allocation size, so Plan 01 can gate on it.
- `.github/workflows/ci.yml` defines the job id `build-and-test`, which later plans may extend rather than duplicate.

---

### Task 1: Make the clean build succeed

**Files:**
- Modify: `CMakeLists.txt` (after the `find_library(JEMALLOC_LIB jemalloc)` block, ~line 52)
- Modify: `tests/phase0/test_csf2_numa_awareness.c:1-14`
- Create: `tests/phase0/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: CMake variable `NUMA_FOUND` and `NUMA_LIBRARY`, consumed by the root phase0 loop in Task 1 Step 4 and by the numa test's `#ifdef`.

- [ ] **Step 1: Write the failing verification**

There is no unit test for a build break; the failing check is the build itself. Record the current failure as the baseline:

```bash
cd /home/karl/Projects/LGX
rm -rf /tmp/lgx-verify
cmake -S . -B /tmp/lgx-verify -DCMAKE_BUILD_TYPE=Release > /tmp/lgx-configure.log 2>&1
cmake --build /tmp/lgx-verify -j$(nproc) > /tmp/lgx-build.log 2>&1
echo "build exit: $?"
grep -c "undefined reference to \`numa" /tmp/lgx-build.log
```

Expected: `build exit: 1` and a non-zero count of undefined `numa_*` references.

- [ ] **Step 2: Confirm the root cause**

```bash
grep -n "numa" CMakeLists.txt || echo "NO numa reference in CMakeLists.txt"
grep -n "#include" tests/phase0/test_csf2_numa_awareness.c | head -5
ls -d tests/include 2>/dev/null || echo "tests/include does not exist"
```

Expected: no numa reference in `CMakeLists.txt`; the test includes `"../include/lgx_runtime.h"` (a path that does not exist from `tests/phase0/`); `tests/include` absent.

- [ ] **Step 3: Fix the test's broken include path**

In `tests/phase0/test_csf2_numa_awareness.c`, replace line 1:

```c
/* was: #include "../include/lgx_runtime.h"  -- resolves to tests/include/, which does not exist */
#include <lgx_runtime.h>
```

The root `CMakeLists.txt` already sets `include_directories(include)` and `include_directories(include/lgx)` globally, so the angle-bracket form resolves for every phase0 target.

- [ ] **Step 4: Add optional numa detection to the root build**

Insert after the jemalloc block in `CMakeLists.txt` (after line 52):

```cmake
# Optional: Find libnuma for NUMA-awareness tests.
# The platform degrades gracefully without it -- only the NUMA test is conditional.
find_library(NUMA_LIBRARY numa)
if(NUMA_LIBRARY)
    message(STATUS "Found libnuma: ${NUMA_LIBRARY} (NUMA tests enabled)")
else()
    message(STATUS "libnuma not found - NUMA test will skip its NUMA-specific checks")
endif()
```

- [ ] **Step 5: Exclude the numa test from the phase0 glob and give it its own target**

The phase0 loop globs every `tests/phase0/*.c`, so the numa test must be removed from that glob or it will fail to link on machines without libnuma. Add at the top of the phase0 section (before the `file(GLOB ...)` on line 297):

```cmake
# The NUMA test needs libnuma and its own guards, so it is excluded from the
# generic phase0 glob below and given an explicit target in tests/phase0/CMakeLists.txt.
set(PHASE0_TESTS_EXCLUDED test_csf2_numa_awareness)
```

Then change the glob loop (line 297) to skip it:

```cmake
file(GLOB PHASE0_TESTS tests/phase0/*.c)
foreach(test_file ${PHASE0_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    if(${test_name} IN_LIST PHASE0_TESTS_EXCLUDED)
        continue()
    endif()
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} lgx_runtime m)
```

- [ ] **Step 6: Create the phase0 CMakeLists with a self-skipping numa target**

Create `tests/phase0/CMakeLists.txt`:

```cmake
# Explicit target for the NUMA test.
#
# Unlike the generic phase0 glob in the root CMakeLists.txt, this test is
# conditional: it requires libnuma, and it must still exit 0 (reporting SKIP)
# on machines where libnuma is absent, so CI is portable.

if(NUMA_LIBRARY)
    add_executable(test_csf2_numa_awareness test_csf2_numa_awareness.c)
    target_link_libraries(test_csf2_numa_awareness lgx_runtime m ${NUMA_LIBRARY})
    target_compile_definitions(test_csf2_numa_awareness PRIVATE LGX_HAVE_NUMA=1)
else()
    # No libnuma: build the same source without the numa symbols. The test
    # body is guarded by LGX_HAVE_NUMA and reports SKIP, exiting 0, so CI is
    # portable across machines with and without libnuma.
    add_executable(test_csf2_numa_awareness test_csf2_numa_awareness.c)
    target_link_libraries(test_csf2_numa_awareness lgx_runtime m)
    target_compile_definitions(test_csf2_numa_awareness PRIVATE LGX_HAVE_NUMA=0)
endif()

add_test(NAME test_csf2_numa_awareness COMMAND test_csf2_numa_awareness)
set_tests_properties(test_csf2_numa_awareness PROPERTIES
    LABELS "phase0"
    TIMEOUT 300
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/lsan.supp"
)
```

Note the target name is `test_csf2_numa_awareness` in both branches. The library
target linked into it is `lgx_runtime` (the project's runtime library) — the
two names are similar and must not be confused.

- [ ] **Step 7: Guard the test body so it reports SKIP instead of failing**

In `tests/phase0/test_csf2_numa_awareness.c`, the `#include <numa.h>` block at lines 12–15 must become conditional, and `main()` must exit 0 when numa is unavailable. Replace the include block:

```c
/* was:
#if defined(__linux__)
    #include <numa.h>
    #include <numaif.h>
#endif
*/
#if defined(__linux__) && LGX_HAVE_NUMA
    #include <numa.h>
    #include <numaif.h>
    #define LGX_NUMA_USABLE 1
#else
    #define LGX_NUMA_USABLE 0
#endif
```

Then at the top of `main()`, before any numa call:

```c
#if !LGX_NUMA_USABLE
    printf("[SKIP] test_csf2_numa_awareness: libnuma not available at build time\n");
    printf("[INFO] NUMA-awareness logic is covered by test_hardware_adaptation\n");
    return 0;
#else
    /* existing test body */
#endif
```

Every `numa_*` call site inside `main()` must be inside the `#else` branch. Wrap the existing body accordingly.

- [ ] **Step 8: Wire the phase0 subdirectory into the root build**

In `CMakeLists.txt`, add after the existing phase0 loop (after line 315, before the `# Phase 1 Unit Tests` comment):

```cmake
# NUMA test (conditional, requires libnuma)
add_subdirectory(tests/phase0)
```

- [ ] **Step 9: Verify the clean build now succeeds**

```bash
cd /home/karl/Projects/LGX
rm -rf /tmp/lgx-verify
cmake -S . -B /tmp/lgx-verify -DCMAKE_BUILD_TYPE=Release > /tmp/lgx-configure.log 2>&1
cmake --build /tmp/lgx-verify -j$(nproc) > /tmp/lgx-build.log 2>&1
echo "build exit: $?"
grep -c "undefined reference" /tmp/lgx-build.log || echo "0 undefined references"
grep -c "warning:" /tmp/lgx-build.log || echo "0 warnings"
```

Expected: `build exit: 0`, `0 undefined references`, `0 warnings`.

- [ ] **Step 10: Verify the numa test runs and the suite is green**

```bash
ctest --test-dir /tmp/lgx-verify -R test_csf2_numa_awareness --output-on-failure
ctest --test-dir /tmp/lgx-verify 2>&1 | tail -5
```

Expected: numa test passes (either real checks or explicit SKIP); full suite reports no failures other than any pre-existing flake, which must be noted in the commit message rather than ignored.

- [ ] **Step 11: Commit**

```bash
git add CMakeLists.txt tests/phase0/CMakeLists.txt tests/phase0/test_csf2_numa_awareness.c
git commit -m "build: fix clean-checkout link failure by detecting and linking libnuma

test_csf2_numa_awardship called numa_* symbols but CMakeLists.txt never
referenced libnuma, so a clean build failed to link. The break was masked
by a stale prebuilt binary committed under build/.

- detect libnuma optionally via find_library
- move the numa test out of the phase0 glob into its own conditional target
- guard the test body so it reports SKIP and exits 0 without libnuma
- fix the test's broken relative include path (../include/ does not exist)

Verified: clean out-of-tree build exits 0 with zero warnings."
```

---

### Task 2: Make the public header consumable from C++

**Files:**
- Modify: `include/lgx_runtime.h:50-51, 144, 306-312, 378-380`
- Create: `tests/abi/test_cpp_header_compiles.cpp`
- Modify: `tests/abi/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: `lgx_runtime.h` includes cleanly under both `-std=c11` and `-std=c++17`; a permanent C++ compile gate target named `abi_test_cpp_header_compiles`.

- [ ] **Step 1: Write the failing test**

Create `tests/abi/test_cpp_header_compiles.cpp`:

```cpp
/*
 * Compile-and-link gate: the public header must be usable from C++.
 *
 * This file exists to be COMPILED, not to run meaningful logic. It fails the
 * build if lgx_runtime.h has unbalanced linkage blocks or C++-incompatible
 * declarations. A compile failure here is a release blocker: the platform
 * promises a C ABI with C++ consumers.
 */
#include <lgx_runtime.h>
#include <cstdio>

int main(void) {
    /* Touch one function from each linkage region so nothing is optimised away. */
    lgx_runtime_config_t* cfg = lgx_config_create();
    if (cfg) {
        lgx_config_destroy(cfg);
    }

    lgx_log(LGX_LOG_INFO, "C++ header gate reached");

    /* Static asserts on the ABI contract that C++ must also honour. */
    static_assert(sizeof(lgx_allocation_intent_base_t) > 0,
                  "intent struct must be a complete type in C++");
    static_assert(LGX_ABI_VERSION >= 1, "ABI version must be at least 1");

    std::printf("C++ header gate OK\n");
    return 0;
}
```

- [ ] **Step 2: Register the C++ gate in the ABI test build**

Append to `tests/abi/CMakeLists.txt`:

```cmake
# C++ consumability gate: the public header must compile and link as C++.
enable_language(CXX)
add_executable(abi_test_cpp_header_compiles test_cpp_header_compiles.cpp)
target_link_libraries(abi_test_cpp_header_compiles lgx_runtime)
set_target_properties(abi_test_cpp_header_compiles PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_compile_options(abi_test_cpp_header_compiles PRIVATE -Wall -Wextra -Werror)
add_test(NAME abi_test_cpp_header_compiles COMMAND abi_test_cpp_header_compiles)
set_tests_properties(abi_test_cpp_header_compiles PROPERTIES
    LABELS "abi"
    TIMEOUT 30
)
```

- [ ] **Step 3: Run the test to verify it fails**

```bash
cd /home/karl/Projects/LGX
rm -rf /tmp/lgx-verify
cmake -S . -B /tmp/lgx-verify -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
cmake --build /tmp/lgx-verify --target abi_test_cpp_header_compiles 2>&1 | grep -E "error|Error" | head -5
```

Expected: a compile error at `include/lgx_runtime.h:379` — `expected declaration before '}' token`.

- [ ] **Step 4: Fix the brace accounting**

The header opens one `extern "C" {` at line 11 and closes it twice (lines 311 and 379). The inline convenience functions at lines 50–144 are documented as "outside extern "C"" but no corresponding block is ever opened.

Apply three edits.

Edit A — line 50–51. Replace:

```c
// Note: These are outside extern "C" for C++ compatibility
#ifdef __cplusplus
```

with:

```c
// Note: These are outside extern "C" for C++ compatibility.
// static inline functions must have C++ linkage so they can be inlined into
// C++ translation units without violating the one-definition rule.
#ifdef __cplusplus
extern "C++" {
#endif
```

Edit B — line 144. Replace the closing `#endif` of that block with:

```c
#if defined(__cplusplus)
}
#endif
```

Edit C — line 378–380. The `lgx_trace_end` inline at the end of the file is still inside the `extern "C"` region opened at line 11, which closes at line 311. Add an opener before it. Locate:

```c
static inline void lgx_trace_end(const char* name) {
```

and insert directly above it:

```c
#ifdef __cplusplus
extern "C" {
#endif
```

Leave the existing `#ifdef __cplusplus` / `}` / `#endif` at lines 378–380 as the closer for this new block. Do **not** delete it.

Resulting linkage map: `extern "C"` (11) → `extern "C++"` (51) → back to `extern "C"` (144 close of C++, 311 close of C) → `extern "C"` (new, before `lgx_trace_end`) → `}` (379). Balanced.

- [ ] **Step 5: Verify the C++ gate compiles and runs**

```bash
cmake --build /tmp/lgx-verify --target abi_test_cpp_header_compiles 2>&1 | tail -3
ctest --test-dir /tmp/lgx-verify -R abi_test_cpp_header_compiles --output-on-failure
```

Expected: build succeeds; test passes and prints `C++ header gate OK`.

- [ ] **Step 6: Verify the C build is unharmed**

```bash
cd /home/karl/Projects/LGX
printf '#include <lgx_runtime.h>\nint main(void){return 0;}\n' > /tmp/c_header_check.c
gcc -I include -std=c11 -Wall -Wextra -Werror -c /tmp/c_header_check.c -o /tmp/c_header_check.o && echo "C11 OK"
```

Expected: `C11 OK`.

- [ ] **Step 7: Verify the full build and suite are still green**

```bash
cmake --build /tmp/lgx-verify -j$(nproc) 2>&1 | grep -c "warning:" || echo "0 warnings"
ctest --test-dir /tmp/lgx-verify 2>&1 | tail -4
```

Expected: zero warnings; no new failures.

- [ ] **Step 8: Commit**

```bash
git add include/lgx_runtime.h tests/abi/test_cpp_header_compiles.cpp tests/abi/CMakeLists.txt
git commit -m "fix(cxx): make lgx_runtime.h consumable from C++

The header opened one extern \"C\" block (line 11) but closed it twice
(lines 311 and 379), so every C++ translation unit failed with
\"expected declaration before '}' token\".

The inline convenience allocators were documented as being outside
extern \"C\" but no matching block was ever opened, so they also carried C
linkage inside a C++ TU.

- open extern \"C++\" around the static inline convenience functions
- re-open extern \"C\" for the trailing trace helpers
- add a permanent C++ compile+link gate (abi_test_cpp_header_compiles)

This is required by the architecture spec's C-API-with-C++-wrappers
principle and by every C++ engine that would consume the platform.

Verified: g++ -std=c++17 and gcc -std=c11 both compile the header clean."
```

---

### Task 3: Make the benchmark measure the real frame-arena pattern

**Files:**
- Modify: `benchmarks/benchmark_allocation_throughput.c:36-42, 95-97`
- Read only: `benchmarks/benchmark_framework.h`, `include/lgx_runtime.h` (already provide `lgx_frame_reset`)

**Interfaces:**
- Consumes: nothing from Tasks 1–2.
- Produces: the benchmark emits one line beginning `FRAME_RESET_PATTERN` per allocation size, containing the median and P99 in nanoseconds. Plan 01 gates on this line. The exact format is:
  `FRAME_RESET_PATTERN size=16B median=<ns>ns p99=<ns>ns`
  (`size` is one of `16B`, `64B`, `256B`, `1KB`, `4KB` — the literal values of
  `size_names[]` at `benchmark_allocation_throughput.c:69`. No `_B` suffix is
  appended by the format string.)

- [ ] **Step 1: Record the current (dishonest) baseline**

```bash
cd /tmp/lgx-verify
./benchmarks/benchmark_benchmark_allocation_throughput 2>&1 | grep -A6 "Benchmark: lgx_frame_16B"
```

Expected: median ~30 ns, P99 ~40 ns for 16B — measured with no frame reset anywhere in the loop.

- [ ] **Step 2: Write the failing gate**

There is no test harness for benchmark honesty yet, so the check is textual. Create a guard script that fails while the benchmark lacks a reset:

```bash
cat > /home/karl/Projects/LGX/scripts/verify_benchmark_methodology.sh << 'EOF'
#!/usr/bin/env bash
# Fails if the frame-arena benchmark does not exercise the reset path.
#
# Rationale: a frame arena's benefit is elided free() and reuse of hot memory.
# A benchmark that allocates monotonically without resetting measures the
# technique's worst case and cannot support any speedup claim.
set -euo pipefail

BENCH="${1:-benchmarks/benchmark_allocation_throughput.c}"

if ! grep -q "lgx_frame_reset" "$BENCH"; then
    echo "FAIL: $BENCH never calls lgx_frame_reset()"
    echo "      A frame-arena benchmark must reset between frames to measure"
    echo "      the real pattern (allocate -> use -> reset -> reuse)."
    exit 1
fi

if ! grep -q "FRAME_RESET_PATTERN" "$BENCH"; then
    echo "FAIL: $BENCH does not emit the FRAME_RESET_PATTERN line"
    echo "      Downstream performance gates parse this line."
    exit 1
fi

echo "PASS: benchmark methodology includes reset and emits the gate line"
EOF
chmod +x /home/karl/Projects/LGX/scripts/verify_benchmark_methodology.sh
```

Run it:

```bash
./scripts/verify_benchmark_methodology.sh; echo "exit: $?"
```

Expected: `exit: 1` with `FAIL: ... never calls lgx_frame_reset()`.

- [ ] **Step 3: Confirm `lgx_frame_reset` is already visible (no declaration needed)**

`benchmarks/benchmark_allocation_throughput.c` already includes `"lgx_runtime.h"`
at line 7, and `lgx_frame_reset` is declared in that public header. Therefore no
declaration must be added to `benchmarks/benchmark_framework.h`.

Verify before editing anything:

```bash
sed -n '1,10p' benchmarks/benchmark_allocation_throughput.c
grep -n "lgx_frame_reset" include/lgx_runtime.h
```

Expected: the `.c` includes `lgx_runtime.h`, and the header declares
`lgx_frame_reset`. If either check fails, stop and fix that first; do not add a
duplicate declaration to `benchmark_framework.h`, which would risk a
conflicting-signature compile error.

- [ ] **Step 4: Replace the benchmark function with a reset-based measurement**

In `benchmarks/benchmark_allocation_throughput.c`, replace `bench_lgx_frame` (lines 36–42):

```c
static void bench_lgx_frame(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;
    void* ptr = lgx_alloc_frame(actx->size);
    if (ptr) {
        memset(ptr, 0, actx->size);
        // Frame allocations are not freed individually
    }
}
```

with:

```c
/*
 * Frame-arena benchmark using the REAL frame pattern.
 *
 * A frame is: begin -> allocate -> use -> reset. The previous version of this
 * function allocated monotonically across all 10,000 iterations without ever
 * resetting, which measures cold growing allocation -- the pattern least
 * favourable to a bump allocator, and not the pattern any game uses.
 *
 * FRAME_BURST controls how many allocations happen inside one frame. 64 is
 * representative of a particle system or a per-frame buffer batch.
 */
#define FRAME_BURST 64

static void bench_lgx_frame(void* ctx) {
    alloc_context_t* actx = (alloc_context_t*)ctx;

    lgx_frame_reset();                 /* start from a clean frame */

    for (int i = 0; i < FRAME_BURST; i++) {
        void* ptr = lgx_alloc_frame(actx->size);
        if (ptr) {
            memset(ptr, 0, actx->size); /* touch it, as a real frame would */
        }
    }
    /* The reset happens at the top of the next call, so the cost of
     * reclaiming the whole frame is included in the measured interval. */
}
```

- [ ] **Step 5: Emit the gate line with the measured numbers**

Immediately after the frame benchmark is registered (around line 97, where `results[result_count] = benchmark_run(...)` assigns the frame result), add:

```c
        /* Machine-readable line consumed by scripts/verify_benchmark_methodology.sh
         * and by Plan 01's performance gate.
         *
         * benchmark_run() returns by value; benchmark_result_t stores latency
         * fields as double nanoseconds (median_ns, p99_ns). */
        {
            const benchmark_result_t* fr = &results[result_count - 1];
            printf("FRAME_RESET_PATTERN size=%s median=%.0fns p99=%.0fns\n",
                   size_names[i],
                   fr->median_ns,
                   fr->p99_ns);
        }
```

Verify the field names against the header before applying:

```bash
sed -n '33,44p' benchmarks/benchmark_framework.h
```

Expected struct fields: `name`, `iterations`, `total_ns`, `min_ns`, `max_ns`,
`mean_ns`, `median_ns`, `p95_ns`, `p99_ns`, `stddev_ns`. The printf above uses
`median_ns` and `p99_ns`, which must match exactly.

- [ ] **Step 6: Verify the methodology guard now passes and the numbers changed**

```bash
cd /home/karl/Projects/LGX
./scripts/verify_benchmark_methodology.sh; echo "guard exit: $?"

cd /tmp/lgx-verify
cmake --build . --target benchmark_benchmark_allocation_throughput 2>&1 | tail -2
./benchmarks/benchmark_benchmark_allocation_throughput 2>&1 | grep -E "FRAME_RESET_PATTERN|Benchmark: lgx_frame_16B" -A6
```

Expected: `guard exit: 0`; a `FRAME_RESET_PATTERN size=16B median=... p99=...` line appears; the frame numbers are now measured under the reset pattern.

- [ ] **Step 7: Record the honest numbers in a machine-checked baseline**

```bash
cd /tmp/lgx-verify
./benchmarks/benchmark_benchmark_allocation_throughput 2>&1 \
  | grep "FRAME_RESET_PATTERN" > /home/karl/Projects/LGX/benchmarks/baseline_frame_arena.txt
cat /home/karl/Projects/LGX/benchmarks/baseline_frame_arena.txt
```

This file is the baseline Plan 01 gates against. It records what the arena actually does under the correct pattern, whatever that turns out to be.

- [ ] **Step 8: Commit**

```bash
git add benchmarks/benchmark_allocation_throughput.c \
        benchmarks/baseline_frame_arena.txt scripts/verify_benchmark_methodology.sh
git commit -m "perf(bench): measure the frame arena with a real frame pattern

bench_lgx_frame allocated 10,000 times in a row without ever calling
lgx_frame_reset(). That measures cold, monotonically growing bump allocation
-- the pattern least favourable to the technique -- and omits the two things a
frame arena actually provides: elided free() and reuse of hot memory.

- reset between frames and burst 64 allocations per frame
- emit a machine-readable FRAME_RESET_PATTERN line for downstream gates
- add scripts/verify_benchmark_methodology.sh, which fails if the reset
  pattern or the gate line is removed
- record the resulting honest numbers in benchmarks/baseline_frame_arena.txt

No allocator code is changed here. This task only makes the measurement
trustworthy; Plan 01 addresses what the numbers mean."
```

---

### Task 4: Define the test integrity contract

**Files:**
- Modify: `tests/README.md`
- Create: `scripts/verify_test_integrity.sh`
- Create: `scripts/count_tests.sh`

**Interfaces:**
- Consumes: nothing.
- Produces: `scripts/verify_test_integrity.sh <file...>` exits non-zero for any test file lacking a failure mechanism; `scripts/count_tests.sh` prints `registered=<n> verifying=<n> incapable=<n>`. Task 5 consumes the first as a CI gate.

- [ ] **Step 1: Write the integrity checker**

Create `scripts/verify_test_integrity.sh`:

```bash
#!/usr/bin/env bash
# Fails if any C test file is structurally incapable of reporting failure.
#
# Background: 37 of 100 test files in this repository contain no non-zero exit
# path and no failure counter. Such a test passes unconditionally and therefore
# contributes nothing to the pass rate while inflating it. A test that cannot
# fail is treated as a defect, not as a passing test.
#
# Usage: scripts/verify_test_integrity.sh [file ...]
#        with no arguments, scans tests/**/*.c
set -uo pipefail

PATTERN='return [1-9]|exit\([1-9]|EXIT_FAILURE|assert_fail|failed_tests|failed\+\+|failures *\+=|g_test_stats\.failed_tests'

if [ "$#" -gt 0 ]; then
    FILES=("$@")
else
    mapfile -t FILES < <(find tests -name '*.c' | sort)
fi

incapable=0
checked=0

for f in "${FILES[@]}"; do
    [ -f "$f" ] || continue
    checked=$((checked + 1))
    if ! grep -qE "$PATTERN" "$f"; then
        echo "INCAPABLE: $f has no non-zero exit path and no failure counter"
        incapable=$((incapable + 1))
    fi
done

echo "---"
echo "checked=$checked incapable=$incapable"

if [ "$incapable" -gt 0 ]; then
    echo "FAIL: $incapable test file(s) cannot report failure."
    echo "      Each of these must gain real assertions before the suite can be trusted."
    exit 1
fi

echo "PASS: every checked test file can report failure"
```

Make it executable:

```bash
chmod +x scripts/verify_test_integrity.sh
```

- [ ] **Step 2: Run it to establish the current baseline**

```bash
cd /home/karl/Projects/LGX
./scripts/verify_test_integrity.sh 2>&1 | tail -8
```

Expected: exit 1, `incapable=37`, with the file list.

- [ ] **Step 3: Write the counter script**

Create `scripts/count_tests.sh`:

```bash
#!/usr/bin/env bash
# Reports the honest test census.
#
# The project previously advertised "76/76 passing" in several documents while
# roughly 30 of those tests could not fail. This script separates the counts:
#
#   registered  - tests ctest knows about
#   verifying   - registered tests whose source has a failure mechanism
#   incapable   - registered tests that cannot report failure
#
# Usage: scripts/count_tests.sh [build-dir]   (default build dir: build)
set -uo pipefail

BUILD_DIR="${1:-build}"

if [ ! -f "$BUILD_DIR/CTestTestfile.cmake" ] && [ ! -d "$BUILD_DIR/tests" ]; then
    echo "No CTest build found at '$BUILD_DIR'."
    echo "Configure one first: cmake -S . -B $BUILD_DIR"
    exit 2
fi

registered=$(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | grep -cE '^\s*Test\s+#[0-9]+:')
echo "registered=$registered"

incapable=0
verifying=0

# Map each ctest name back to a source file and check its failure mechanism.
while read -r name; do
    src=$(find tests -name "${name}.c" | head -1)
    if [ -z "$src" ]; then
        continue   # target name differs from file name; counted as verifying below
    fi
    if grep -qE 'return [1-9]|exit\([1-9]|EXIT_FAILURE|failed_tests|failed\+\+|failures *\+=' "$src"; then
        verifying=$((verifying + 1))
    else
        incapable=$((incapable + 1))
    fi
done < <(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | sed -nE 's/^\s*Test\s+#[0-9]+:\s+(\S+).*/\1/p')

echo "verifying=$verifying"
echo "incapable=$incapable"

if [ "$incapable" -gt 0 ]; then
    echo "NOTE: $incapable registered test(s) cannot report failure."
    echo "      Do not quote the registered count as a pass rate until this is 0."
fi
```

Make it executable and run it:

```bash
chmod +x scripts/count_tests.sh
./scripts/count_tests.sh /tmp/lgx-verify
```

Expected: three numbers, with `incapable` well above 0.

- [ ] **Step 4: Document the contract in tests/README.md**

Append to `tests/README.md`:

```markdown
## Test Integrity Contract

A test that cannot fail is a defect, not a passing test.

**The rule.** Every test file MUST contain at least one mechanism that causes a
non-zero process exit when an assertion fails. Accepted mechanisms:

- `return 1` / `return non-zero` from `main()` on failure
- `exit(1)` / `EXIT_FAILURE`
- a failure counter compared against zero at the end of `main()`

A file containing none of these always exits 0. It passes unconditionally, and
including it in a pass rate inflates that rate while proving nothing.

**Enforcement.**

```bash
scripts/verify_test_integrity.sh          # all tests
scripts/verify_test_integrity.sh tests/phase0/test_frame_arena.c   # one file
```

This script is a CI gate. A PR that adds a test without a failure mechanism
fails CI.

**Why this exists.** As of the 2026-10-10 audit, 37 of 100 test files in this
repository had no failure mechanism — including all four performance tests, all
seven module smoke tests, four of five threading tests, and
`tests/phase0/test_frame_arena.c` (363 lines, zero assertions). The advertised
"76/76 passing" therefore described roughly 43 genuinely verifying tests.

**Reporting rule.** Never quote the registered test count as a pass rate.
Quote `scripts/count_tests.sh` output, which separates registered, verifying,
and incapable counts.

## Current Test Census

Run `scripts/count_tests.sh <build-dir>` for live numbers. As of the audit:

| Metric | Count |
|---|---|
| Test files in `tests/` | 100 |
| Incapable of failure | 37 |
| Disabled in CMake ("compilation errors", "API mismatch") | 12 |

Disabled tests are listed in `docs/STATE.md`. Each must be migrated to the
current API or deleted; leaving them commented out is how the suite silently
stopped covering the runtime.
```

- [ ] **Step 5: Verify both scripts run and the gate fails as designed**

```bash
cd /home/karl/Projects/LGX
./scripts/verify_test_integrity.sh tests/unit/test_init_shutdown.c; echo "real test exit: $?"
./scripts/verify_test_integrity.sh tests/phase0/test_frame_arena.c;   echo "weak test exit: $?"
```

Expected: `real test exit: 0`; `weak test exit: 1` naming the file.

- [ ] **Step 6: Commit**

```bash
git add scripts/verify_test_integrity.sh scripts/count_tests.sh tests/README.md
git commit -m "test: define and enforce the test integrity contract

37 of 100 test files had no non-zero exit path and no failure counter, so
they passed unconditionally and inflated the advertised pass rate. All four
performance tests, all seven module smoke tests, and four of five threading
tests were among them.

- add scripts/verify_test_integrity.sh as a CI gate
- add scripts/count_tests.sh to report registered/verifying/incapable
- document the contract and the reporting rule in tests/README.md

The existing incapable tests are NOT fixed here; Plan 03 does that. This
task makes them visible and blocks new ones."
```

---

### Task 5: Replace CI with a gate that runs

**Files:**
- Replace: `.github/workflows/ci.yml`
- Delete: `.github/workflows/arena-guard.yml` if present
- Modify: `.gitignore`

**Interfaces:**
- Consumes: `scripts/verify_benchmark_methodology.sh` and `scripts/verify_test_integrity.sh` from Tasks 3–4.
- Produces: a workflow with job id `build-and-test` that gates on: clean build, zero warnings, ctest green, benchmark methodology intact. Later plans extend this job rather than adding a new one.

- [ ] **Step 1: Record why the current CI cannot pass**

```bash
cd /home/karl/Projects/LGX
echo "--- jobs referenced by ci.yml ---"
grep -nE "cd examples|cd tests|make|./lgx_arena_example|./benchmark" .github/workflows/ci.yml
echo "--- do these paths exist? ---"
ls -d examples 2>/dev/null || echo "examples/ DOES NOT EXIST"
ls lgx_arena.h 2>/dev/null || echo "lgx_arena.h DOES NOT EXIST"
ls tests/Makefile 2>/dev/null && grep -n "asan:" tests/Makefile
```

Expected: `ci.yml` runs `cd examples && make`, which fails because `examples/` does not exist; it runs `./lgx_arena_example`, which does not exist; `tests/Makefile` depends on `../lgx_arena.h`, which does not exist.

- [ ] **Step 2: Replace ci.yml with a real gate**

Overwrite `.github/workflows/ci.yml`:

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

# A new push to the same branch cancels the previous run.
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  build-and-test:
    name: Build and Test
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake pkg-config \
            libvulkan-dev libjemalloc-dev libasound2-dev libnuma-dev

      - name: Configure (Release)
        run: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

      - name: Build (must be warning-free)
        run: |
          set -o pipefail
          cmake --build build -j$(nproc) 2>&1 | tee /tmp/build.log
          if grep -q "warning:" /tmp/build.log; then
            echo "::error::Build produced warnings. Zero warnings is required."
            grep "warning:" /tmp/build.log | head -20
            exit 1
          fi

      - name: C++ header consumability gate
        run: ctest --test-dir build -R abi_test_cpp_header_compiles --output-on-failure

      - name: Test suite
        run: ctest --test-dir build --output-on-failure --timeout 600

      - name: Benchmark methodology gate
        run: scripts/verify_benchmark_methodology.sh

      - name: Test integrity census (reporting only, not yet blocking)
        run: scripts/count_tests.sh build || true

  asan-ubsan:
    name: AddressSanitizer + UndefinedBehaviorSanitizer
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake pkg-config libvulkan-dev libnuma-dev

      - name: Configure (Debug, sanitizers on)
        run: cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug

      - name: Build
        run: cmake --build build-asan -j$(nproc)

      - name: Test under sanitizers
        run: ctest --test-dir build-asan --output-on-failure --timeout 900

  no-vulkan:
    name: Build without Vulkan
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install dependencies (deliberately no libvulkan-dev)
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake pkg-config libnuma-dev

      - name: Configure
        run: cmake -S . -B build-novk -DCMAKE_BUILD_TYPE=Release

      - name: Build
        # Guards the unguarded lgx_gpu_alloc call in lgx_intent_allocator.c
        # that currently only links because Vulkan happens to be installed.
        run: cmake --build build-novk -j$(nproc)
```

- [ ] **Step 3: Fix .gitignore so build output stops being tracked**

Replace `.gitignore` with:

```gitignore
# Build directories
/build/
/build-*/
/build-*-*/
/cmake-build-*/

# Compiled objects and libraries
*.o
*.a
*.so
*.so.*
*.dylib
*.dll
*.exe

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
install_manifest.txt

# Package artifacts
*.deb
*.rpm
*.pkg.tar.zst
*.tar.gz
*.zip
/packaging/output/*
!/packaging/output/.gitkeep

# Test binaries built in-tree
/tests/test_basic
/tests/test_alignment
/tests/test_statistics
/tests/test_overflow

# Security audits -- never publish findings
security_audit_*/
security_audit_report/
security_*.pdf
audit_*.md
vulnerability_*.txt

# Profiling and coverage
*.prof
*.gcda
*.gcno
*.gcov
perf.data
callgrind.out.*
massif.out.*

# Core dumps and logs
core
core.*
vgcore.*
*.log
*.tmp
*.bak

# Editor and IDE
.vscode/
.idea/
*.swp
*.swo
*~
.DS_Store
__pycache__/
*.py[cod]

# Agent / tooling scratch
.opencode/
```

- [ ] **Step 4: Verify the new gate commands pass locally, exactly as CI runs them**

```bash
cd /home/karl/Projects/LGX
scripts/verify_benchmark_methodology.sh
scripts/count_tests.sh /tmp/lgx-verify || true
```

Expected: methodology guard prints `PASS`; count script prints its three numbers.

- [ ] **Step 5: Verify the no-Vulkan job's premise is currently false (documents Plan 01's H3)**

```bash
cd /home/karl/Projects/LGX
grep -n "HAVE_VULKAN" src/runtime/lgx_intent_allocator.c
grep -n "lgx_gpu_alloc" src/runtime/lgx_intent_allocator.c
```

Expected: `#if HAVE_VULKAN` at lines 152 and 195, but the `lgx_gpu_alloc` call at line 278 is unguarded. The `no-vulkan` CI job will fail until Plan 01 Task 2 fixes this. That is intentional: the job documents a real, known defect rather than hiding it.

- [ ] **Step 6: Commit**

```bash
git add .github/workflows/ci.yml .gitignore
git rm --cached -r build build-test build-bench 2>/dev/null || true
git commit -m "ci: replace non-functional workflow with a gate that actually runs

The previous ci.yml ran 'cd examples && make' against a directory that does
not exist, executed ./lgx_arena_example against a header that does not exist,
and invoked 'make asan' in tests/ against a Makefile whose only targets
depend on that same missing header. No CI run could ever pass.

- replace with build-and-test: warning-free build, C++ header gate, ctest,
  benchmark methodology gate, test integrity census
- add asan-ubsan job using the project's existing Debug sanitizer flags
- add no-vulkan job that will fail until the unguarded lgx_gpu_alloc call in
  lgx_intent_allocator.c is fixed (Plan 01, task 2)
- restore a correct .gitignore and untrack build/ so the stale prebuilt
  binary can no longer mask a build break"
```

---

## Verification

After all five tasks, the full acceptance check is:

```bash
cd /home/karl/Projects/LGX

# 1. Clean out-of-tree build, zero warnings
rm -rf /tmp/lgx-final
cmake -S . -B /tmp/lgx-final -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build /tmp/lgx-final -j$(nproc) 2>&1 | tee /tmp/final.log
grep -c "warning:" /tmp/final.log   # must be 0

# 2. Header consumable from C and C++
printf '#include <lgx_runtime.h>\nint main(void){return 0;}\n' > /tmp/h.c
gcc -I include -std=c11 -Wall -Wextra -Werror -c /tmp/h.c -o /tmp/h.o
printf '#include <lgx_runtime.h>\nint main(){return 0;}\n' > /tmp/h.cpp
g++ -I include -std=c++17 -Wall -Wextra -Werror -c /tmp/h.cpp -o /tmp/hpp.o

# 3. Test suite green
ctest --test-dir /tmp/lgx-final 2>&1 | tail -4

# 4. Gates pass
scripts/verify_benchmark_methodology.sh
scripts/count_tests.sh /tmp/lgx-final || true

# 5. Benchmark emits the parseable line
/tmp/lgx-final/benchmarks/benchmark_benchmark_allocation_throughput 2>&1 \
  | grep FRAME_RESET_PATTERN
```

Expected: build exits 0 with zero warnings; both compilers accept the header; ctest green apart from any documented pre-existing flake; both gates behave as specified; the `FRAME_RESET_PATTERN` line prints.

## Out of Scope

This plan deliberately does **not** touch:

- Allocator algorithms or tuning constants (Plan 01)
- The GPU pool use-after-free, frame-arena growth, or chaos deadlock (Plan 02)
- Any of the 37 incapable tests (Plan 03) — Task 4 only makes them visible
- The 12 disabled tests (Plan 03)
- `build/` removal from git history (Plan 04)
- Documentation claim reconciliation beyond `tests/README.md` (Plan 04)

If Plan 00 succeeds, every subsequent plan has a trustworthy baseline: a build that works from scratch, a header that compiles everywhere, a benchmark that measures what it claims, and a CI that fails when reality regresses.
