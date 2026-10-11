# GitHub Actions workflows — what actually runs

This file was previously a description of what these workflows were *supposed*
to do. Most of it was fiction: several of the workflows have never been able to
pass, because they invoke files that do not exist or binaries that are never
built. It has been rewritten to say what is true as of 2026-10-10.

Only `ci.yml` has been repaired. The other eight are listed here as an accurate
inventory and are **deliberately left broken** — the ground-truth plan scopes
their repair to a later task. Nothing here has been edited to look better than
it is.

## Status

| Workflow | Runs | Verdict |
|----------|------|---------|
| `ci.yml` | yes | **Working.** `build-and-test` and `asan-ubsan` pass; `no-vulkan` is red on purpose. |
| `abi-compatibility.yml` | no | **Broken.** Runs `scripts/lgx-abi-test-matrix.sh`, which does not exist. |
| `chaos-testing.yml` | no | **Broken.** Runs four `test_chaos_*` binaries that are never built — no such sources exist. |
| `compatibility-matrix.yml` | yes | **Meaningless.** Every `ctest` invocation ends in `\|\| true`; the generated report checks a library path that is never where CMake puts it. |
| `coverity.yml` | unknown | **Unassessable.** Gated on `secrets.COVERITY_SCAN_TOKEN`; the job is skipped when it is unset. |
| `fuzzing.yml` | probably | **Unverified.** Its build scripts and `testcases/` exist; not exercised locally. |
| `multi-distro.yml` | no | **Broken.** `cmake --install` fails (see below). |
| `performance-regression.yml` | yes | **Its gate is unreliable.** See below. |
| `release.yml` | no | **Broken.** Its test-verification step cannot pass on any tree. |

## The one root cause behind two of them

`tests/abi/CMakeLists.txt` ends with:

```cmake
install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/lgx-abi-test-matrix.sh
    DESTINATION bin
    RENAME lgx-abi-test-matrix
)
```

`scripts/lgx-abi-test-matrix.sh` has never been committed — `.gitignore` ignores
`/scripts/*` — so **every** `cmake --install` in this repository fails:

```
$ cmake --install build --prefix /tmp/x
...
No such file or directory.
Call Stack (most recent call first):
  build/cmake_install.cmake:399 (include)
```

That single missing file breaks `abi-compatibility.yml`, `multi-distro.yml`'s
install step, and the install verification the previous `ci.yml` performed.
It is also the clearest argument for the `/scripts/*` ignore rule: it has
already silently swallowed a script the build system depends on. Repairing it
means either committing the script or removing the `install(PROGRAMS)` line, and
neither is in scope for the work that touched `ci.yml`.

## ci.yml

Three jobs. The gate is `build-and-test`; later plans extend it rather than
adding a parallel one.

### `build-and-test` — the merge gate

| Step | What it enforces |
|------|------------------|
| Configure (Release) | A tree configures from scratch. No `-G Ninja`: the default generator is what `count_tests.sh` was verified against. |
| Build | Zero compiler warnings **in this project's sources**. `CMakeLists.txt:14` already carries `-Werror`; the grep catches what `-Werror` does not. The grep is scoped on purpose — an unscoped `grep warning:` also matches three `libasan.so.8: warning: the use of tmpnam is dangerous` lines from the Debug build, none of them ours. |
| Gate scripts tracked | `scripts/check_gate_scripts_tracked.sh`. `.gitignore` ignores `/scripts/*` and un-ignores only the gates; this is the check that notices when that stops being true. |
| C++ header gate | `abi_test_cpp_header_compiles`. The public header must compile *and link* as C++17. |
| Test suite | `.github/ci/run-tests.sh` — see below. |
| Benchmark methodology | `scripts/verify_benchmark_methodology.sh`. |
| Test integrity census | `scripts/count_tests.sh`. **Blocking**, and safe to block: it exits non-zero only for real faults (unconfigured tree, or a registered test that cannot be tied to a source). |
| Test integrity ratchet | `scripts/verify_test_integrity.sh`, **not** blocking. See below. |

There is no `cmake --install` step, because it cannot succeed.

### Honest-failure policy

Three tests fail in a default Release build and two more under the sanitizers.
None are CI's to fix and none are hidden.

`.github/ci/run-tests.sh` runs **every** registered test, then judges the
failures by name against `.github/ci/known-test-failures.txt`:

* a failure that is not listed → `::error::` and **exit 1** (a regression)
* a failure that is listed → `::warning::`, tolerated, reported with its reason
* a listed test that passed → `::warning::` the entry is stale, delete it

Intermittent tests are **not** only allowlisted. `test_threading_basic`,
`test_threading_stress` and `benchmark_threading` are unfixed intermittent
defects in the threading module (measured locally at 1/30, 5/30 and 3/30 runs;
`benchmark_threading` *hangs* until the 60 s ctest timeout). They are passed to
the step as a flaky regex and **retried** with `--repeat until-pass:3`, so a
test that keeps failing is still a failure. Nothing is skipped: `-E` only takes
the flaky set out of the first pass so it can be retried, and it runs
immediately afterwards.

If `ctest` reports failure but its summary cannot be parsed, the script exits 2
rather than reporting PASS. A gate that cannot tell the difference between
"green" and "I did not understand the output" is the thing being replaced.

### The test-integrity ratchet

`scripts/verify_test_integrity.sh` exits 1 today: **30 of 98** test files have
no live failure mechanism, so they pass unconditionally.

* Making it blocking buys a check that is red on every commit forever. A
  permanently red check is disabled within a week and is then worth nothing.
* `|| true` buys the opposite failure: a gate that cannot fail.

So the step prints the census, writes it into the job summary, and **fails if
the number grows** past the recorded baseline of 30. New tests that cannot fail
are caught immediately; the existing backlog is tracked rather than forgotten.
When Plan 03 repairs the backlog, the baseline is lowered in the same commit.

### `asan-ubsan`

Runs the Debug configuration, whose `-fsanitize=address -fsanitize=undefined
-mavx2` come from `CMakeLists.txt:17`. It reports truthfully and does not claim
the suite is green under sanitizers. Two tests fail here that pass in Release,
both recorded under the `asan` label:

* `abi_test_cpp_header_compiles` aborts before `main()` with *"ASan runtime does
  not come first in initial library list"*. `CMakeLists.txt` puts the sanitizer
  flags in `CMAKE_C_FLAGS_DEBUG` and never in `CMAKE_CXX_FLAGS_DEBUG`, so the
  C++ test binary is uninstrumented while the library it links is.
* `audio_test_basic` fails LeakSanitizer on ~38 KB leaked from
  libpipewire/libasound inside `snd_pcm_open`. It is the one test with no
  `LSAN_OPTIONS` suppression set. On a runner with no sound device this may not
  reproduce, which is why it is recorded rather than assumed.

### `no-vulkan` — red on purpose

Building without Vulkan does not work. `src/runtime/lgx_intent_allocator.c`
guards two of its three Vulkan call sites with `#if HAVE_VULKAN` (lines 152 and
195) but calls `lgx_gpu_alloc()` unguarded at line 278, while
`src/runtime/lgx_gpu_pool.c` — the only definition — is compiled only when
CMake found Vulkan:

```
$ cmake -S . -B build-novk -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=ON
-- Vulkan not found - GPU memory pool features will be disabled
$ cmake --build build-novk
/usr/bin/ld: liblgx_runtime.so.1.0.1: undefined reference to `lgx_gpu_alloc'
... exit 2
```

The job is left failing rather than deleted, because deleting it is how the
defect became invisible in the first place. `build-and-test` does not depend on
it, so the merge gate is unaffected, and the job's name says what it is.
**Do not add it to required checks while it is red.** When Plan 01 guards line
278 the build stops failing and the job's final step exits 0 on its own; at that
point delete that step and the job becomes a real gate.

## The other eight

### `abi-compatibility.yml` — broken
The `abi-compatibility` job fails at its "Run ABI compatibility tests" step,
which does `chmod +x scripts/lgx-abi-test-matrix.sh` on a file that does not
exist. Everything before it (configure, build `lgx_runtime`, build the three
ABI test targets) does work; the job then dies and the upload/report steps,
which are `if: always()`, upload nothing.

### `chaos-testing.yml` — broken
All four jobs build the tree and then run `./test_chaos_memory_pressure`,
`./test_chaos_latency_spikes`, `./test_chaos_io_errors` or
`./test_chaos_combined` from `build/tests/phase0`. No source file matching
`test_chaos_*` exists anywhere under `tests/`, so CMake never builds those
binaries and every job dies on "No such file or directory".

### `compatibility-matrix.yml` — runs, proves nothing
Every `ctest` call ends in `|| true`, so no test result can ever fail the job.
The generated report then checks `[ -f build-debug/src/liblgx_runtime.so ]` and
`[ -f build-release/src/liblgx_runtime.so ]`; CMake writes the library at the
top of the build tree, not under `src/`, so the report always prints
"Build Status: Failed" regardless of what happened. `gpu-compatibility-test` and
`kernel-compatibility-test` are explicit `exit 0` placeholders.

### `coverity.yml` — unassessable
Gated on `secrets.COVERITY_SCAN_TOKEN`; with the secret unset the job is
skipped and reports green. With it set, the download is guarded by
`wget ... || exit 0` but the very next line `tar xzf coverity_tool.tgz` is not,
so a failed download fails that step (which is `continue-on-error`), after
which `cov-build` runs against a `coverity-tool/bin` that was never extracted.

### `fuzzing.yml` — plausible, unverified
`tests/fuzzing/build_libfuzzer.sh`, `tests/fuzzing/build_afl.sh` and
`tests/fuzzing/testcases/` all exist, and the shape is defensible: the run step
ends in `|| true` and a separate step then checks for crashes. Not run locally.

### `multi-distro.yml` — broken
"Test installation" runs `cmake --install build --prefix /tmp/lgx-install`,
which fails on the missing `scripts/lgx-abi-test-matrix.sh` (above). Its
`ctest -L "unit|integration"` also selects 6 of the 77 registered tests (5 with
the `unit` label, 1 with `integration`), so even if the install worked it would
be checking under a tenth of the suite.

### `performance-regression.yml` — runs, gate is unreliable
"Compare performance" redirects python's stdout into `regression_report.md` and
appends `REGRESSION_DETECTED=true` to `$GITHUB_OUTPUT` only when python exits
non-zero. Python prints `REGRESSION_DETECTED=false` **into the report file** on
success, never into `$GITHUB_OUTPUT`, so a clean comparison leaves the output
variable unset and the following step is skipped — correct outcome by accident.
The real problem is the other direction: if no `benchmark_results_*.txt` is
produced on either side, the comparison finds nothing to compare, prints "No
significant performance changes detected" and exits 0, and a total measurement
failure is indistinguishable from a clean run. It also `git checkout`s the
baseline ref over the working tree it just built.

### `release.yml` — broken, cannot pass on any tree
Its "Verify test results" step does:

```bash
PASSED=$(ctest --show-only=json-v1 | jq '[.tests[] | select(.status == "Passed")] | length')
```

`--show-only` *lists* tests; it does not run them, and every `.status` comes
back `null`. Verified on this tree: `jq '[.tests[].status] | unique'` → `[null]`,
and the `select(.status == "Passed")` expression → `0`. So `PASSED` is always 0
while `TOTAL` is 77, and the step always exits 1 — on a fully green run too. It
also runs the full `ctest` with no allowance for the three known failures.
`create-release` depends on this job, so no release can be cut.

## Adding or changing a workflow here

1. Run every command it contains, locally, in order, and record the exit code.
   "It looks right" is not evidence; three of the workflows above were written
   with commands nobody ever ran.
2. Confirm every path it references exists in a fresh checkout. The single most
   common failure in this directory is a script or binary that does not exist.
3. If a step cannot fail, say so in its name. A step called "Run tests" that
   ends in `|| true` is worse than no step.
