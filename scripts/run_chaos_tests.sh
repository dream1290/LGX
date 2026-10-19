#!/usr/bin/env bash
# Chaos testing regression gate. The deadlock this script exists for is
# covered by test_chaos_deadlock (explicit target, 20s timeout): pre-fix
# the test hangs and ctest kills it with a non-zero result.
set -euo pipefail
BUILD_DIR="${1:-build}"
ctest --test-dir "$BUILD_DIR" -R '^test_chaos_deadlock$' --output-on-failure
