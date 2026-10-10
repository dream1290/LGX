#!/usr/bin/env bash
# Fails if any test file is structurally incapable of reporting failure.
#
# Background: an audit on 2026-10-10 found that a large share of the test files
# in this repository contain no non-zero exit path and no live failure counter.
# Such a test passes unconditionally, so it contributes nothing to the pass
# rate while inflating it. A test that cannot fail is a defect, not a passing
# test. This script makes that condition visible and blocks new instances; it
# does not repair the existing ones (see docs plan 03).
#
# ===========================================================================
# DETECTION RULES
# ===========================================================================
# The source is first stripped of comments and string/character literals by an
# awk state machine (strip()), so `return 1;` inside a comment, or the text
# "return 1" that a test prints, does NOT count as a failure mechanism.
#
# Only main()'s own exit path is credited for `return` statements. This matters
# more than it sounds: tests/phase0/test_frame_arena.c is full of `return
# false;` inside static bool helpers, and a naive "any non-zero return exists"
# rule would happily certify a test whose main() provably cannot fail.
#
# A file is judged CAPABLE (credited) if ANY of the following holds.
#
#   M1  Process-terminating failure, anywhere in the file:
#         exit(1) / _exit(1) / quick_exit(1) / EXIT_FAILURE / EXIT_FAIL
#         abort() / _Exit() / __builtin_trap() / raise(SIGABRT)
#       exit(0) does NOT count. Credit is deliberately file-wide rather than
#       main-scoped: these terminate the process from whatever call path
#       reaches them, so proving reachability from main() is not this
#       script's job (limitation L2).
#
#   M2  main() returns a bare non-zero literal:
#         return 1;   return -1;   return EXIT_FAILURE;
#
#   M3  main()'s verdict expression depends on an identifier that is DRIVEN
#       (see M4), e.g. all of these:
#           int rc = check();  if (rc) return rc;
#           return failures ? 1 : 0;
#           return (tests_failed == 0) ? 0 : 1;
#       Identifiers that are function calls, C keywords, ALL_CAPS constants
#       and `.member` / `->member` names are not treated as verdicts.
#
#       Note that `return run_suite();` is therefore NOT credited: a call is not
#       an identifier, so the statement carries no verdict this checker can read
#       and the file is reported ALWAYS_ZERO. That is the fail-safe direction
#       (capable reported as incapable, not the reverse), but it means a test
#       that delegates its whole verdict to a function must still expose one:
#
#           int rc = run_suite();   /* rc is driven by the assignment -> M3 */
#           return rc;
#
#       Verified: a file whose only exit is `return run_suite();` is reported
#       incapable [ALWAYS_ZERO], whether or not run_suite() can return non-zero.
#
#   M4  Every identifier carrying main()'s verdict is DRIVEN. An identifier is
#       driven if the file increments it (v++, v--) or assigns it something
#       other than a zero constant (v = check()).
#
# A DEAD COUNTER -- read by the verdict but only ever initialised, e.g.
# `static int failures = 0;` with no `failures++` anywhere -- is what makes a
# verdict a compile-time constant. That is the most insidious form of rot: a
# test that prints "0 failed" and returns 0 while having no way to ever return
# anything else. Such a file is reported as INCAPABLE, naming the dead
# identifier.
#
#   M5  A macro DEFINED IN THIS FILE whose body returns non-zero is invoked from
#       main(), e.g. `#define CHECK(c) do { if (!(c)) return 1; } while (0)`.
#       This is macro-expansion-lite: the body is read directly, arguments are
#       never substituted.
#
# assert() is deliberately NOT credited as a failure mechanism.
# CMakeLists.txt:9-10 defaults CMAKE_BUILD_TYPE to Release, and
# CMakeLists.txt:18-19 add -DNDEBUG to CMAKE_C_FLAGS_RELEASE and
# CMAKE_C_FLAGS_RELWITHDEBINFO. Under NDEBUG, assert() expands to ((void)0),
# so an assert()-only test exits 0 in every configuration this project
# actually ships. This is not hypothetical: it is exactly how
# tests/phase0/test_csf2_numa_awareness.c was broken until commit ca172c2.
# Files whose only apparent mechanism is assert() are reported with an explicit
# ASSERT_ONLY diagnosis so they are not mistaken for covered tests.
#
# Before any of this the source is normalised two ways:
#   * backslash continuations are folded into logical lines, so a multi-line
#     #define (or a wrapped `return` expression) is seen whole;
#   * main()'s body is split into statements on `;`, so `if (x) return 1;
#     return 0;` is two verdicts rather than one.
#
# ===========================================================================
# KNOWN LIMITATIONS -- this is a text heuristic, not a compiler
# ===========================================================================
#   L1  A verdict of the form `return some_function();` is NOT credited at all,
#       because the callee's return value cannot be read without dataflow
#       analysis. The file is reported incapable even if the function does
#       return non-zero. This errs toward flagging capable files, which is the
#       safe direction for a gate: it never lets an incapable file through.
#   L2  exit()/abort() is credited file-wide (M1) even when it sits in a helper
#       main() never calls. Verified against a scratch fixture:
#       scripts/verify_test_integrity.sh --classify classifies a file whose only
#       non-zero exit is in an unreached static helper as verifying.
#   L3  Brace matching counts braces in code inside `#if 0 ... #endif`, and
#       statement splitting on `;` also splits `for` headers. Neither loses a
#       verdict that exists, but both can move it to a neighbouring fragment.
#   L4  Conditional compilation is not evaluated, so a `return 1` that lives
#       only inside a `#if 0` branch is still credited (false negative).
#   L5  A file whose main() is declared unusually (K&R style, or defined via a
#       macro) is reported NO_MAIN, which counts as incapable.
#   L6  Failure macros that come from a HEADER rather than from the test file
#       itself are not seen by M5. Object-like macros (used without
#       parentheses) are not seen either. No such macro exists in the suite
#       today; adding one means teaching this script about it.
#   L7  A verdict reached through a framework that owns the exit code
#       (g_test_run(), a RUN_TESTS() macro from a shared header) is not
#       recognised unless the non-zero exit is visible in this file. No such
#       framework is in use today.
#   L8  This gate answers "could this file ever exit non-zero?", not "does it
#       assert enough?". A file credited here may still assert almost nothing
#       -- a test that can fail but checks one trivial condition passes this
#       gate. Both questions matter; only the first is mechanically checkable.
#   L9  Classification is textual, so it is a floor on coverage, not a proof.
#       `checked`/`verifying`/`incapable` are honest lower bounds: "incapable"
#       is a strong claim (the file provably has no non-zero exit path), but
#       "verifying" only means "a non-zero exit path exists", never "the test
#       is adequate".
#
# ===========================================================================
# Usage: scripts/verify_test_integrity.sh [file ...]
#        no arguments  -> scan every .c/.cc/.cpp under tests/
#        --classify F -> machine-readable one-line verdict for file F;
#                        used by scripts/count_tests.sh so that the two
#                        scripts can never disagree about the definitions.
#
# Exit: 0 = every checked file can report failure, 1 = at least one cannot.
# ===========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"

# Both gate scripts must stay tracked by git, or CI silently stops running
# them. See scripts/check_gate_scripts_tracked.sh for why this is checked at
# run time rather than left as a comment in .gitignore.
if [ "${LGX_SKIP_TRACKING_CHECK:-0}" != "1" ] && [ -f "$REPO_ROOT/scripts/check_gate_scripts_tracked.sh" ]; then
    "$REPO_ROOT/scripts/check_gate_scripts_tracked.sh" "$REPO_ROOT" || exit 2
fi
trap 'rm -rf "$WORK"' EXIT

# ---------------------------------------------------------------------------
# The analyser, in one awk program so both scripts share a single definition of
# "capable of failing".
# ---------------------------------------------------------------------------
cat > "$WORK/analyse.awk" <<'AWK_EOF'
function strip(s,   i, n, c, out) {
    n = length(s); i = 1; out = ""
    while (i <= n) {
        c = substr(s, i, 1)
        if (blk) {                                   # inside a /* */ block
            if (c == "*" && substr(s, i + 1, 1) == "/") { blk = 0; i += 2 } else { i++ }
            continue
        }
        if (c == "/" && substr(s, i + 1, 1) == "*") {
            i += 2; blk = 1
        } else if (c == "/" && substr(s, i + 1, 1) == "/") {
            while (i <= n && substr(s, i, 1) != "\n") i++
        } else if (c == "\"") {
            i++
            while (i <= n) {
                if (substr(s, i, 1) == "\\") { i += 2; continue }
                if (substr(s, i, 1) == "\"") { i++; break }
                i++
            }
            out = out " STR "
        } else if (c == "'") {
            q = ""; i++
            while (i <= n && substr(s, i, 1) != "'" && substr(s, i, 1) != "\n") { q = q substr(s, i, 1); i++ }
            if (substr(s, i, 1) == "'") { i++; out = out " 0 " } else { out = out "'" q }
        } else {
            out = out c; i++
        }
    }
    return out
}

function is_keyword(t) {
    return (t == "return" || t == "if" || t == "else" || t == "while" || t == "for" ||
            t == "do" || t == "switch" || t == "case" || t == "default" || t == "sizeof" ||
            t == "break" || t == "continue" || t == "goto" || t == "true" || t == "false" ||
            t == "NULL" || t == "EXIT_SUCCESS" || t == "std" || t == "main")
}

# Identifiers in an expression that could carry the verdict. Skips function
# calls, keywords, ALL_CAPS constants and member accesses.
function collect_idents(expr,   rest, id, after, before, k) {
    n_id = 0
    rest = expr
    while (match(rest, /[A-Za-z_][A-Za-z0-9_]*/)) {
        id  = substr(rest, RSTART, RLENGTH)
        after  = substr(rest, RSTART + RLENGTH, 1)
        before = ""
        for (k = RSTART - 1; k >= 1; k--) {
            c = substr(rest, k, 1)
            if (c != " " && c != "\t") { before = c; break }
        }
        if (before == "-" && substr(rest, k + 1, 1) == ">") before = "."
        if (after != "(" && before != "." && before != ">" &&
            !is_keyword(id) && id !~ /^[A-Z][A-Z0-9_]*$/)
            ids[++n_id] = id
        rest = substr(rest, RSTART + RLENGTH)
    }
    return n_id
}

# Is `v` ever incremented, or assigned something other than a zero constant?
function is_driven(v,   i, t, re, p, op, nx, rhs, pre) {
    re = "[^A-Za-z0-9_]" v "[ \t]*"
    for (i = 1; i <= nlines; i++) {
        t = code[i]
        while (match(t, re)) {
            p  = RSTART + RLENGTH - 1
            pre = substr(t, RSTART, 2)
            if (pre == "++" || pre == "--") return 1        # ++v / --v
            op = substr(t, p + 1, 2)
            if (op == "++" || op == "--") return 1          # v++ / v--
            if (index("+-*/%&|^", substr(op, 1, 1)) > 0 && substr(op, 2, 1) == "=") return 1
            if (substr(op, 1, 1) == "=") {
                nx = substr(t, p + 2, 1)
                if (nx == "=" || nx == "&") { t = substr(t, p + 2); continue }
                rhs = substr(t, p + 2)
                if (rhs ~ /^[ \t]*(0[0LlUu]*|false|NULL)[ \t]*([;,)\]]|$)/) { t = substr(t, p + 2); continue }
                return 1
            }
            t = substr(t, p + 1)
        }
    }
    return 0
}

function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t]+$/, "", s); return s }

function analyse(   i, j, k, ln, d, t, nb, nc, expr, arg, verdicts, nfound, nonzero, buf, m, nm, q) {
    nlines = 0; n_id = 0; dead = ""; rule = ""; has_assert = 0; mainline = 0; blk = 0
    for (i = 1; i <= n_id_max; i++) delete ids[i]
    n_id_max = 0

    # Fold backslash continuations into logical lines first. Without this a
    # multi-line macro whose FIRST line is a #define is skipped for brace
    # counting while its continuation lines are not, so the running depth goes
    # negative and main()'s body can no longer be located.
    buf = ""
    for (i = 1; i <= n_all; i++) {
        ln = raw[i]
        if (ln ~ /\\[ \t]*$/) {
            sub(/\\[ \t]*$/, "", ln)
            buf = buf " " ln
        } else {
            buf = (buf == "") ? ln : buf " " ln
            nlines++
            code[nlines] = strip(buf)
            is_pp[nlines] = (code[nlines] ~ /^[ \t]*#/) ? 1 : 0
            buf = ""
        }
    }
    if (buf != "") {
        nlines++
        code[nlines] = strip(buf)
        is_pp[nlines] = (code[nlines] ~ /^[ \t]*#/) ? 1 : 0
    }

    d = 0
    for (i = 1; i <= nlines; i++) {
        dpre[i] = d
        if (code[i] ~ /[^A-Za-z0-9_]assert[ \t]*\(/) has_assert = 1
        if (is_pp[i]) { dpost[i] = d; continue }
        t = code[i]; nb = gsub(/\{/, "", t)
        t = code[i]; nc = gsub(/\}/, "", t)
        d = d + nb - nc
        dpost[i] = d
    }

    for (i = 1; i <= nlines; i++) {
        if (code[i] ~ /(^|[^A-Za-z0-9_])main[ \t]*\(/ &&
            code[i] !~ /main[ \t]*\([^)]*\)[ \t]*[;,]/) { mainline = i; break }
    }
    if (mainline == 0) { rule = "NO_MAIN"; return }

    body_lo = 0
    for (j = mainline; j <= nlines; j++)
        if (code[j] ~ /\{/ && dpre[j] == 0) { body_lo = j; break }
    if (body_lo == 0) { rule = "NO_MAIN"; return }
    body_hi = body_lo
    if (dpost[body_lo] > 0)
        for (k = body_lo + 1; k <= nlines; k++)
            if (dpost[k] == 0) { body_hi = k; break }

    for (i = 1; i <= nlines; i++) {
        if (is_pp[i]) continue
        ln = code[i]
        if (ln ~ /[^A-Za-z0-9_](abort|_Exit)[ \t]*\(/ ||
            ln ~ /[^A-Za-z0-9_]__builtin_trap[ \t]*\(/ ||
            ln ~ /[^A-Za-z0-9_]raise[ \t]*\([ \t]*SIGABRT/) { rule = "M1-ABORT"; return }
        if (ln ~ /EXIT_FAILURE|EXIT_FAIL/) { rule = "M1-EXIT_FAILURE"; return }
        if (ln ~ /[^A-Za-z0-9_](_exit|quick_exit|exit)[ \t]*\(/) {
            arg = ln
            sub(/^.*[^A-Za-z0-9_](_exit|quick_exit|exit)[ \t]*\(/, "", arg)
            if (arg ~ /^[ \t]*0[0-9LlUu]*[ \t]*\)/) continue
            if (arg ~ /EXIT_FAIL/ || arg ~ /(^|[^0-9A-Za-z_])[1-9]/ || arg !~ /^[ \t]*0[0-9LlUu]*[ \t]*[;,)]/)
                { rule = "M1-EXIT"; return }
        }
    }

    # Split main()'s body into statements first. A verdict is one statement,
    # so `int main(void) { if (x) return 1; return 0; }` on a single line has to
    # be seen as TWO verdicts, not one -- a greedy "text after the last return"
    # would only ever look at `return 0;`.
    n_stmt = 0
    for (i = body_lo; i <= body_hi; i++) {
        if (is_pp[i]) continue
        ln = code[i]
        while (1) {
            q = index(ln, ";")
            if (q == 0) { if (trim(ln) != "") { n_stmt++; stmt[n_stmt] = ln; n_stmt_max = n_stmt } ; break }
            n_stmt++
            stmt[n_stmt] = substr(ln, 1, q)
            n_stmt_max = n_stmt
            ln = substr(ln, q + 1)
        }
    }

    verdicts = 0
    for (i = 1; i <= n_stmt; i++) {
        ln = stmt[i]
        if (ln !~ /(^|[^A-Za-z0-9_])(return|_exit|quick_exit|exit)[ \t]*\(?/) continue
        verdicts++
        expr = ln
        if (ln ~ /(^|[^A-Za-z0-9_])return[ \t]*[^;]*;/) sub(/^.*[^A-Za-z0-9_]return[ \t]*/, "", expr)
        else                                           sub(/^.*[^A-Za-z0-9_](_exit|quick_exit|exit)[ \t]*\(/, "", expr)
        sub(/;.*$/, "", expr)
        expr = trim(expr)
        if (expr == "" || expr == "0" || expr == "0L" || expr == "0U" ||
            expr == "(void)0" || expr == "NULL" || expr == "false") continue

        nfound  = collect_idents(expr)
        nonzero = (expr ~ /(^|[^0-9])[1-9]/ || expr ~ /EXIT_FAIL/)

        if (nfound == 0) {
            if (nonzero) { rule = "M2-NONZERO_LITERAL"; return }
            continue
        }
        if (n_id > n_id_max) n_id_max = n_id
        for (j = 1; j <= n_id; j++) if (!is_driven(ids[j])) dead = dead " " ids[j]
        if (dead != "") { rule = "M4-DEAD_COUNTER"; dead = trim(dead); return }
        rule = (nonzero ? "M3-COUNTER_VERDICT" : "M3-RETURNED_VALUE")
        return
    }
    if (verdicts == 0) { rule = "NO_VERDICT"; return }

    # M5: a macro DEFINED IN THIS FILE whose body returns non-zero, invoked
    # from main(). Line folding above puts a whole multi-line macro on one
    # logical line, so the body is directly visible. This is macro
    # expansion-lite, not expansion: it covers the common `CHECK(cond)` /
    # `TEST_ASSERT(cond, msg)` idiom without trying to substitute arguments.
    for (i = 1; i <= nlines; i++) {
        if (!is_pp[i]) continue
        m = code[i]
        if (m !~ /^[ \t]*#[ \t]*define[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\(/) continue
        nm = m
        sub(/^.*#[ \t]*define[ \t]+/, "", nm)
        sub(/[ \t(].*$/, "", nm)
        if (m !~ /(^|[^A-Za-z0-9_])(return|_exit|quick_exit|exit)[ \t]*\(?[^;]*(^|[^0-9])[1-9]/ &&
            m !~ /EXIT_FAIL/) continue
        for (j = body_lo; j <= body_hi; j++) {
            if (is_pp[j]) continue
            if (code[j] ~ ("(^|[^A-Za-z0-9_])" nm "[ \t]*\\(")) { rule = "M5-MACRO_RETURN"; return }
        }
    }

    if (has_assert) { rule = "ASSERT_ONLY"; return }
    rule = "ALWAYS_ZERO"
}

# One verdict line per input file. reset_per_file() matters: without it the
# raw/code/depth arrays (and the block-comment state) leak from one file into
# the next and the whole batch scan silently analyses garbage.
FNR == 1 { if (n_all > 0) flush() }
{ raw[++n_all] = $0 }
END { if (n_all > 0) flush() }

function flush(   i) {
    analyse()
    printf "%s\t%s\n", rule, dead
    for (i = 1; i <= n_all; i++) { delete raw[i]; delete code[i]; delete is_pp[i]; delete dpre[i]; delete dpost[i] }
    for (i = 1; i <= n_stmt_max; i++) delete stmt[i]
    n_stmt_max = 0; n_stmt = 0
    for (i = 1; i <= n_id_max; i++) delete ids[i]
    n_all = 0; n_id = 0; n_id_max = 0
}
AWK_EOF

# ---------------------------------------------------------------------------
# classify <file> -> "verifying|incapable<TAB>file<TAB>human readable reason"
# ---------------------------------------------------------------------------
classify() {
    local file="$1" rule dead
    if [ ! -f "$file" ]; then
        printf 'error\t%s\tfile does not exist\terror\n' "$file"
        return 2
    fi
    if ! out="$(awk -f "$WORK/analyse.awk" "$file" 2>/dev/null)"; then
        printf 'error\t%s\tanalyser failed\terror\n' "$file"
        return 2
    fi
    rule="${out%%$'\t'*}"
    dead="${out#*$'\t'}"

    case "$rule" in
        M1-EXIT)            reason="exit() with a non-zero status" ;;
        M1-EXIT_FAILURE)    reason="EXIT_FAILURE is reachable" ;;
        M1-ABORT)           reason="abort()/raise(SIGABRT)/__builtin_trap() present" ;;
        M2-NONZERO_LITERAL) reason="main() returns a non-zero literal" ;;
        M5-MACRO_RETURN)    reason="a macro defined here returns non-zero and is invoked from main()" ;;
        M3-COUNTER_VERDICT) reason="main() returns a live failure counter" ;;
        M3-RETURNED_VALUE)  reason="main() returns a computed value" ;;
        NO_VERDICT)         reason="main() has no return or exit statement, so it returns 0" ;;
        NO_MAIN)            reason="no main() found" ;;
        ASSERT_ONLY)        reason="assert() only -- compiled out under -DNDEBUG in every shipped build" ;;
        M4-DEAD_COUNTER)    reason="verdict reads dead counter(s): $dead (never incremented)" ;;
        ALWAYS_ZERO)        reason="main() can only ever return 0" ;;
        *)                  reason="unclassified ($rule)" ;;
    esac

    case "$rule" in
        M1-*|M2-*|M3-*|M5-*) printf 'verifying\t%s\t%s\t%s\n' "$file" "$reason" "$rule" ;;
        *)               printf 'incapable\t%s\t%s\t%s\n' "$file" "$reason" "$rule" ;;
    esac
}

# Print this file's comment header as help. Stops at the first non-comment
# line rather than using a line range, so the header can grow without silently
# truncating the limitations section -- which is the part that matters.
print_header() {
    awk 'NR == 1 { next }
         /^#/    { sub(/^# ?/, ""); print; next }
         { exit }' "$0"
}

# ---------------------------------------------------------------------------
# Argument handling. Paths given on the command line are resolved against the
# caller's cwd first, then against the repository root, so the script behaves
# the same from anywhere without ever depending on being run from the root.
# ---------------------------------------------------------------------------
MODE="scan"
case "${1:-}" in
    --classify) MODE="classify"; shift ;;
    -h|--help)  print_header; exit 0 ;;
esac

if [ "$MODE" = "classify" ]; then
    [ "$#" -ge 1 ] || { echo "usage: $0 --classify FILE" >&2; exit 2; }
    classify "$1"
    exit 0
fi

FILES=()
if [ "$#" -gt 0 ]; then
    for f in "$@"; do
        if [ -f "$f" ]; then FILES+=("$f")
        elif [ -f "$REPO_ROOT/$f" ]; then FILES+=("$REPO_ROOT/$f")
        else echo "FAIL: no such file: $f" >&2; exit 2; fi
    done
else
    while IFS= read -r f; do FILES+=("$f"); done < <(
        find "$REPO_ROOT/tests" \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \) -type f | sort
    )
fi

if [ "${#FILES[@]}" -eq 0 ]; then
    echo "FAIL: no test files found to check." >&2
    exit 2
fi

incapable=0
checked=0
for f in "${FILES[@]}"; do
    checked=$((checked + 1))
    verdict="$(classify "$f")"
    status="${verdict%%$'\t'*}"
    if [ "$status" = "incapable" ]; then
        incapable=$((incapable + 1))
        rel="${f#"$REPO_ROOT"/}"
        echo "INCAPABLE: $rel"
        echo "    reason: $(printf '%s' "$verdict" | cut -f3)"
        echo "    rule:   $(printf '%s' "$verdict" | cut -f4)"
    fi
done

echo "---"
echo "checked=$checked verifying=$((checked - incapable)) incapable=$incapable"

if [ "$incapable" -gt 0 ]; then
    echo "FAIL: $incapable of $checked test file(s) cannot report failure."
    echo "      Each must gain a live failure mechanism before the suite can be"
    echo "      trusted. See scripts/verify_test_integrity.sh --help for the rules."
    exit 1
fi

echo "PASS: every checked test file can report failure"
