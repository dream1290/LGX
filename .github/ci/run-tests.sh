#!/usr/bin/env bash
#
# Run the whole ctest suite, then decide pass/fail against a reviewed list.
#
# WHY THIS EXISTS
# ---------------
# This repository has real, pre-existing test failures. Three in the default
# build, two more under the sanitizers. They are not CI's fault and they are
# not this script's to fix.
#
# The two obvious ways to handle them are both wrong:
#
#   * require 100% green   -> the gate is red forever. A permanently red check
#                             gets disabled within a week, and when it is
#                             re-enabled nobody believes it any more.
#   * append `|| true`     -> a brand new regression is indistinguishable from
#                             the backlog. The build stays green while the
#                             suite rots. This is the failure mode the whole
#                             ground-truth plan exists to stop.
#
# So neither. Every test is RUN, every failure is compared BY NAME against
# .github/ci/known-test-failures.txt, and:
#
#   failure not listed        -> ::error:: annotation, exit 1   (a regression)
#   failure listed            -> ::warning:: annotation, tolerated, and reported
#                                in the job summary with its recorded reason
#   listed test that PASSED   -> ::warning:: annotation: the allowlist is stale
#                                and the entry should be deleted
#   nothing failed            -> exit 0
#
# INTERMITTENT tests are not allowlisted, because an allowlist entry for a test
# that fails 30% of the time is an allowlist entry that hides real breakage.
# They are passed in as a flaky regex and RETRIED with --repeat until-pass:N.
# Only if such a test still fails after N attempts is it treated as a failure
# like any other -- so a hang that stops being intermittent still fails.
#
# NOTHING IS SKIPPED. `-E` appears below only to take the flaky set out of the
# first pass so it can be retried; it is run immediately afterwards, and a test
# that is never retried cannot be silently dropped.
#
# Exit: 0 = no unlisted failure
#       1 = at least one unlisted failure (a real regression)
#       2 = bad invocation, missing inputs, or unusable build tree
#
# Usage: run-tests.sh <build-dir> <label> [flaky-regex] [retries]
#   build-dir    a configured AND built ctest tree, e.g. build
#   label        allowlist section to judge against: release | asan
#   flaky-regex  ctest -R regex for the tests to retry before judging
#   retries      until-pass attempts for those tests (default 3)
set -uo pipefail

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <build-dir> <label> [flaky-regex] [retries]" >&2
    exit 2
fi

BUILD_DIR="$1"
LABEL="$2"
FLAKY="${3:-}"
RETRIES="${4:-3}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ALLOWLIST="$HERE/known-test-failures.txt"

[ -f "$ALLOWLIST" ] || { echo "FAIL: allowlist not found: $ALLOWLIST" >&2; exit 2; }
[ -d "$BUILD_DIR" ] || { echo "FAIL: build dir not found: $BUILD_DIR" >&2; exit 2; }
[ -f "$BUILD_DIR/CTestTestfile.cmake" ] || {
    echo "FAIL: $BUILD_DIR is not a CMake test tree." >&2
    echo "      Configure and build it first: cmake -S . -B $BUILD_DIR" >&2
    exit 2
}
case "$RETRIES" in
    ''|*[!0-9]*) echo "FAIL: retries must be a positive integer, got '$RETRIES'" >&2; exit 2 ;;
    0) echo "FAIL: retries must be >= 1 (0 would silently drop the flaky tests)" >&2; exit 2 ;;
esac

command -v ctest >/dev/null 2>&1 || { echo "FAIL: ctest not on PATH" >&2; exit 2; }

LOG="$(mktemp)"
CENSUS="$(mktemp)"
trap 'rm -f "$LOG" "$CENSUS"' EXIT

note() { printf '%s\n' "$*"; }

# True when <test> is listed under <label> in the allowlist.
is_listed() {
    awk -v label="$LABEL" -v want="$1" '
        /^[[:space:]]*#/ { next }
        /^[[:space:]]*$/ { next }
        $1 == label && $2 == want { found = 1 }
        END { exit(found ? 0 : 1) }
    ' "$ALLOWLIST"
}

# The reason recorded for <test>, or "" if unlisted.
reason_for() {
    awk -v label="$LABEL" -v want="$1" '
        /^[[:space:]]*#/ { next }
        /^[[:space:]]*$/ { next }
        $1 == label && $2 == want {
            line = $0
            sub(/^[^ ]+[ ]+[^ ]+[ ]+/, "", line)
            print line
            exit
        }
    ' "$ALLOWLIST"
}

# Test names from ctest's "The following tests FAILED:" block. Scoped to that
# block on purpose: with --output-on-failure the test's own stdout is in the
# same file, and a test that prints "12 - something" must not be mistaken for
# a ctest summary line.
failed_names() {
    awk '
        /^The following tests FAILED:/ { inblock = 1; next }
        inblock && $1 ~ /^[0-9]+$/ && $2 == "-" && NF >= 3 { print $3; next }
        inblock && $0 ~ /^Errors while running CTest/ { inblock = 0 }
    ' "$1"
}

# Collect a pass's failures into the census and refuse to continue if ctest
# reported failure but nothing could be parsed out of it. Reporting PASS on a
# parse failure would be the worst possible bug in this script: it would turn
# the gate into the exact no-op it was written to replace.
collect() {
    local log="$1" what="$2" rc="$3" before after
    before=$(wc -l < "$CENSUS")
    failed_names "$log" >> "$CENSUS"
    after=$(wc -l < "$CENSUS")
    if [ "$rc" -ne 0 ] && [ "$after" -eq "$before" ]; then
        echo "FAIL: ctest reported failure on the $what pass (exit $rc) but no" >&2
        echo "      'The following tests FAILED:' block could be parsed from its" >&2
        echo "      output. This script will not guess; refusing to report PASS." >&2
        echo "      ctest output was:" >&2
        sed -n '1,40p' "$log" >&2
        exit 2
    fi
}

: > "$CENSUS"

# ── pass 1: the flaky subset, retried ────────────────────────────────────────
if [ -n "$FLAKY" ]; then
    note "── flaky subset (retried up to $RETRIES attempts): $FLAKY"
    ctest --test-dir "$BUILD_DIR" -R "$FLAKY" \
          --repeat "until-pass:$RETRIES" \
          --output-on-failure --timeout 900 > "$LOG" 2>&1
    rc=$?
    collect "$LOG" "flaky subset" "$rc"
    note "   flaky pass exit=$rc"
else
    note "── no flaky subset declared for this job"
fi

# ── pass 2: everything else ─────────────────────────────────────────────────
if [ -n "$FLAKY" ]; then
    note "── remaining suite"
    ctest --test-dir "$BUILD_DIR" -E "$FLAKY" \
          --output-on-failure --timeout 900 > "$LOG" 2>&1
else
    note "── full suite"
    ctest --test-dir "$BUILD_DIR" \
          --output-on-failure --timeout 900 > "$LOG" 2>&1
fi
rc=$?
collect "$LOG" "main" "$rc"
note "   suite pass exit=$rc"

# ── judge ───────────────────────────────────────────────────────────────────

UNLISTED="$(mktemp)"
trap 'rm -f "$LOG" "$CENSUS" "$UNLISTED"' EXIT
: > "$UNLISTED"

unlisted=0
listed=0

if [ -s "$CENSUS" ]; then
    note ""
    note "── failures observed"
    while IFS= read -r t; do
        [ -n "$t" ] || continue
        if is_listed "$t"; then
            listed=$((listed + 1))
            r="$(reason_for "$t")"
            note "  KNOWN   $t"
            note "            reason: $r"
        else
            unlisted=$((unlisted + 1))
            note "$t" >> "$UNLISTED"
            note "  NEW     $t  <-- not in $ALLOWLIST"
        fi
    done < "$CENSUS"
else
    note ""
    note "── no test failed in this pass set"
fi

# A listed test that did not fail is a stale entry, not a problem -- but it is
# reported so the allowlist cannot quietly accumulate dead weight. Entries that
# match the flaky regex are exempt: a retried flaky test is EXPECTED to pass,
# so its absence from the failure list is not evidence the entry is stale.
if [ -s "$CENSUS" ]; then
    while IFS= read -r t; do
        [ -n "$t" ] || continue
        is_listed "$t" || continue
        if [ -n "$FLAKY" ] && printf '%s\n' "$t" | grep -qE "$FLAKY"; then
            continue
        fi
        if ! grep -qxF "$t" "$CENSUS"; then
            note "  STALE   $t is listed under '$LABEL' but did not fail; delete the entry"
        fi
    done < <(awk -v label="$LABEL" '$1 == label && $2 != "" { print $2 }' "$ALLOWLIST")
fi

note ""
note "── verdict ($LABEL)"
note "   tolerated known failures : $listed"
note "   unlisted failures        : $unlisted"

if [ "$unlisted" -gt 0 ]; then
    # An unlisted failure must be reproducible, and reproducing starts from
    # what the test actually printed. The census alone does not carry that:
    # report the failure sections of the ctest log too. The verdict and the
    # error annotations below are unchanged by this.
    note ""
    note "── output of the failing tests (from ctest --output-on-failure)"
    while IFS= read -r t; do
        [ -n "$t" ] || continue
        note ""
        note "── $t"
        # ctest --output-on-failure emits, per test: "    Start N: <name>"
        # then immediately the result line "<k>/<m> Test #N: <name> ...*Failed"
        # and the test's stdout, up to the next Start line (or the end of the
        # summary). The Start line is the exact test-name anchor; the awk
        # below prints the block from a target Start until the next one.
        awk -v t="$t" '
            index($0, "    Start ") == 1 {
                name = substr($0, 11)      # drop the four-space "    Start " prefix
                sub(/^[0-9]+: /, "", name) # drop "N: "
                in_target = (name == t)
            }
            in_target { print }
        ' "$LOG" | tail -n 200
        # The summary line carries the failure class (Failed / Not Run /
        # Timeout / Required regular expression not found). The census prints
        # names only, so surface the class next to the block above.
        grep -E "^[[:space:]]*[0-9]+ - $t \(" "$LOG" || true
    done < "$UNLISTED"
    while IFS= read -r t; do
        [ -n "$t" ] || continue
        note "::error title=Unlisted test failure::'$t' failed but is not in .github/ci/known-test-failures.txt under label '$LABEL'. Reproduce it; if it is pre-existing, add a line there with the reason and the tracking issue. Do not silence the CI step instead."
    done < "$UNLISTED"
    note "::error title=Regression::$unlisted test(s) failed that are not on the known-failure list."
    exit 1
fi

if [ "$listed" -gt 0 ]; then
    while IFS= read -r t; do
        [ -n "$t" ] || continue
        is_listed "$t" || continue
        note "::warning title=Known failure::$t -- $(reason_for "$t")"
    done < "$CENSUS"
    note "::warning title=Suite not green::$listed test(s) failed for recorded, pre-existing reasons. See .github/ci/known-test-failures.txt."
fi

note ""
note "PASS: no unlisted test failure (label=$LABEL)"
exit 0
