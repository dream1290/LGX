#!/usr/bin/env bash
# Gate: the frame arena must not regress against the recorded snapshot.
#
# Absolute times are machine-dependent, so nothing below is an absolute
# threshold. The gate does two things with a fresh pinned run:
#
#   1. STRUCTURAL (the actual claim): the fresh 256B frame median must be at
#      most 1.5x the fresh 16B median. A game frame arena whose cost jumps
#      non-linearly with size has a cliff at an interesting size; every
#      recorded configuration measured this ratio between 1.01x and 1.13x,
#      so 1.5x is a wide regression band, not a tuned one.
#
#   2. PROVENANCE: each parsed latency is inside a band around the snapshot
#      committed in docs/05-performance/arena-hotpath-profile.md ("Final
#      measurement" block). A band wide enough to survive cross-row noise is
#      ONE that still catches the D2 regression it exists to re-catch, so the
#      bands are per-row-class and carry the justification:
#
#        size ladder (16B/64B/256B)   [0.6x, 1.5x]
#          The regression this gate must be able to catch is the D2 state
#          (per-reset stderr fprintf back on the timed path): 770/741/810ns
#          against the snapshot's 470/454/532ns = 1.64x/1.63x/1.52x. A band
#          of 1.5x catches all three; the plan's sketch band [0.5x, 2.0x]
#          would have PASSED the pre-fix state, which is exactly what the
#          plan tells us not to ship. Recorded post-D2 run-to-run movement
#          at these sizes is under 5%.
#        MIXED_LINE                   [0.6x, 1.6x]
#          The mixed-size scenario moves +-13% between invocations on one
#          machine (recorded medians 80.4-91.8us); 1.6x still bounds it.
#        bump floors (bump_floor_16B) [0.5x, 2.5x]
#          The bare-pointer floor occasionally prints ~1.9x its typical value
#          on a cold first measure (81ns typical, one recorded 154ns); the
#          floor was never the regression vector and is provenance only.
#
# On failure it prints the two numbers that disagreed before exiting 1, so
# the failure is diagnosable from CI logs alone.
#
# Usage: scripts/verify_arena_ratios.sh [benchmark-binary] [profile-doc]
#   Defaults: build/benchmarks/benchmark_benchmark_allocation_throughput
#             docs/05-performance/arena-hotpath-profile.md (both under the repo)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCH_BIN="${1:-$REPO_ROOT/build/benchmarks/benchmark_benchmark_allocation_throughput}"
DOC="${2:-$REPO_ROOT/docs/05-performance/arena-hotpath-profile.md}"

if [ ! -x "$BENCH_BIN" ]; then
    echo "FAIL: benchmark binary not found or not executable: $BENCH_BIN" >&2
    echo "      Build it first: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build" >&2
    exit 1
fi
if [ ! -f "$DOC" ]; then
    echo "FAIL: snapshot document not found: $DOC" >&2
    exit 1
fi

"$REPO_ROOT/scripts/check_gate_scripts_tracked.sh" "$REPO_ROOT" >&2

TMP_OUT="$(mktemp)"
trap 'rm -f "$TMP_OUT"' EXIT
echo "running pinned benchmark: $BENCH_BIN" >&2
if ! "$BENCH_BIN" > "$TMP_OUT" 2>&1; then
    echo "FAIL: benchmark run exited non-zero" >&2
    tail -20 "$TMP_OUT" >&2
    exit 1
fi

# The snapshot block is anchored to the "## Final measurement" heading so
# earlier D2/D3 record blocks in the same document cannot be parsed instead.
SNAPSHOT="$(awk '/^## Final measurement/{f=1; next} f' "$DOC")"
if [ -z "$SNAPSHOT" ]; then
    echo "FAIL: no '## Final measurement' section in $DOC" >&2
    exit 1
fi

snap_value() {
    printf '%s\n' "$SNAPSHOT" | grep -m1 "$1" | sed -E 's/.*median=([0-9]+)ns.*/\1/'
}
fresh_value() { # $1: file, $2: pattern
    grep -m1 "$2" "$1" | sed -E 's/.*median=([0-9]+)ns.*/\1/'
}
# $1: row name, $2: fresh, $3: snapshot, $4..: two multipliers lo hi
in_band() {
    local name="$1" fresh="$2" snap="$3" lo="$4" hi="$5"
    if [ "$snap" -le 0 ]; then
        echo "FAIL: $name: snapshot median is not a positive number ($snap)" >&2
        exit 1
    fi
    # awk keeps fractions where bash arithmetic would lose them
    local ratio
    ratio="$(awk -v a="$fresh" -v b="$snap" 'BEGIN{printf "%.3f", a/b}')"
    local ok
    ok="$(awk -v r="$ratio" -v lo="$lo" -v hi="$hi" 'BEGIN{print (r>=lo && r<=hi) ? 1 : 0}')"
    if [ "$ok" != "1" ]; then
        echo "FAIL: $name" >&2
        echo "      fresh=$fresh ns  snapshot=$snap ns  ratio=$ratio" >&2
        echo "      required band [$lo, $hi]" >&2
        exit 1
    fi
    echo "PASS: $name  ratio=$ratio (band [$lo, $hi])" >&2
}

# 0. Methodology sanity on the fresh run itself: pinned, no failed resets.
if ! grep -q "^BENCH_AFFINITY cpu=[0-9]* status=pinned" "$TMP_OUT"; then
    echo "FAIL: fresh run is not pinned (missing BENCH_AFFINITY status=pinned)" >&2
    exit 1
fi
if ! grep -q "FRAME_RESET_STATUS .* failures=0" "$TMP_OUT"; then
    echo "FAIL: fresh run reports failed frame resets; numbers invalid" >&2
    grep "FRAME_RESET_STATUS" "$TMP_OUT" >&2 || true
    exit 1
fi

# 1. The structural cliff: fresh 256B <= 1.5 * fresh 16B (from the same run).
FRESH_16="$(fresh_value "$TMP_OUT" '^FRAME_RESET_PATTERN size=16B median=')"
FRESH_256="$(fresh_value "$TMP_OUT" '^FRAME_RESET_PATTERN size=256B median=')"
if [ -z "$FRESH_16" ] || [ -z "$FRESH_256" ]; then
    echo "FAIL: could not parse FRAME_RESET_PATTERN 16B/256B medians from fresh run" >&2
    exit 1
fi
CLIFF="$(awk -v a="$FRESH_256" -v b="$FRESH_16" 'BEGIN{printf "%.3f", a/b}')"
if [ "$(awk -v r="$CLIFF" 'BEGIN{print (r<=1.5) ? 1 : 0}')" != "1" ]; then
    echo "FAIL: size-ladder cliff" >&2
    echo "      fresh 16B=$FRESH_16 ns  256B=$FRESH_256 ns  ratio=$CLIFF" >&2
    echo "      required: 256B <= 1.5 * 16B" >&2
    exit 1
fi
echo "PASS: size-ladder cliff  16B=$FRESH_16 256B=$FRESH_256 ratio=$CLIFF (<= 1.5)" >&2

# 2. Band checks against the snapshot.
in_band "lgx_frame_16B  vs snapshot" \
    "$FRESH_16" "$(snap_value '^FRAME_RESET_PATTERN size=16B median=')" 0.6 1.5
in_band "lgx_frame_64B  vs snapshot" \
    "$(fresh_value "$TMP_OUT" '^FRAME_RESET_PATTERN size=64B median=')" \
    "$(snap_value '^FRAME_RESET_PATTERN size=64B median=')" 0.6 1.5
in_band "lgx_frame_256B vs snapshot" \
    "$FRESH_256" "$(snap_value '^FRAME_RESET_PATTERN size=256B median=')" 0.6 1.5
in_band "MIXED_LINE     vs snapshot" \
    "$(fresh_value "$TMP_OUT" '^MIXED_LINE median=')" \
    "$(snap_value '^MIXED_LINE median=')" 0.6 1.6
in_band "bump_floor_16B vs snapshot" \
    "$(fresh_value "$TMP_OUT" '^FLOOR_LINE bump_floor_16B median=')" \
    "$(snap_value '^FLOOR_LINE bump_floor_16B median=')" 0.5 2.5

echo "PASS: arena ratios gate (fresh vs snapshot, band-failures exit 1)"
