#!/bin/bash
# LGX Runtime Core - Chaos Testing Suite
# Runs comprehensive chaos tests to validate system resilience

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to run a test
run_test() {
    local test_name=$1
    local test_command=$2
    
    echo -e "${YELLOW}Running: ${test_name}${NC}"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if eval "$test_command"; then
        echo -e "${GREEN}✓ PASSED: ${test_name}${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ FAILED: ${test_name}${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    echo ""
}

echo "========================================"
echo "LGX Runtime Core - Chaos Testing Suite"
echo "========================================"
echo ""

# Test 1: Memory Pressure
run_test "Memory Pressure (5% failure rate)" \
    "./test_chaos_memory_pressure --failure-rate=0.05 --iterations=10000"

# Test 2: Memory Pressure (High stress)
run_test "Memory Pressure (20% failure rate)" \
    "./test_chaos_memory_pressure --failure-rate=0.20 --iterations=5000"

# Test 3: Latency Spikes
run_test "Latency Spikes (2% rate, 10ms max)" \
    "./test_chaos_latency_spikes --spike-rate=0.02 --max-latency-ms=10 --iterations=5000"

# Test 4: I/O Errors
run_test "I/O Errors (5% failure rate)" \
    "./test_chaos_io_errors --error-rate=0.05 --operations=1000"

# Test 5: Combined Chaos (all failures)
run_test "Combined Chaos (60 second duration)" \
    "./test_chaos_combined --all-failures --duration-sec=60"

# Summary
echo "========================================"
echo "Test Summary"
echo "========================================"
echo -e "Total tests:  ${TOTAL_TESTS}"
echo -e "Passed:       ${GREEN}${PASSED_TESTS}${NC}"
echo -e "Failed:       ${RED}${FAILED_TESTS}${NC}"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All chaos tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some chaos tests failed!${NC}"
    exit 1
fi
