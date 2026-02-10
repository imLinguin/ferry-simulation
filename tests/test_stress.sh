#!/bin/bash
# Stress test - validates system handles large load without deadlocks

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"
SIM_BIN="${1:-$BUILD_DIR/ferry-simulation}"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "Stress Test"
echo "========================================"
echo "Large passenger count (5000+), multiple ferries, fast operations"
echo ""

rm -f "$LOG_FILE"

# Stress configuration
export PASSENGER_COUNT=5000
export FERRY_COUNT=10
export FERRY_CAPACITY=600
export RAMP_CAPACITY_REG=5
export RAMP_CAPACITY_VIP=3
export FERRY_DEPARTURE_INTERVAL=8
export FERRY_TRAVEL_TIME=1
export PASSENGER_SECURITY_TIME_MIN=3
export PASSENGER_SECURITY_TIME_MAX=7
export PASSENGER_BOARDING_TIME=500
export FERRY_GATE_MAX_DELAY=5000
export FERRY_BAGGAGE_LIMIT_MIN=40
export FERRY_BAGGAGE_LIMIT_MAX=60
export PASSENGER_BAG_WEIGHT_MIN=5
export PASSENGER_BAG_WEIGHT_MAX=50
export DANGEROUS_ITEM_CHANCE=0
export VIP_CHANCE=10

log_info "Running stress test with $PASSENGER_COUNT passengers..."
log_warning "This test may take up to 120 seconds..."

# Use longer timeout for stress test
run_test_with_timeout 120 "$SIM_BIN"

exit_code=$?

if [ $exit_code -eq 124 ]; then
    log_error "STRESS TEST FAILED: Simulation timed out (possible deadlock)"
    exit 1
elif [ $exit_code -ne 0 ]; then
    log_error "Simulation failed with exit code: $exit_code"
    exit 1
fi

if ! verify_log_exists "$LOG_FILE"; then
    exit 1
fi

log_info "✓ Simulation completed without timeout (no deadlock detected)"
((TESTS_PASSED++))

echo ""
log_info "Validating stress test results..."
echo ""

spawned=$(get_stat_passengers_spawned "$LOG_FILE")
boarded=$(get_stat_passengers_boarded "$LOG_FILE")
rejected_bag=$(get_stat_passengers_rejected_baggage "$LOG_FILE")
ferry_trips=$(count_ferry_trips "$LOG_FILE")

log_info "Passengers spawned: $spawned"
log_info "Passengers boarded: $boarded"
log_info "Passengers rejected at baggage: $rejected_bag"
log_info "Ferry trips completed: $ferry_trips"

# Validate passenger accounting
validate_passenger_accounting "$LOG_FILE"

# Validate capacity constraints
validate_ferry_capacity "$LOG_FILE" "$FERRY_CAPACITY"
validate_ramp_capacity "$LOG_FILE" "$RAMP_CAPACITY_REG" "$RAMP_CAPACITY_VIP"

# Validate reasonable throughput
assert_greater_than "$boarded" 4000 "Most passengers boarded"
assert_greater_than "$ferry_trips" 5 "Multiple ferry trips completed"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
