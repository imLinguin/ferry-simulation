#!/bin/bash
# Master test runner - executes all ferry simulation tests

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"
SIM_BIN="${1:-$BUILD_DIR/ferry-simulation}"

source "$SCRIPT_DIR/tests.shlib"

echo "========================================"
echo "Ferry Simulation - Complete Test Suite"
echo "========================================"
echo ""

TOTAL_TESTS_PASSED=0
TOTAL_TESTS_FAILED=0

# Array of all test scripts
declare -a test_scripts=(
    "test_basic_flow.sh"
    "test_ferry_trips.sh"
    "test_passenger_accounting.sh"
    "test_capacity_limits.sh"
    "test_vip_priority.sh"
    "test_security_segregation.sh"
    "test_edge_cases.sh"
    "test_early_departure.sh"
    "test_port_closure.sh"
    "test_stress.sh"
)

# Run each test
for test_script in "${test_scripts[@]}"; do
    test_path="$SCRIPT_DIR/$test_script"
    
    if [ ! -f "$test_path" ]; then
        log_error "Test script not found: $test_script"
        continue
    fi
    
    echo ""
    echo "========================================" 
    echo "Running: $test_script"
    echo "========================================"
    echo ""
    
    # Make script executable
    chmod +x "$test_path"
    
    # Run test and capture exit code
    "$test_path" "$SIM_BIN"
    exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        log_info "✓ $test_script: PASSED"
        ((TOTAL_TESTS_PASSED++))
    else
        log_error "✗ $test_script: FAILED (exit code: $exit_code)"
        ((TOTAL_TESTS_FAILED++))
    fi
    
    # Clean up log file between tests
    rm -f simulation.log
    
    echo ""
done

# Print final summary
echo ""
echo "========================================"
echo "Final Test Suite Summary"
echo "========================================"
echo "Total test suites run: $((TOTAL_TESTS_PASSED + TOTAL_TESTS_FAILED))"
echo -e "Passed: ${GREEN}${TOTAL_TESTS_PASSED}${NC}"
echo -e "Failed: ${RED}${TOTAL_TESTS_FAILED}${NC}"
echo "========================================"

if [ $TOTAL_TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All test suites passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some test suites failed!${NC}"
    exit 1
fi
