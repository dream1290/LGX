#!/usr/bin/env bash
# Reports the honest test census.
#
# The project advertised "76/76 passing" while roughly a third of those tests
# could not fail. This script separates the three counts that were being
# conflated into one number:
#
#   registered - tests ctest knows about
#   verifying  - registered tests whose source has a failure mechanism
#   incapable  - registered tests that cannot report failure
#   unmapped   - registered tests whose source file could not be identified
#                (counted separately; NEVER folded into verifying)
#
# An unmapped test is not a verifying test. Dropping it, which an earlier
# version of this script did, quietly inflates `verifying` and is exactly the
# failure mode this project keeps committing, so it is reported explicitly.
#
# Detection is NOT reimplemented here: each mapped source is passed to
# scripts/verify_test_integrity.sh --classify, so the gate and the census can
# never disagree about what "capable of failing" means.
#
# Name-to-source mapping, in order of preference. ctest target names are built
# from source names, but with decorations, so several strategies are tried:
#
#   1. exact         <target>.c                        e.g. test_threading_basic
#   2. prefix peel   drop 1 or 2 leading `_` words     e.g. unit_test_error_handler
#                                                          -> test_error_handler
#                                                        perf_test_allocation_latency
#                                                          -> test_allocation_latency
#                                                        benchmark_benchmark_..._throughput
#                                                          -> benchmark_..._throughput
#   3. token match   the `_`-separated words are the same multiset, any order
#                                                        e.g. graphics_test_basic
#                                                          -> test_graphics_basic
#
# Each strategy must produce exactly one file; an ambiguous match is reported
# as unmapped rather than guessed. Sources are searched in tests/ and
# benchmarks/, because benchmarks/ registers ctest targets too.
#
# Usage: scripts/count_tests.sh [build-dir]    (default build dir: build)
# Exit:  0 always, unless the build dir is missing (2).
# ===========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLASSIFY="$REPO_ROOT/scripts/verify_test_integrity.sh"
BUILD_DIR="${1:-build}"

if [ ! -f "$CLASSIFY" ]; then
    echo "FAIL: $CLASSIFY not found." >&2
    exit 2
fi

# Resolve the build dir against the caller's cwd, then against the repo root,
# so this behaves the same from anywhere without ever depending on cwd.
if [ ! -d "$BUILD_DIR" ]; then
    if [ -d "$REPO_ROOT/$BUILD_DIR" ]; then BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
    else
        echo "No CTest build directory at '$BUILD_DIR'."
        echo "Configure one first: cmake -S $REPO_ROOT -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release"
        exit 2
    fi
fi

if ! command -v ctest >/dev/null 2>&1; then
    echo "FAIL: ctest not found on PATH." >&2
    exit 2
fi

CTEST_LIST="$(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null || true)"
TEST_NAMES="$(printf '%s\n' "$CTEST_LIST" | sed -nE 's/^[[:space:]]*Test[[:space:]]+#[0-9]+:[[:space:]]+([^[:space:]]+).*/\1/p')"

registered=0
verifying=0
incapable=0
unmapped=0
UNMAPPED_FILE="$(mktemp)"
trap 'rm -f "$UNMAPPED_FILE"' EXIT

if [ -z "$TEST_NAMES" ]; then
    echo "FAIL: ctest reported no tests in '$BUILD_DIR'." >&2
    echo "      Configure and build first: cmake -S . -B $BUILD_DIR && cmake --build $BUILD_DIR" >&2
    exit 2
fi

# Every candidate test source: relative path -> basename without extension.
mapfile -t SOURCES < <(find "$REPO_ROOT/tests" "$REPO_ROOT/benchmarks" \
    \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \) -type f 2>/dev/null | sort)

# Basename (no extension) -> space separated paths, for exact lookup.
declare -A BY_NAME=()
for src in "${SOURCES[@]}"; do
    base="$(basename "${src%.*}")"
    BY_NAME["$base"]="${BY_NAME[$base]:-} ${src#$REPO_ROOT/}"
done

words_sorted() { printf '%s\n' "$1" | tr '_' '\n' | { grep -v '^$' || true; } | sort | tr '\n' ' '; }

# Echo the single matching path for a ctest target name, or nothing.
resolve_source() {
    local name="$1" words rest
    local -a matches=()

    # 1. exact
    if [ -n "${BY_NAME[$name]:-}" ]; then
        read -r -a matches <<< "${BY_NAME[$name]}"
        if [ "${#matches[@]}" -eq 1 ]; then printf '%s\n' "${matches[0]}"; return 0; fi
        return 1   # duplicate basenames: ambiguous
    fi

    # 2. peel up to two leading underscore-separated words
    rest="$name"
    for _ in 1 2; do
        rest="${rest#*_}"
        [ "$rest" = "$name" ] && break
        [ -n "${BY_NAME[$rest]:-}" ] || continue
        read -r -a matches <<< "${BY_NAME[$rest]}"
        if [ "${#matches[@]}" -eq 1 ]; then printf '%s\n' "${matches[0]}"; return 0; fi
        return 1
    done

    # 3. token multiset, any order
    words="$(words_sorted "$name")"
    for src in "${SOURCES[@]}"; do
        if [ "$(words_sorted "$(basename "${src%.*}")")" = "$words" ]; then
            matches+=("${src#$REPO_ROOT/}")
        fi
    done
    matches=($(printf '%s\n' "${matches[@]}" | sort -u))
    if [ "${#matches[@]}" -eq 1 ]; then printf '%s\n' "${matches[0]}"; return 0; fi
    return 1
}

while IFS= read -r name; do
    [ -n "$name" ] || continue
    registered=$((registered + 1))
    if src="$(resolve_source "$name")"; then
        verdict="$("$CLASSIFY" --classify "$REPO_ROOT/$src" 2>/dev/null | cut -f1 || true)"
        verdict="${verdict:-error}"
        case "$verdict" in
            verifying) verifying=$((verifying + 1)) ;;
            incapable) incapable=$((incapable + 1)) ;;
            *)         unmapped=$((unmapped + 1))
                       printf '%s\t(classifier error: %s)\n' "$name" "$verdict" >> "$UNMAPPED_FILE" ;;
        esac
    else
        unmapped=$((unmapped + 1))
        printf '%s\tno unique source file\n' "$name" >> "$UNMAPPED_FILE"
    fi
done <<< "$TEST_NAMES"

echo "registered=$registered"
echo "verifying=$verifying"
echo "incapable=$incapable"
echo "unmapped=$unmapped"

if [ "$unmapped" -gt 0 ]; then
    echo ""
    echo "UNMAPPED: $unmapped registered test(s) could not be tied to a source file."
    echo "          They are NOT counted as verifying. Add a mapping case or fix the"
    echo "          test name; an unaccounted-for test is how the count drifts."
    sed 's/^/  - /' "$UNMAPPED_FILE"
fi

if [ "$incapable" -gt 0 ]; then
    echo ""
    echo "NOTE: $incapable of $registered registered test(s) cannot report failure."
    echo "      Do not quote the registered count as a pass rate until this is 0."
    echo "      scripts/verify_test_integrity.sh lists them by name."
fi

if [ $((verifying + incapable + unmapped)) -ne "$registered" ]; then
    echo "FAIL: counts do not sum to registered=$registered" >&2
    exit 2
fi