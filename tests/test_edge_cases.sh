#!/bin/bash
# Edge cases test - validates various edge conditions

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"
SIM_BIN="${1:-$BUILD_DIR/ferry-simulation}"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "Edge Cases Test"
echo "========================================"
echo "Testing: exact capacity limits, empty ferries, boundary conditions"
echo ""

rm -f "$LOG_FILE"

# Configuration with boundary conditions
export PASSENGER_COUNT=50
export FERRY_COUNT=5
export FERRY_CAPACITY=10  # Exact capacity
export RAMP_CAPACITY_REG=2
export RAMP_CAPACITY_VIP=1
export FERRY_DEPARTURE_INTERVAL=5  # Short interval (may cause empty ferries)
export FERRY_TRAVEL_TIME=1
export PASSENGER_SECURITY_TIME_MIN=20  # Slow security
export PASSENGER_SECURITY_TIME_MAX=40  # to create timing issues
export PASSENGER_BOARDING_TIME=1000
export FERRY_GATE_MAX_DELAY=2000
export FERRY_BAGGAGE_LIMIT_MIN=30
export FERRY_BAGGAGE_LIMIT_MAX=40  # Narrow range
export PASSENGER_BAG_WEIGHT_MIN=25  # High weights
export PASSENGER_BAG_WEIGHT_MAX=35  # to test edge of limits
export DANGEROUS_ITEM_CHANCE=0
export VIP_CHANCE=0

log_info "Running edge cases test..."
run_test_with_timeout 90 "$SIM_BIN"

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
log_info "Validating edge cases..."
echo ""

spawned=$(get_stat_passengers_spawned "$LOG_FILE")
boarded=$(get_stat_passengers_boarded "$LOG_FILE")
rejected_bag=$(get_stat_passengers_rejected_baggage "$LOG_FILE")

log_info "Passengers spawned: $spawned"
log_info "Passengers boarded: $boarded"
log_info "Passengers rejected at baggage: $rejected_bag"

# Count empty ferry departures (with 0 passengers)
empty_ferries=$(grep "Ferry departing" "$LOG_FILE" | \
    awk -F'final_passenger_count: ' '{print $2}' | \
    awk -F',' '{print $1}' | \
    awk '$1 == 0' | \
    wc -l)

log_info "Empty ferry departures: $empty_ferries"

if [ "$empty_ferries" -gt 0 ]; then
    log_info "✓ System handled empty ferry departures correctly"
    ((TESTS_PASSED++))
fi

# Count ferries with exactly full capacity
full_ferries=$(grep "Ferry departing" "$LOG_FILE" | \
    awk -F'final_passenger_count: ' '{print $2}' | \
    awk -F',' '{print $1}' | \
    awk -v cap="$FERRY_CAPACITY" '$1 == cap' | \
    wc -l)

log_info "Ferries at exact capacity: $full_ferries"

if [ "$full_ferries" -gt 0 ]; then
    log_info "✓ System handled exact capacity correctly"
    ((TESTS_PASSED++))
fi

# Validate passenger accounting
validate_passenger_accounting "$LOG_FILE"

# Validate capacity was never exceeded
validate_ferry_capacity "$LOG_FILE" "$FERRY_CAPACITY"

# Validate ramp empty on departure
validate_ramp_empty_on_departure "$LOG_FILE"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
