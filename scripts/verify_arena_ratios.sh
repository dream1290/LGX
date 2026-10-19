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
# HOST/CPU COMPARABILITY (a measured constraint, not a design opinion): two
# runs on different CPUs are not comparable -- benchmarks/baseline_frame_arena.txt
# records a 42% 4KB move across CPUs on one machine, and this machine's hybrid
# P-cores/E-cores differ by ~2x. The magnitude bands therefore run ONLY when
# the fresh run executes on the snapshot's host and CPU: the script reads the
# snapshot's host brand and cpu= record and drives the benchmark with
# LGX_BENCH_CPU=<snapshot cpu>, which the binary honours only inside the
# inherited affinity mask. On a different host -- CI runners, a contributor's
# laptop -- the bands are skipped with a loud SNAPSHOT-SKIP line and the gate
# still enforces the structural properties: pinned run, no failed resets, the
# size-ladder cliff. Failing bands on hardware change would turn CI red
# forever, which the plan explicitly excludes; a silent skip would hide the
# regression the gate exists for, so the skip prints.
#
# On failure it prints the two numbers that disagreed before exiting 1, so
# the failure is diagnosable from CI logs alone.
#
# SATURATION (a measured constraint, same class as the cross-CPU hazard):
# under machine load the arena medians move ~2x (this machine, load ~25 on
# 14 cores, measured 16B 470 -> ~900 ns). A loaded run cannot be compared
# with the snapshot, so when the 5-minute load average exceeds the core
# count the magnitude bands are skipped like the cross-host case, with the
# reason printed. Structural checks still run on a loaded machine.
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

# The snapshot block is anchored to the "## Final measurement" heading so
# earlier D2/D3 record blocks in the same document cannot be parsed instead.
SNAPSHOT="$(awk '/^## Final measurement/{f=1; next} f' "$DOC")"
if [ -z "$SNAPSHOT" ]; then
    echo "FAIL: no '## Final measurement' section in $DOC" >&2
    exit 1
fi

# Host/CPU identity from the snapshot's own record. The fresh benchmark is
# directed at the snapshot's CPU via LGX_BENCH_CPU (honoured only inside the
# inherited affinity mask; the binary refuses to widen the mask).
# Host normalization: keep the brand string up to the first parenthetical
# detail, collapse whitespace, so the snapshot's "host: Intel(R) core...
# ... 235U (...)" line and /proc/cpuinfo's model name compare equal.
normalize_host() {
    sed -E 's/[[:space:]]+/ /g; s/^host: //; s/.*: //; s/ \(.+$//; s/^[[:space:]]+//; s/[[:space:]]+$//'
}
SNAP_CPU="$(printf '%s\n' "$SNAPSHOT" | grep -m1 '^BENCH_AFFINITY' | sed -E 's/.*cpu=([0-9]+).*/\1/')"
SNAP_HOST="$(printf '%s\n' "$SNAPSHOT" | grep -m1 '^host: ' | normalize_host | sed -E 's/ ([(].*)$//; s/[[:space:]]+$//')"
LOCAL_HOST="$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | sed 's/.*: //' | sed -E 's/[[:space:]]+/ /g; s/[[:space:]]+$//')"

BANDS_ACTIVE=0
BENCH_ENV=()
LOAD_NOTE=""
if [ -r /proc/loadavg ] && [ -r /proc/cpuinfo ] && grep -q "^processor" /proc/cpuinfo; then
    LOAD5="$(cut -d' ' -f1 /proc/loadavg)"
    NPROC="$(grep -c '^processor' /proc/cpuinfo)"
    LOADED="$(awk -v a="$LOAD5" -v b="$NPROC" 'BEGIN{print (a>b) ? 1 : 0}')"
    if [ "$LOADED" = "1" ]; then
        LOAD_NOTE="machine load ${LOAD5} exceeds ${NPROC} cores; medians move ~2x under load"
        echo "SNAPSHOT-SKIP: $LOAD_NOTE. Magnitude bands skipped by design" \
             "(a loaded run is not comparable with the snapshot); structural checks still run." >&2
    fi
fi
if [ -z "$LOAD_NOTE" ] && [ -n "$SNAP_HOST" ] && [ -n "$LOCAL_HOST" ] && [ "$SNAP_HOST" = "$LOCAL_HOST" ] && [ -n "$SNAP_CPU" ]; then
    BANDS_ACTIVE=1
    BENCH_ENV=(env "LGX_BENCH_CPU=$SNAP_CPU")
elif [ -z "$LOAD_NOTE" ]; then
    echo "SNAPSHOT-SKIP: host/CPU differs from the snapshot (fresh host='${LOCAL_HOST:-unknown}'," \
         "snapshot host='${SNAP_HOST:-unknown}', snapshot cpu=${SNAP_CPU:-none}). Magnitude bands" \
         "are skipped by design (cross-host runs are not comparable); structural checks still run." >&2
fi

TMP_OUT="$(mktemp)"
trap 'rm -f "$TMP_OUT"' EXIT
echo "running pinned benchmark: $BENCH_BIN" >&2
if ! "${BENCH_ENV[@]+"${BENCH_ENV[@]}"}" "$BENCH_BIN" > "$TMP_OUT" 2>&1; then
    echo "FAIL: benchmark run exited non-zero" >&2
    tail -20 "$TMP_OUT" >&2
    exit 1
fi

snap_value() {
    printf '%s\n' "$SNAPSHOT" | grep -m1 "$1" | sed -E 's/.*median=([0-9]+)ns.*/\1/'
}
fresh_value() { # $1: file, $2: pattern
    grep -m1 "$2" "$1" | sed -E 's/.*median=([0-9]+)ns.*/\1/'
}
# $1: row name, $2: fresh, $3: snapshot, $4: lo, $5: hi
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

# 2. Band checks against the snapshot (same host + CPU only).
if [ "$BANDS_ACTIVE" = "1" ]; then
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
fi

echo "PASS: arena ratios gate (fresh vs snapshot, band-failures exit 1)"
