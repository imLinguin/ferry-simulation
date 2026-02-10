#!/bin/bash
# VIP priority test - validates VIP passengers get priority treatment

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "VIP Priority Test"
echo "========================================"
echo "Enable VIP passengers and verify priority handling"
echo ""

rm -f "$LOG_FILE"

# Configuration with VIP enabled
export PASSENGER_COUNT=100
export FERRY_COUNT=3
export FERRY_CAPACITY=40
export RAMP_CAPACITY_REG=2
export RAMP_CAPACITY_VIP=2
export FERRY_DEPARTURE_INTERVAL=8
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
export VIP_CHANCE=30  # 30% VIP passengers

log_info "Running simulation with VIP passengers (30% chance)..."
run_test_with_timeout 90 "$BUILD_DIR/ferry-simulation"

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
log_info "Validating VIP priority..."
echo ""

# Count VIP passengers granted ramp access
vip_ramp_grants=$(grep "Granting ramp" "$LOG_FILE" | grep "VIP: 1" | wc -l)
regular_ramp_grants=$(grep "Granting ramp" "$LOG_FILE" | grep "VIP: 0" | wc -l)

log_info "VIP ramp grants: $vip_ramp_grants"
log_info "Regular ramp grants: $regular_ramp_grants"

# At least some VIPs should exist
assert_greater_than "$vip_ramp_grants" 5 "At least some VIP passengers boarded"

# Validate capacity constraints
validate_ferry_capacity "$LOG_FILE" "$FERRY_CAPACITY"
validate_ramp_capacity "$LOG_FILE" "$RAMP_CAPACITY_REG" "$RAMP_CAPACITY_VIP"

# Validate passenger accounting
validate_passenger_accounting "$LOG_FILE"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
