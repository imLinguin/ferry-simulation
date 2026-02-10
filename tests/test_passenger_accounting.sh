#!/bin/bash
# Passenger accounting test - validates all passengers are accounted for

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"
SIM_BIN="${1:-$BUILD_DIR/ferry-simulation}"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "Passenger Accounting Test"
echo "========================================"
echo "Verify: spawned = boarded + rejected"
echo ""

rm -f "$LOG_FILE"

export PASSENGER_COUNT=100
export FERRY_COUNT=3
export FERRY_CAPACITY=50
export RAMP_CAPACITY_REG=3
export RAMP_CAPACITY_VIP=2
export FERRY_DEPARTURE_INTERVAL=7
export FERRY_TRAVEL_TIME=1
export PASSENGER_SECURITY_TIME_MIN=5
export PASSENGER_SECURITY_TIME_MAX=10
export PASSENGER_BOARDING_TIME=1000
export FERRY_GATE_MAX_DELAY=1000
export FERRY_BAGGAGE_LIMIT_MIN=30
export FERRY_BAGGAGE_LIMIT_MAX=60
export PASSENGER_BAG_WEIGHT_MIN=5
export PASSENGER_BAG_WEIGHT_MAX=50
export DANGEROUS_ITEM_CHANCE=0
export VIP_CHANCE=0

log_info "Running simulation..."
run_test_with_timeout 60 "$SIM_BIN"

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
log_info "Validating passenger accounting..."
echo ""

spawned=$(get_stat_passengers_spawned "$LOG_FILE")
boarded=$(get_stat_passengers_boarded "$LOG_FILE")
rejected_bag=$(get_stat_passengers_rejected_baggage "$LOG_FILE")
rejected_security=$(get_stat_passengers_screened_rejected "$LOG_FILE")

log_info "Passengers spawned: $spawned"
log_info "Passengers boarded: $boarded"
log_info "Passengers rejected at baggage: $rejected_bag"
log_info "Passengers rejected at security: $rejected_security"

total_accounted=$((boarded + rejected_bag + rejected_security))
log_info "Total accounted: $total_accounted"

# Main validation: all passengers must be accounted for
assert_equals "$spawned" "$total_accounted" "All passengers accounted for"

# Validate capacity constraints
validate_ferry_capacity "$LOG_FILE" "$FERRY_CAPACITY"
validate_ramp_capacity "$LOG_FILE" "$RAMP_CAPACITY_REG" "$RAMP_CAPACITY_VIP"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
