#!/bin/bash
# Comprehensive Security Audit for LGX Runtime
# This script runs all available security testing tools and generates a report

set -e

AUDIT_DIR="security_audit_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$AUDIT_DIR"

echo "=========================================="
echo "LGX Runtime Security Audit"
echo "=========================================="
echo "Audit directory: $AUDIT_DIR"
echo ""

# Function to check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Initialize summary
echo "LGX Runtime Security Audit - $(date)" > "$AUDIT_DIR/audit_summary.txt"

# Function to log results
log_result() {
    echo "$1" | tee -a "$AUDIT_DIR/audit_summary.txt"
}
echo "========================================" >> "$AUDIT_DIR/audit_summary.txt"
echo "" >> "$AUDIT_DIR/audit_summary.txt"

# ============================================
# 1. Static Analysis with GCC
# ============================================
echo "1. Running GCC Static Analysis..."
log_result "1. GCC Static Analysis"

if command_exists gcc; then
    log_result "   Status: RUNNING"
    
    # Build with all warnings enabled
    mkdir -p "$AUDIT_DIR/gcc_analysis"
    cd "$AUDIT_DIR/gcc_analysis"
    
    cmake ../.. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wformat-security -Wnull-dereference -Wstack-protector -Wstrict-overflow=3 -Warray-bounds=2 -Wshift-overflow=2 -Wstringop-overflow=4 -fanalyzer" \
        > gcc_config.log 2>&1 || true
    
    cmake --build . -j$(nproc) > gcc_build.log 2>&1 || true
    
    # Count warnings and errors
    WARNINGS=$(grep -c "warning:" gcc_build.log || echo "0")
    ERRORS=$(grep -c "error:" gcc_build.log || echo "0")
    
    log_result "   Warnings: $WARNINGS"
    log_result "   Errors: $ERRORS"
    
    if [ "$ERRORS" -eq 0 ]; then
        log_result "   Result: PASS"
    else
        log_result "   Result: FAIL - Review gcc_build.log"
    fi
    
    cd ../..
else
    log_result "   Status: SKIPPED (GCC not found)"
fi

echo ""

# ============================================
# 2. Clang Static Analyzer
# ============================================
echo "2. Running Clang Static Analyzer..."
log_result "2. Clang Static Analyzer"

if command_exists scan-build; then
    log_result "   Status: RUNNING"
    ./scripts/run_static_analysis.sh > "$AUDIT_DIR/clang_analyzer.log" 2>&1 || true
    
    # Check for issues
    REPORT_DIR="static_analysis_reports"
    LATEST_REPORT=$(ls -td "$REPORT_DIR"/*/ 2>/dev/null | head -1 || echo "")
    
    if [ -n "$LATEST_REPORT" ]; then
        ISSUE_COUNT=$(find "$LATEST_REPORT" -name "*.html" | wc -l)
        log_result "   Issues Found: $ISSUE_COUNT"
        
        if [ "$ISSUE_COUNT" -eq 0 ]; then
            log_result "   Result: PASS"
        else
            log_result "   Result: REVIEW REQUIRED - See $LATEST_REPORT"
        fi
    else
        log_result "   Result: PASS (No issues)"
    fi
else
    log_result "   Status: SKIPPED (scan-build not installed)"
    log_result "   Install: sudo apt-get install clang-tools"
fi

echo ""

# ============================================
# 3. clang-tidy
# ============================================
echo "3. Running clang-tidy..."
log_result "3. clang-tidy"

if command_exists clang-tidy; then
    log_result "   Status: RUNNING"
    ./scripts/run_clang_tidy.sh > "$AUDIT_DIR/clang_tidy.log" 2>&1 || true
    
    # Count issues
    if [ -f "clang_tidy_report.txt" ]; then
        ISSUE_COUNT=$(grep -c "warning:" clang_tidy_report.txt || echo "0")
        log_result "   Issues Found: $ISSUE_COUNT"
        
        mv clang_tidy_report.txt "$AUDIT_DIR/"
        
        if [ "$ISSUE_COUNT" -eq 0 ]; then
            log_result "   Result: PASS"
        else
            log_result "   Result: REVIEW REQUIRED - See clang_tidy_report.txt"
        fi
    fi
else
    log_result "   Status: SKIPPED (clang-tidy not installed)"
    log_result "   Install: sudo apt-get install clang-tidy"
fi

echo ""

# ============================================
# 4. Coverity Scan
# ============================================
echo "4. Running Coverity Scan..."
log_result "4. Coverity Scan"

if command_exists cov-build; then
    log_result "   Status: RUNNING"
    ./scripts/run_coverity_scan.sh > "$AUDIT_DIR/coverity.log" 2>&1 || true
    
    # Check for defects
    if [ -d "coverity_report" ]; then
        DEFECT_COUNT=$(find coverity_report -name "*.html" | wc -l)
        log_result "   Defects Found: $DEFECT_COUNT"
        
        mv coverity_report "$AUDIT_DIR/"
        
        if [ "$DEFECT_COUNT" -eq 0 ]; then
            log_result "   Result: PASS"
        else
            log_result "   Result: REVIEW REQUIRED - See coverity_report/"
        fi
    fi
else
    log_result "   Status: SKIPPED (Coverity not installed)"
    log_result "   Install: https://scan.coverity.com/"
fi

echo ""

# ============================================
# 5. AFL Fuzzing
# ============================================
echo "5. Running AFL Fuzzing (Quick Test)..."
log_result "5. AFL Fuzzing"

if command_exists afl-fuzz; then
    log_result "   Status: RUNNING (60 second test)"
    
    cd tests/fuzzing
    ./build_afl.sh > "$AUDIT_DIR/afl_build.log" 2>&1 || true
    
    # Run AFL for 60 seconds
    timeout 60 afl-fuzz -i testcases -o findings ./fuzz_api_inputs > "$AUDIT_DIR/afl_output.log" 2>&1 || true
    
    # Check for crashes
    if [ -d "findings/crashes" ]; then
        CRASH_COUNT=$(ls findings/crashes/ 2>/dev/null | wc -l)
        log_result "   Crashes Found: $CRASH_COUNT"
        
        if [ "$CRASH_COUNT" -eq 0 ]; then
            log_result "   Result: PASS (60s test)"
        else
            log_result "   Result: FAIL - Crashes found in findings/crashes/"
        fi
    fi
    
    cd ../..
else
    log_result "   Status: SKIPPED (AFL not installed)"
    log_result "   Install: sudo apt-get install afl"
    log_result "   Note: Full fuzzing campaign should run for 24+ hours"
fi

echo ""

# ============================================
# 6. libFuzzer
# ============================================
echo "6. Running libFuzzer (Quick Test)..."
log_result "6. libFuzzer"

if command_exists clang && clang --version | grep -q "libFuzzer"; then
    log_result "   Status: RUNNING (60 second test)"
    
    cd tests/fuzzing
    ./build_libfuzzer.sh > "$AUDIT_DIR/libfuzzer_build.log" 2>&1 || true
    
    # Run libFuzzer for 60 seconds
    ./fuzz_allocation_patterns -max_total_time=60 > "$AUDIT_DIR/libfuzzer_output.log" 2>&1 || true
    
    # Check for crashes
    CRASH_COUNT=$(ls crash-* 2>/dev/null | wc -l || echo "0")
    log_result "   Crashes Found: $CRASH_COUNT"
    
    if [ "$CRASH_COUNT" -eq 0 ]; then
        log_result "   Result: PASS (60s test)"
    else
        log_result "   Result: FAIL - Crashes found"
    fi
    
    cd ../..
else
    log_result "   Status: SKIPPED (clang with libFuzzer not available)"
    log_result "   Install: sudo apt-get install clang"
    log_result "   Note: Full fuzzing campaign should run for 24+ hours"
fi

echo ""

# ============================================
# 7. Memory Safety Tests
# ============================================
echo "7. Running Memory Safety Tests..."
log_result "7. Memory Safety Tests (Valgrind/AddressSanitizer)"

if command_exists valgrind; then
    log_result "   Status: RUNNING Valgrind"
    
    # Build tests
    cd build
    
    # Run unit tests under Valgrind
    valgrind --leak-check=full --error-exitcode=1 \
        ./tests/unit/test_init_shutdown > "$AUDIT_DIR/valgrind.log" 2>&1 || true
    
    # Check for errors
    ERRORS=$(grep -c "ERROR SUMMARY: [1-9]" "$AUDIT_DIR/valgrind.log" || echo "0")
    LEAKS=$(grep -c "definitely lost:" "$AUDIT_DIR/valgrind.log" || echo "0")
    
    log_result "   Memory Errors: $ERRORS"
    log_result "   Memory Leaks: $LEAKS"
    
    if [ "$ERRORS" -eq 0 ] && [ "$LEAKS" -eq 0 ]; then
        log_result "   Result: PASS"
    else
        log_result "   Result: FAIL - See valgrind.log"
    fi
    
    cd ..
else
    log_result "   Status: SKIPPED (Valgrind not installed)"
    log_result "   Install: sudo apt-get install valgrind"
fi

# AddressSanitizer
log_result ""
log_result "   AddressSanitizer:"

if command_exists gcc; then
    log_result "   Status: RUNNING"
    
    # Build with ASan
    mkdir -p "$AUDIT_DIR/asan_build"
    cd "$AUDIT_DIR/asan_build"
    
    cmake ../.. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
        > asan_config.log 2>&1 || true
    
    cmake --build . -j$(nproc) > asan_build.log 2>&1 || true
    
    # Run tests
    ctest --output-on-failure > asan_test.log 2>&1 || true
    
    # Check for ASan errors
    ASAN_ERRORS=$(grep -c "AddressSanitizer" asan_test.log || echo "0")
    log_result "   ASan Errors: $ASAN_ERRORS"
    
    if [ "$ASAN_ERRORS" -eq 0 ]; then
        log_result "   Result: PASS"
    else
        log_result "   Result: FAIL - See asan_test.log"
    fi
    
    cd ../..
else
    log_result "   Status: SKIPPED (GCC not available)"
fi

echo ""

# ============================================
# 8. Security-Specific Tests
# ============================================
echo "8. Running Security-Specific Tests..."
log_result "8. Security-Specific Tests"

cd build

# Run failure injection tests
log_result "   Failure Injection Tests:"
ctest -R failure_injection --output-on-failure > "$AUDIT_DIR/failure_injection.log" 2>&1 || true
FAILURES=$(grep -c "Failed" "$AUDIT_DIR/failure_injection.log" || echo "0")
log_result "   Failures: $FAILURES"

# Run fuzzing tests
log_result "   Fuzzing Tests:"
ctest -R fuzzing --output-on-failure > "$AUDIT_DIR/fuzzing_tests.log" 2>&1 || true
FAILURES=$(grep -c "Failed" "$AUDIT_DIR/fuzzing_tests.log" || echo "0")
log_result "   Failures: $FAILURES"

cd ..

if [ "$FAILURES" -eq 0 ]; then
    log_result "   Result: PASS"
else
    log_result "   Result: FAIL - See test logs"
fi

echo ""

# ============================================
# 9. Generate Final Report
# ============================================
echo "=========================================="
echo "Security Audit Complete"
echo "=========================================="
echo ""

log_result ""
log_result "=========================================="
log_result "AUDIT SUMMARY"
log_result "=========================================="
log_result ""

# Count total issues
TOTAL_ISSUES=0

# Add up all issues found
if [ -f "$AUDIT_DIR/gcc_analysis/gcc_build.log" ]; then
    GCC_ISSUES=$(grep -c "warning:\|error:" "$AUDIT_DIR/gcc_analysis/gcc_build.log" || echo "0")
    TOTAL_ISSUES=$((TOTAL_ISSUES + GCC_ISSUES))
fi

log_result "Total Issues Found: $TOTAL_ISSUES"
log_result ""

# Recommendations
log_result "RECOMMENDATIONS:"
log_result "1. Review all FAIL and REVIEW REQUIRED items above"
log_result "2. Install missing tools for complete coverage"
log_result "3. Run full fuzzing campaigns (24+ hours) before production"
log_result "4. Schedule regular security audits (quarterly)"
log_result "5. Consider third-party security audit before v1.0 release"
log_result ""

# Missing tools
log_result "MISSING TOOLS (install for complete audit):"
command_exists scan-build || log_result "  - scan-build (clang-tools)"
command_exists clang-tidy || log_result "  - clang-tidy"
command_exists cov-build || log_result "  - Coverity Scan"
command_exists afl-fuzz || log_result "  - AFL fuzzer"
command_exists valgrind || log_result "  - Valgrind"
log_result ""

log_result "Full audit report saved to: $AUDIT_DIR/"
log_result "Summary: $AUDIT_DIR/audit_summary.txt"

echo ""
echo "Full audit report saved to: $AUDIT_DIR/"
echo "Summary: $AUDIT_DIR/audit_summary.txt"
echo ""

# Exit with error if critical issues found
if [ "$TOTAL_ISSUES" -gt 0 ]; then
    echo "WARNING: Issues found - review required before production deployment"
    exit 1
else
    echo "SUCCESS: No critical issues found"
    exit 0
fi
