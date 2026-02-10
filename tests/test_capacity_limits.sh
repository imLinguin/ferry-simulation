#!/bin/bash
# Capacity limits test - validates ferry, ramp, and security capacity constraints

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"
SIM_BIN="${1:-$BUILD_DIR/ferry-simulation}"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "Capacity Limits Test"
echo "========================================"
echo "Stress test capacity constraints on ferry, ramp, and security"
echo ""

rm -f "$LOG_FILE"

# Configuration to stress capacity limits
export PASSENGER_COUNT=200
export FERRY_COUNT=3
export FERRY_CAPACITY=30
export RAMP_CAPACITY_REG=3
export RAMP_CAPACITY_VIP=2
export FERRY_DEPARTURE_INTERVAL=5
export FERRY_TRAVEL_TIME=1
export PASSENGER_SECURITY_TIME_MIN=10
export PASSENGER_SECURITY_TIME_MAX=20
export PASSENGER_BOARDING_TIME=2000  # Slower boarding to stress ramp
export FERRY_GATE_MAX_DELAY=500
export FERRY_BAGGAGE_LIMIT_MIN=40
export FERRY_BAGGAGE_LIMIT_MAX=60
export PASSENGER_BAG_WEIGHT_MIN=5
export PASSENGER_BAG_WEIGHT_MAX=50
export DANGEROUS_ITEM_CHANCE=0
export VIP_CHANCE=0

log_info "Running simulation with capacity stress test..."
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
log_info "Validating capacity constraints..."
echo ""

# Validate ferry capacity was never exceeded
validate_ferry_capacity "$LOG_FILE" "$FERRY_CAPACITY"

# Validate ramp capacity was never exceeded
validate_ramp_capacity "$LOG_FILE" "$RAMP_CAPACITY_REG" "$RAMP_CAPACITY_VIP"

# Count max concurrent passengers on ramp (from log)
max_on_ramp=0
current_on_ramp=0

while IFS= read -r line; do
    if echo "$line" | grep -q "Granting ramp"; then
        ((current_on_ramp++))
        if [ "$current_on_ramp" -gt "$max_on_ramp" ]; then
            max_on_ramp=$current_on_ramp
        fi
    elif echo "$line" | grep -q "left ramp"; then
        ((current_on_ramp--))
    fi
done < <(grep -E "Granting ramp|left ramp" "$LOG_FILE")

log_info "Maximum observed on ramp: $max_on_ramp"
log_info "Configured ramp capacity: $((RAMP_CAPACITY_REG + RAMP_CAPACITY_VIP))"

# Validate statistics
spawned=$(get_stat_passengers_spawned "$LOG_FILE")
boarded=$(get_stat_passengers_boarded "$LOG_FILE")

log_info "Passengers spawned: $spawned"
log_info "Passengers boarded: $boarded"

validate_passenger_accounting "$LOG_FILE"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
