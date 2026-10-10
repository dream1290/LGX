#!/usr/bin/env bash
# Fails if the frame-arena benchmark does not exercise the reset path.
#
# Rationale: a frame arena's benefit is elided free() and reuse of hot memory.
# A benchmark that allocates monotonically without resetting measures the
# technique's worst case and cannot support any speedup claim.
#
# The check looks for an actual CALL to lgx_frame_reset(), not merely for the
# identifier: a prototype or a mention in a comment must not satisfy it, or
# deleting the reset call would pass unnoticed.
#
# Usage: scripts/verify_benchmark_methodology.sh [benchmark-source]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCH="${1:-$REPO_ROOT/benchmarks/benchmark_allocation_throughput.c}"

if [ ! -f "$BENCH" ]; then
    echo "FAIL: benchmark source not found: $BENCH"
    exit 1
fi

# Strip comment-only lines and "//" line comments, then drop any prototype
# ("... lgx_frame_reset(void);") so only real call sites remain.
reset_calls="$(
    sed -e 's://.*::' "$BENCH" \
        | grep -vE '^[[:space:]]*(\*|/\*|#)' \
        | grep -E 'lgx_frame_reset[[:space:]]*\(' \
        | grep -vE 'lgx_frame_reset[[:space:]]*\([[:space:]]*void[[:space:]]*\)' \
        || true
)"

if [ -z "$reset_calls" ]; then
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
