#!/bin/bash
# Basic flow test - validates complete passenger journey with small count

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"
SIM_BIN="${1:-$BUILD_DIR/ferry-simulation}"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "Basic Flow Test"
echo "========================================"
echo "Small passenger count (20), verify all complete their journey"
echo ""

rm -f "$LOG_FILE"

# Configuration for basic test
export PASSENGER_COUNT=20
export FERRY_COUNT=2
export FERRY_CAPACITY=15
export RAMP_CAPACITY_REG=2
export RAMP_CAPACITY_VIP=1
export FERRY_DEPARTURE_INTERVAL=5
export FERRY_TRAVEL_TIME=1
export PASSENGER_SECURITY_TIME_MIN=5
export PASSENGER_SECURITY_TIME_MAX=10
export PASSENGER_BOARDING_TIME=1000
export FERRY_GATE_MAX_DELAY=1000
export FERRY_BAGGAGE_LIMIT_MIN=40
export FERRY_BAGGAGE_LIMIT_MAX=60
export PASSENGER_BAG_WEIGHT_MIN=5
export PASSENGER_BAG_WEIGHT_MAX=50
export DANGEROUS_ITEM_CHANCE=0
export VIP_CHANCE=0

log_info "Running simulation..."
run_test_with_timeout 30 "$SIM_BIN"

exit_code=$?

if [ $exit_code -eq 124 ]; then
    log_error "Simulation timed out!"
    exit 1
elif [ $exit_code -ne 0 ]; then
    log_error "Simulation failed with exit code: $exit_code"
    exit 1
fi

if ! verify_log_exists "$LOG_FILE"; then
    exit 1
fi

echo ""
log_info "Validating results..."
echo ""

spawned=$(get_stat_passengers_spawned "$LOG_FILE")
boarded=$(get_stat_passengers_boarded "$LOG_FILE")
rejected_bag=$(get_stat_passengers_rejected_baggage "$LOG_FILE")

log_info "Passengers spawned: $spawned"
log_info "Passengers boarded: $boarded"
log_info "Passengers rejected at baggage: $rejected_bag"

# All passengers should either board or be rejected
validate_passenger_accounting "$LOG_FILE"

# Check that most passengers boarded (baggage limits are generous)
assert_greater_than "$boarded" 15 "Most passengers boarded"

# Validate capacity constraints
validate_ferry_capacity "$LOG_FILE" "$FERRY_CAPACITY"
validate_ramp_capacity "$LOG_FILE" "$RAMP_CAPACITY_REG" "$RAMP_CAPACITY_VIP"

# Validate ramp empty on departure
validate_ramp_empty_on_departure "$LOG_FILE"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
