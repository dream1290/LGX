#!/usr/bin/env bash
# Reports the honest test census.
#
# The project advertised "76/76 passing" while roughly a third of those tests
# could not fail. This script separates the counts that were being conflated
# into one number:
#
#   registered - tests ctest knows about
#   verifying  - registered tests whose source has a failure mechanism
#   incapable  - registered tests that cannot report failure
#   unmapped   - registered tests whose source file could not be identified
#                (counted separately; NEVER folded into verifying)
#
# An unmapped test is not a verifying test. Dropping it quietly inflates
# `verifying`, which is exactly the failure mode this project keeps committing,
# so it is reported explicitly and by name.
#
# Detection is NOT reimplemented here: each mapped source is passed to
# scripts/verify_test_integrity.sh --classify, so the gate and the census can
# never disagree about what "capable of failing" means.
#
# ---------------------------------------------------------------------------
# HOW A CTEST TARGET IS TIED TO A SOURCE FILE
# ---------------------------------------------------------------------------
# This script does NOT guess from names. An earlier version did, and it was
# wrong in a way nothing caught: it peeled arbitrary leading underscore-words
# off a target name until the remainder matched some file's basename. So a
# target called `basic_test_init_shutdown` peeled to `test_init_shutdown` and
# was reported as a verifying test, sourced from tests/unit/test_init_shutdown.c
# -- a file it has nothing to do with. Same for
# `shutdown_basic_test_init_shutdown`. Those wrong mappings were invisible:
# they produced a plausible `verifying` and no complaint. Name similarity is not
# evidence that two files are related, so there is no name matching here at all.
#
# The mapping is read from CMake's own generated build metadata, in this order:
#
#   1. CMakeFiles/<exe>.dir/DependInfo.cmake   (Unix Makefiles generator)
#      `set(CMAKE_DEPENDS_DEPENDENCY_FILES ...)` lists the exact sources.
#   2. CMakeFiles/<exe>.dir/build.make        (same generator, fallback)
#   3. build.ninja                            (Ninja generator)
#      the `<exe>.dir/<name>.c.o` build edge names the source it compiles.
#      Note the object rule path may carry a subdirectory prefix
#      (`tests/unit/CMakeFiles/<exe>.dir/...`), so it is matched anywhere in
#      the path, not only at the start.
#
# `name -> executable` comes from parsing `add_test(<name> "<abs-exe>")` out of
# the generated CTestTestfile.cmake files, which is what CMake itself writes
# there. Plain CTestTestfile.cmake parsing is used in preference to
# `ctest --show-only=json-v1` deliberately: the JSON output is pretty-printed
# with incidental line breaks and nested "name"/"command" keys, and its exact
# shape has moved between CMake versions, so parsing it in awk means carrying a
# fragile JSON reader. The CTestTestfile.cmake form is one line per test in a
# layout CMake has emitted for many years, and it is what a reader can verify by
# eye. ctest is still consulted for the authoritative test COUNT, and a
# disagreement between the two fails loudly rather than being resolved silently.
#
# The per-target metadata is written when the tree is CONFIGURED, not when it
# is built, so a configured-but-unbuilt tree is enough to take a census. Only an
# unconfigured (or foreign) build dir is rejected, and it is rejected with a
# message that says so rather than blaming a missing build.
#
# If a target has several sources, ALL of them are classified and the target is
# counted as incapable only if every source is incapable. Multi-source targets
# are listed explicitly.
#
# ---------------------------------------------------------------------------
# Usage: scripts/count_tests.sh [build-dir]    (default build dir: build)
#        scripts/count_tests.sh --help
#
# Exit:
#   0  census complete, every registered test determined
#   2  usage error, or the build dir cannot be used
#   3  census produced, but one or more targets could NOT be determined
#      (unmapped > 0). This is a FAILURE SIGNAL, not a warning: an undetermined
#      test is neither verifying nor incapable, so the census does not account
#      for the whole suite. Set LGX_ALLOW_UNMAPPED=1 to downgrade it while
#      migrating naming, never in CI.
# ===========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLASSIFY="$REPO_ROOT/scripts/verify_test_integrity.sh"
TRACKING_CHECK="$REPO_ROOT/scripts/check_gate_scripts_tracked.sh"

case "${1:-}" in
    -h|--help) awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"; exit 0 ;;
esac

if [ "$#" -gt 1 ]; then
    echo "usage: $0 [build-dir]" >&2
    echo "       $0 accepts at most one argument; got $#: $*" >&2
    exit 2
fi

if [ ! -f "$CLASSIFY" ]; then
    echo "FAIL: $CLASSIFY not found." >&2
    exit 2
fi

# The gate scripts must be tracked by git, or CI cannot run them at all and a
# later wholesale .gitignore rewrite (as planned for task 5) would silently
# un-track them. See scripts/check_gate_scripts_tracked.sh.
if [ -f "$TRACKING_CHECK" ]; then
    "$TRACKING_CHECK" || exit 2
fi

BUILD_DIR="${1:-build}"

# Resolve the build dir against the caller's cwd, then against the repo root,
# so this behaves the same from anywhere without ever depending on cwd.
if [ ! -d "$BUILD_DIR" ]; then
    if [ -d "$REPO_ROOT/$BUILD_DIR" ]; then BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
    else
        echo "No CTest build directory at '$BUILD_DIR'." >&2
        echo "Configure one first: cmake -S $REPO_ROOT -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release" >&2
        echo "then: cmake --build $BUILD_DIR" >&2
        exit 2
    fi
fi

command -v ctest >/dev/null 2>&1 || { echo "FAIL: ctest not found on PATH." >&2; exit 2; }

# ---------------------------------------------------------------------------
# A build directory configured from somewhere else would tie these tests to
# sources that are not the ones in this tree. Say so rather than reporting a
# confident and meaningless census.
# ---------------------------------------------------------------------------
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cache_src="$(sed -nE 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$BUILD_DIR/CMakeCache.txt")"
    if [ -n "$cache_src" ] && [ "$cache_src" != "$REPO_ROOT" ]; then
        echo "FAIL: build dir '$BUILD_DIR' was configured from '$cache_src'," >&2
        echo "      not from '$REPO_ROOT'. A census against it would describe" >&2
        echo "      a different tree." >&2
        exit 2
    fi
fi

# ---------------------------------------------------------------------------
# Test names and the executable each one runs.
#
# Read from the CTestTestfile.cmake files CMake generates, each of which states
# `add_test(<name> "<abs-exe-path>")`. That is a direct, machine-readable
# statement of what ctest runs: no JSON to parse, no inference, and identical
# under every generator.
# ---------------------------------------------------------------------------
list_tests() {
    # CMake's generated files use the `add_test(<name> "<exe>")` spelling.
    { grep -rh '^[[:space:]]*add_test([^N]' "$BUILD_DIR" --include=CTestTestfile.cmake 2>/dev/null || true; } \
        | sed -nE 's/^[[:space:]]*add_test\(([A-Za-z0-9_.-]+)[[:space:]]+"([^"]+)".*/\1\t\2/p'
    # Also accept `add_test(NAME <name> COMMAND <exe>)`, in case a build tree uses it.
    { grep -rh '^[[:space:]]*add_test([[:space:]]*NAME[[:space:]]' "$BUILD_DIR" --include=CTestTestfile.cmake 2>/dev/null || true; } \
        | sed -nE 's/^[[:space:]]*add_test\([[:space:]]*NAME[[:space:]]+([A-Za-z0-9_.-]+)[[:space:]]+COMMAND[[:space:]]+([^[:space:]]+).*/\1\t\2/p'
}

NAMES_EXES="$(list_tests | grep -v '^$' || true)"

if [ -z "$NAMES_EXES" ]; then
    echo "FAIL: no add_test() entries found under '$BUILD_DIR'." >&2
    echo "      Configure and build first:" >&2
    echo "        cmake -S . -B $BUILD_DIR && cmake --build $BUILD_DIR" >&2
    exit 2
fi

# ctest is the authority on how many tests there are. If the build tree's
# CTestTestfile.cmake files disagree with ctest, one of the two is stale and a
# census built from the wrong one is worse than no census at all.
ctest_count="$(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null \
    | grep -cE '^[[:space:]]*Test[[:space:]]+#[0-9]+:' || true)"
parsed_count="$(printf '%s\n' "$NAMES_EXES" | grep -c . || true)"
if [ "$ctest_count" != "$parsed_count" ]; then
    echo "FAIL: ctest reports $ctest_count tests but $parsed_count were found in" >&2
    echo "      $BUILD_DIR's CTestTestfile.cmake files. The build tree is stale;" >&2
    echo "      re-run cmake before taking a census." >&2
    exit 2
fi

# ---------------------------------------------------------------------------
# Index: executable basename -> the sources CMake compiled into it.
# Built in one pass over the build tree rather than one `find` per test.
# ---------------------------------------------------------------------------
INDEX="$(mktemp)"
UNMAPPED_FILE="$(mktemp)"
MULTI_FILE="$(mktemp)"
trap 'rm -f "$INDEX" "$UNMAPPED_FILE" "$MULTI_FILE"' EXIT

build_index() {
    local f dir target srcs
    # 1 & 2: per-target CMakeFiles/<exe>.dir/{DependInfo.cmake,build.make}
    while IFS= read -r f; do
        dir="$(dirname "$f")"
        target="$(basename "$dir" .dir)"
        if [ "${f##*/}" = "DependInfo.cmake" ]; then
            srcs="$(sed -nE '/^set\(CMAKE_DEPENDS_DEPENDENCY_FILES/,/^[ \t]*\)/p' "$f" \
                    | { grep -oE '"[^"]+\.(c|cc|cpp)"' || true; } | tr -d '"')"
        else
            srcs="$({ grep -oE '[^ ]+\.(c|cc|cpp)\b' "$f" || true; } | sort -u)"
        fi
        [ -n "$srcs" ] || continue
        while IFS= read -r src; do
            [ -f "$src" ] || continue
            printf '%s\t%s\n' "$target" "${src#$REPO_ROOT/}" >> "$INDEX"
        done <<< "$srcs"
    done < <(find "$BUILD_DIR" -path '*.dir/DependInfo.cmake' -o -path '*.dir/build.make' 2>/dev/null)

    # 3: Ninja has neither of the above, so the source is read off the build
    #    edge itself. The previous version registered the target on the `build`
    #    line and then `next`ed past it -- but the source path is ON that line,
    #    so it extracted nothing, and the indented variable lines it did look at
    #    belonged to whatever rule happened to be last. On a fully built Ninja
    #    tree that produced an empty index and the misleading "no CMake build
    #    metadata found, run the build first" message for a tree that had been
    #    built. Both bugs are fixed here: the source is taken from the build
    #    line, and variable lines are ignored entirely.
    #
    # Pass 1 collects `NAME = value` definitions so `${NAME}` in a source token
    # can be expanded. A token that still contains `$` after that is DROPPED,
    # not guessed: an unresolved path would be a wrong mapping, and the target
    # then lands in UNMAPPED, which is loud.
    if [ -f "$BUILD_DIR/build.ninja" ]; then
        awk -v builddir="$BUILD_DIR" '
            NR == FNR {
                if ($0 ~ /^[ 	]*[A-Za-z_][A-Za-z0-9_]*[ 	]*=[ 	]/) {
                    k = $0; sub(/^[ 	]*/, "", k); sub(/[ 	]*=.*$/, "", k)
                    v = $0; sub(/^[^=]*=[ 	]*/, "", v)
                    if (k != "") var[k] = v
                }
                next
            }
            /^build / {
                out = $2
                sub(/:$/, "", out)
                if (out !~ /CMakeFiles\/[^/]+\.dir\//) next
                t = out
                sub(/^.*CMakeFiles\//, "", t)
                sub(/\.dir\/.*$/, "", t)
                for (i = 4; i <= NF; i++) {
                    if ($i == "||") break
                    s = $i
                    if (s !~ /\.(c|cc|cpp)$/) continue
                    for (k in var) { gsub("\\$\\{" k "\\}", var[k], s); gsub("\\$" k "\\>", var[k], s) }
                    if (s ~ /\$/) continue          # unresolved: drop, do not guess
                    print t "\t" s
                }
            }
        ' "$BUILD_DIR/build.ninja" "$BUILD_DIR/build.ninja" 2>/dev/null \
        | while IFS=$'\t' read -r ntarget nsrc; do
            [ -n "$ntarget" ] && [ -n "$nsrc" ] || continue
            case "$nsrc" in /*) nabs="$nsrc" ;; *) nabs="$BUILD_DIR/$nsrc" ;; esac
            [ -f "$nabs" ] || continue
            case "$nabs" in
                "$REPO_ROOT"/*) printf '%s\t%s\n' "$ntarget" "${nabs#$REPO_ROOT/}" >> "$INDEX" ;;
            esac
        done || true
    fi
    sort -u -o "$INDEX" "$INDEX"
}

build_index

if [ ! -s "$INDEX" ]; then
    echo "FAIL: no CMake build metadata found under '$BUILD_DIR'." >&2
    echo "      This script ties each ctest target to its source using the files" >&2
    echo "      CMake writes when a tree is configured. This one has none, so it" >&2
    echo "      was never configured (or was wiped). Configure it:" >&2
    echo "        cmake -S . -B $BUILD_DIR" >&2
    echo "      Building is NOT required for a census -- configuring is enough." >&2
    echo "      No test is guessed at when this metadata is missing." >&2
    exit 2
fi

# ---------------------------------------------------------------------------
# Census.
# ---------------------------------------------------------------------------
registered=0
verifying=0
incapable=0
unmapped=0

while IFS=$'\t' read -r name exe; do
    [ -n "$name" ] || continue
    registered=$((registered + 1))

    if [ -z "$exe" ]; then
        unmapped=$((unmapped + 1))
        printf '%s\tctest did not report an executable for this test\n' "$name" >> "$UNMAPPED_FILE"
        continue
    fi

    exe_base="$(basename "$exe")"
    mapfile -t srcs < <(awk -F'\t' -v t="$exe_base" '$1 == t {print $2}' "$INDEX" | sort -u)

    if [ "${#srcs[@]}" -eq 0 ]; then
        unmapped=$((unmapped + 1))
        printf '%s\texecutable %s has no recorded source in the build tree\n' \
               "$name" "$exe_base" >> "$UNMAPPED_FILE"
        continue
    fi

    if [ "${#srcs[@]}" -gt 1 ]; then
        printf '%s\t%s\n' "$name" "${srcs[*]}" >> "$MULTI_FILE"
    fi

    # A target is capable of failing if ANY of its sources is. It is incapable
    # only when none of them is.
    capable=0
    undetermined=0
    for s in "${srcs[@]}"; do
        v="$("$CLASSIFY" --classify "$REPO_ROOT/$s" 2>/dev/null | cut -f1 || true)"
        case "$v" in
            verifying) capable=1 ;;
            incapable) ;;
            *)         undetermined=1
                         printf '%s\tclassifier error on %s\n' "$name" "$s" >> "$UNMAPPED_FILE" ;;
        esac
    done
    # "not determined" is never rounded up to "verifying": that is precisely the
    # silent inflation this script exists to prevent.
    if   [ "$undetermined" -eq 1 ]; then unmapped=$((unmapped + 1))
    elif [ "$capable" -eq 1 ];       then verifying=$((verifying + 1))
    else                                   incapable=$((incapable + 1))
    fi
done <<< "$NAMES_EXES"

echo "registered=$registered"
echo "verifying=$verifying"
echo "incapable=$incapable"
echo "unmapped=$unmapped"

if [ -s "$MULTI_FILE" ]; then
    echo ""
    echo "MULTI-SOURCE: these targets compile more than one source. Each is counted"
    echo "              capable if any source is, incapable only if none is."
    sed 's/^/  - /' "$MULTI_FILE"
fi

if [ "$unmapped" -gt 0 ]; then
    echo ""
    echo "UNMAPPED: $unmapped registered test(s) could not be tied to a source file."
    echo "          They are NOT counted as verifying. Re-running cmake will"
    echo "          refresh the mapping if the build tree is stale; otherwise the"
    echo "          test's executable has no source in the tree, which usually"
    echo "          means the target is a wrapper, a script, or a renamed test."
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

# An undetermined test is a hole in the census, so this is a hard failure by
# default. The alternative -- reporting a confident number that silently omits
# tests -- is the failure mode this script was written to end.
if [ "$unmapped" -gt 0 ] && [ "${LGX_ALLOW_UNMAPPED:-0}" != "1" ]; then
    exit 3
fi
