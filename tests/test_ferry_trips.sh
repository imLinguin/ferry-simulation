#!/bin/bash
# Test ferry trips count - validates that test_ramp.sh produces expected number of trips

# Get the directory where the test script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"
SIM_BIN="${1:-$BUILD_DIR/ferry-simulation}"

# Source shared library
source "$SCRIPT_DIR/tests.shlib"

# Test configuration
LOG_FILE="simulation.log"
EXPECTED_FERRY_TRIPS=10

echo "========================================" 
echo "Ferry Trips Count Test"
echo "========================================"
echo "This test validates that test_ramp.sh configuration produces exactly 10 ferry trips"
echo ""

# Clean up previous log
rm -f "$LOG_FILE"

# Run simulation with test_ramp.sh configuration
log_info "Running simulation with test_ramp.sh configuration..."
run_test_with_timeout 90 "$SCRIPT_DIR/test_ramp.sh $SIM_BIN"

exit_code=$?

if [ $exit_code -eq 124 ]; then
    log_error "Simulation timed out!"
    exit 1
elif [ $exit_code -ne 0 ]; then
    log_error "Simulation failed with exit code: $exit_code"
    exit 1
fi

# Verify log exists
if ! verify_log_exists "$LOG_FILE"; then
    log_error "Log file validation failed"
    exit 1
fi

echo ""
log_info "Validating simulation results..."
echo ""

# Count ferry trips with passengers > 0
actual_trips=$(count_ferry_trips "$LOG_FILE")

log_info "Expected ferry trips: $EXPECTED_FERRY_TRIPS"
log_info "Actual ferry trips (with passengers > 0): $actual_trips"

# Validate trip count
assert_equals "$EXPECTED_FERRY_TRIPS" "$actual_trips" "Ferry trips count"

# Get statistics
spawned=$(get_stat_passengers_spawned "$LOG_FILE")
boarded=$(get_stat_passengers_boarded "$LOG_FILE")
rejected_bag=$(get_stat_passengers_rejected_baggage "$LOG_FILE")

log_info "Passengers spawned: $spawned"
log_info "Passengers boarded: $boarded"
log_info "Passengers rejected at baggage: $rejected_bag"

# Validate passenger accounting
validate_passenger_accounting "$LOG_FILE"

# Validate capacity constraints
validate_ferry_capacity "$LOG_FILE" 500

# Check for errors
check_for_errors "$LOG_FILE"

# Print summary
print_test_summary

exit $TESTS_FAILED
