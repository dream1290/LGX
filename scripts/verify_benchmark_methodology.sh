#!/usr/bin/env bash
# Fails if the frame-arena benchmark loses any of the properties that make its
# numbers trustworthy. Every check below corresponds to a fix that was, at some
# point, silently revertible without anything failing.
#
#   1. the benchmark actually calls lgx_frame_reset()
#   2. it emits the FRAME_RESET_PATTERN line downstream gates parse
#   3. stderr is muted around the timed region (the reset fprintf's on every call
#      in a microbenchmark, which made the number depend on the caller's stderr)
#   4. the reset return value is compared against LGX_SUCCESS (an uninitialized
#      arena makes the reset a silent no-op and the measurement a lie)
#   5. the harness pins itself to one CPU (unpinned runs move by >2x)
#
# Rationale for 1-2: a frame arena's benefit is elided free() and reuse of hot
# memory. A benchmark that allocates monotonically without resetting measures
# the technique's worst case and cannot support any speedup claim.
# Rationale for 3-5: a measurement that changes with the invocation, ignores a
# failed reset, or wanders across CPUs cannot be gated on.
#
# These are textual checks, deliberately: they must run with no build step.
# They match CALL SITES, not the identifier -- a prototype, a mention in a
# comment, or the same text inside a string literal must not satisfy them.
#
# Usage: scripts/verify_benchmark_methodology.sh [benchmark-source]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCH="${1:-$REPO_ROOT/benchmarks/benchmark_allocation_throughput.c}"

if [ ! -f "$BENCH" ]; then
    echo "FAIL: benchmark source not found: $BENCH"
    exit 1
fi

# Source with comments and string literals neutralised. Comment-only lines are
# blanked rather than deleted so that line numbers still refer to the real file.
strip_noise() {
    sed -E -e 's:/\*[^*]*\*/: :g' \
           -e 's:^[[:space:]]*(\*|/\*).*$::' \
           -e 's://.*::' \
           -e 's:"[^"]*": :g' \
        "$BENCH"
}

CODE="$(strip_noise)"

fail() {
    echo "FAIL: $1"
    shift
    for line in "$@"; do echo "      $line"; done
    exit 1
}

# First line number of a genuine CALL to $1: excludes the definition/prototype
# form "name(void)" as well as comments and string literals.
call_line() {
    printf '%s\n' "$CODE" \
        | grep -nE "[^_[:alnum:]]$1[[:space:]]*\(" \
        | grep -vE "$1[[:space:]]*\([[:space:]]*void[[:space:]]*\)" \
        | head -1 | cut -d: -f1 || true
}

# 1. A real call to lgx_frame_reset(), not the prototype, a comment or prose.
reset_line="$(call_line lgx_frame_reset)"
[ -n "$reset_line" ] || fail "$BENCH never calls lgx_frame_reset()" \
    "A frame-arena benchmark must reset between frames to measure" \
    "the real pattern (allocate -> use -> reset -> reuse)."

# 2. The gate line downstream performance gates parse.
grep -q "FRAME_RESET_PATTERN" "$BENCH" \
    || fail "$BENCH does not emit the FRAME_RESET_PATTERN line" \
           "Downstream performance gates parse this line."

# 3. stderr muted, and muted AROUND the timed frame benchmark rather than
#    somewhere in the file: begin < benchmark_run(bench_lgx_frame) < end.
mute_begin_line="$(call_line stderr_mute_begin)"
mute_end_line="$(call_line stderr_mute_end)"
frame_run_line="$(call_line benchmark_run)"
frame_run_line="$(printf '%s\n' "$CODE" | grep -nE 'benchmark_run[[:space:]]*\([^;]*bench_lgx_frame' | head -1 | cut -d: -f1 || true)"

if [ -z "$mute_begin_line" ] || [ -z "$mute_end_line" ] || [ -z "$frame_run_line" ]; then
    fail "$BENCH does not bracket the timed frame benchmark with a stderr mute" \
        "lgx_frame_reset() fprintf's to stderr on every reset in a microbenchmark," \
        "which made the measured cost depend on where the caller's stderr points." \
        "Expected: stderr_mute_begin() ... benchmark_run(..., bench_lgx_frame, ...) ... stderr_mute_end()"
elif [ "$mute_begin_line" -ge "$frame_run_line" ] || [ "$mute_end_line" -le "$frame_run_line" ]; then
    fail "$BENCH calls the stderr mute, but not around the timed frame benchmark" \
        "stderr_mute_begin() at line $mute_begin_line, frame benchmark_run() at line $frame_run_line," \
        "stderr_mute_end() at line $mute_end_line -- the mute must enclose the timing."
fi

# 4. The reset result is compared against LGX_SUCCESS, on the call line or
#    within the next two lines (for the `rc = lgx_frame_reset(); if (rc != ...)`
#    shape).
if ! printf '%s\n' "$CODE" | awk '
    { line[NR] = $0 }
    END {
        found = 0
        for (i = 1; i <= NR; i++) {
            if (line[i] !~ /lgx_frame_reset[[:space:]]*\(/) continue
            if (line[i] ~ /lgx_frame_reset[[:space:]]*\([[:space:]]*void[[:space:]]*\)/) continue
            win = line[i]
            if (i + 1 <= NR) win = win " " line[i + 1]
            if (i + 2 <= NR) win = win " " line[i + 2]
            if (win ~ /LGX_SUCCESS/ && win ~ /(!=|==)/) found = 1
        }
        exit(found ? 0 : 1)
    }'; then
    fail "$BENCH discards the lgx_frame_reset() return value" \
        "An uninitialized arena makes the reset a silent no-op, so the benchmark" \
        "would measure the no-reset pattern while still passing this gate." \
        "Expected a comparison against LGX_SUCCESS."
fi

# 5a. The run record names the CPU it used. "pinned" without an id cannot be
#     compared with anything, and the whole premise of this baseline is that runs
#     from different CPUs are not comparable.
grep -qE 'BENCH_AFFINITY cpu=%d' "$BENCH" \
    || fail "$BENCH does not record which CPU it ran on" \
        "BENCH_AFFINITY must print the CPU id (cpu=%d), not just the word" \
        "pinned: two runs cannot be compared unless the baseline says which CPU" \
        "produced it."

# 5b. The reset total must be checkable against the work that was requested,
#     rather than hard-coded by the reader.
grep -qE 'expected_resets=%llu' "$BENCH" \
    || fail "$BENCH does not report expected_resets on FRAME_RESET_STATUS" \
        "A gate must be able to assert the reset count against" \
        "sizes x repeats x (iterations + warmup)."

# 5. The harness pins itself to one CPU.
pin_line="$(call_line pin_to_single_cpu)"
[ -n "$pin_line" ] || fail "$BENCH never calls pin_to_single_cpu()" \
    "Unpinned runs of this benchmark move by more than 2x, which makes an" \
    "absolute-nanosecond gate meaningless. Pinning must be in the program."

echo "PASS: benchmark methodology includes reset, reset-result check, stderr mute around" \
     "the timed region, CPU pinning, and emits the gate line"
