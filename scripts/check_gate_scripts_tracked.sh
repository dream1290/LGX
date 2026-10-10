#!/usr/bin/env bash
# Fails if a CI gate script is not tracked by git.
#
# Why this exists
# ---------------
# .gitignore ignores /scripts/* so that developer scratch scripts do not get
# committed, and then un-ignores the ones CI depends on. That makes the gate
# scripts' tracked status depend entirely on a few negation lines staying in
# .gitignore. A later wholesale rewrite of .gitignore -- which is exactly what
# the next task in this plan does -- silently drops those negations, the scripts
# become untracked, and nothing fails at that moment: `git status` goes quiet,
# CI stops running the gate, and the suite rots again unnoticed.
#
# So the requirement is checked at run time instead of being left as a comment.
# Both gate scripts call this on startup. If either one is untracked, they fail
# loudly instead of quietly losing their own enforcement.
#
# The two scripts that must stay tracked:
#   scripts/verify_test_integrity.sh   - the CI gate itself
#   scripts/count_tests.sh             - the census it is measured against
#
# Behaviour:
#   * not inside a git work tree -> SKIP (exit 0). Git tracking is meaningless
#     in a source tarball; failing there would be wrong.
#   * inside a work tree and both scripts tracked -> PASS.
#   * inside a work tree and either script untracked -> FAIL (exit 1).
#
# Usage: scripts/check_gate_scripts_tracked.sh [repo-root] [path ...]
#        Defaults to this script's own repository and the two gate scripts.
# ===========================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ROOT="${1:-$REPO_ROOT}"
if [ "$#" -gt 0 ]; then shift; fi
if [ "$#" -gt 0 ]; then
    GATES=("$@")
else
    GATES=(scripts/verify_test_integrity.sh scripts/count_tests.sh)
fi

if ! command -v git >/dev/null 2>&1; then
    echo "SKIP: git not available; cannot confirm the gate scripts are tracked" >&2
    exit 0
fi

if ! git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "SKIP: $ROOT is not a git work tree; gate-script tracking does not apply" >&2
    exit 0
fi

untracked=()
for gate in "${GATES[@]}"; do
    if ! git -C "$ROOT" ls-files --error-unmatch -- "$gate" >/dev/null 2>&1; then
        untracked+=("$gate")
    fi
done

if [ "${#untracked[@]}" -eq 0 ]; then
    echo "PASS: CI gate scripts are tracked by git (${#GATES[@]} checked)" >&2
    exit 0
fi

echo "FAIL: ${#untracked[@]} CI gate script(s) are NOT tracked by git:" >&2
for gate in "${untracked[@]}"; do
    echo "  - $gate" >&2
done
echo >&2
echo "  .gitignore ignores /scripts/* and un-ignores only the gates." >&2
echo "  If these are untracked, CI cannot run them and the suite rots unnoticed." >&2
echo "  Restore the negations, or force-add once:" >&2
for gate in "${untracked[@]}"; do
    echo "    git add -f $gate" >&2
done
exit 1
