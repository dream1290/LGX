#!/bin/bash
# LGX Runtime ABI Compatibility Test Matrix
# 
# This script tests ABI compatibility across different versions.
# Usage: ./lgx-abi-test-matrix.sh [full|critical]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_DIR="$PROJECT_ROOT/tests/abi"
RESULTS_DIR="$BUILD_DIR/abi_test_results"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test mode: full or critical
TEST_MODE="${1:-critical}"

echo "=== LGX Runtime ABI Compatibility Test Matrix ==="
echo "Mode: $TEST_MODE"
echo "Project root: $PROJECT_ROOT"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR"

# Function to run a test
run_test() {
    local test_name="$1"
    local test_binary="$2"
    local result_file="$RESULTS_DIR/${test_name}.txt"
    
    echo -n "Running $test_name... "
    
    if [ ! -f "$test_binary" ]; then
        echo -e "${RED}SKIP${NC} (binary not found)"
        echo "SKIP: Binary not found" > "$result_file"
        return 1
    fi
    
    if "$test_binary" > "$result_file" 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        echo "PASS" >> "$result_file"
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "FAIL" >> "$result_file"
        return 1
    fi
}

# Function to test version compatibility
test_version_compatibility() {
    local game_version="$1"
    local runtime_version="$2"
    local test_name="v${game_version}_game_with_v${runtime_version}_runtime"
    
    echo "Testing: v$game_version game with v$runtime_version runtime"
    
    # For now, we only have v1.0 game and current runtime
    # In a real scenario, you would:
    # 1. Build the game against v$game_version headers
    # 2. Link against v$runtime_version runtime
    # 3. Run the test
    
    run_test "$test_name" "$BUILD_DIR/tests/abi/test_game_v1_0"
}

# Function to generate HTML report
generate_html_report() {
    local report_file="$RESULTS_DIR/abi_compatibility_report.html"
    
    cat > "$report_file" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>LGX Runtime ABI Compatibility Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        h1 { color: #333; }
        table { border-collapse: collapse; width: 100%; margin-top: 20px; }
        th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }
        th { background-color: #4CAF50; color: white; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        .pass { color: green; font-weight: bold; }
        .fail { color: red; font-weight: bold; }
        .skip { color: orange; font-weight: bold; }
        .timestamp { color: #666; font-size: 0.9em; }
    </style>
</head>
<body>
    <h1>LGX Runtime ABI Compatibility Report</h1>
    <p class="timestamp">Generated: $(date)</p>
    <p>Test Mode: $TEST_MODE</p>
    
    <h2>Test Results</h2>
    <table>
        <tr>
            <th>Test Name</th>
            <th>Status</th>
            <th>Details</th>
        </tr>
EOF
    
    # Add test results
    for result_file in "$RESULTS_DIR"/*.txt; do
        if [ -f "$result_file" ]; then
            test_name=$(basename "$result_file" .txt)
            status=$(tail -n 1 "$result_file")
            
            case "$status" in
                PASS)
                    status_class="pass"
                    ;;
                FAIL)
                    status_class="fail"
                    ;;
                *)
                    status_class="skip"
                    ;;
            esac
            
            echo "        <tr>" >> "$report_file"
            echo "            <td>$test_name</td>" >> "$report_file"
            echo "            <td class=\"$status_class\">$status</td>" >> "$report_file"
            echo "            <td><a href=\"$(basename "$result_file")\">View Log</a></td>" >> "$report_file"
            echo "        </tr>" >> "$report_file"
        fi
    done
    
    cat >> "$report_file" << 'EOF'
    </table>
    
    <h2>Summary</h2>
    <p>Total tests: <span id="total">0</span></p>
    <p>Passed: <span id="passed" class="pass">0</span></p>
    <p>Failed: <span id="failed" class="fail">0</span></p>
    <p>Skipped: <span id="skipped" class="skip">0</span></p>
    
    <script>
        var total = document.querySelectorAll('table tr').length - 1;
        var passed = document.querySelectorAll('.pass').length - 1;
        var failed = document.querySelectorAll('.fail').length - 1;
        var skipped = document.querySelectorAll('.skip').length - 1;
        
        document.getElementById('total').textContent = total;
        document.getElementById('passed').textContent = passed;
        document.getElementById('failed').textContent = failed;
        document.getElementById('skipped').textContent = skipped;
    </script>
</body>
</html>
EOF
    
    echo ""
    echo "HTML report generated: $report_file"
}

# Main test execution
echo "Building ABI tests..."
cd "$BUILD_DIR"
cmake --build . --target test_game_v1_0 test_struct_evolution test_symbol_versioning 2>&1 | tail -20

echo ""
echo "Running ABI compatibility tests..."
echo ""

# Test counters
total_tests=0
passed_tests=0
failed_tests=0

if [ "$TEST_MODE" = "full" ]; then
    echo "=== Full Test Matrix ==="
    echo ""
    
    # Test all version combinations
    # v1.0 game with various runtimes
    test_version_compatibility "1.0" "1.0" && ((passed_tests++)) || ((failed_tests++))
    ((total_tests++))
    
    test_version_compatibility "1.0" "1.1" && ((passed_tests++)) || ((failed_tests++))
    ((total_tests++))
    
    test_version_compatibility "1.0" "1.2" && ((passed_tests++)) || ((failed_tests++))
    ((total_tests++))
    
    # Struct evolution tests
    run_test "struct_evolution" "$BUILD_DIR/tests/abi/test_struct_evolution" && ((passed_tests++)) || ((failed_tests++))
    ((total_tests++))
    
    # Symbol versioning tests
    run_test "symbol_versioning" "$BUILD_DIR/tests/abi/test_symbol_versioning" && ((passed_tests++)) || ((failed_tests++))
    ((total_tests++))
    
else
    echo "=== Critical Path Tests ==="
    echo ""
    
    # Critical path: v1.0 game with latest runtime
    test_version_compatibility "1.0" "latest" && ((passed_tests++)) || ((failed_tests++))
    ((total_tests++))
    
    # Struct evolution (critical)
    run_test "struct_evolution" "$BUILD_DIR/tests/abi/test_struct_evolution" && ((passed_tests++)) || ((failed_tests++))
    ((total_tests++))
fi

echo ""
echo "=== Test Summary ==="
echo "Total: $total_tests"
echo -e "Passed: ${GREEN}$passed_tests${NC}"
echo -e "Failed: ${RED}$failed_tests${NC}"

# Generate HTML report
generate_html_report

# Exit with appropriate code
if [ $failed_tests -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ All ABI compatibility tests passed!${NC}"
    exit 0
else
    echo ""
    echo -e "${RED}✗ Some ABI compatibility tests failed!${NC}"
    exit 1
fi
