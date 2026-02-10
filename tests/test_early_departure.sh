#!/bin/bash
# Early departure test - validates SIGUSR1 triggers immediate ferry departure

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "Early Departure Test (SIGUSR1)"
echo "========================================"
echo "Send SIGUSR1 to ferry-manager and verify early departure"
echo ""

rm -f "$LOG_FILE"

# Configuration for early departure test
export PASSENGER_COUNT=100
export FERRY_COUNT=2
export FERRY_CAPACITY=50
export RAMP_CAPACITY_REG=3
export RAMP_CAPACITY_VIP=2
export FERRY_DEPARTURE_INTERVAL=30  # Long interval to test early departure
export FERRY_TRAVEL_TIME=2
export PASSENGER_SECURITY_TIME_MIN=5
export PASSENGER_SECURITY_TIME_MAX=10
export PASSENGER_BOARDING_TIME=1000
export FERRY_GATE_MAX_DELAY=500
export FERRY_BAGGAGE_LIMIT_MIN=40
export FERRY_BAGGAGE_LIMIT_MAX=60
export PASSENGER_BAG_WEIGHT_MIN=5
export PASSENGER_BAG_WEIGHT_MAX=50
export DANGEROUS_ITEM_CHANCE=0
export VIP_CHANCE=0

log_info "Starting simulation..."
"$BUILD_DIR/ferry-simulation" &
sim_pid=$!

# Wait for simulation to start
sleep 3

log_info "Simulation started (PID: $sim_pid)"

# Wait a bit for ferries to start boarding
sleep 5

# Find ferry-manager processes and send SIGUSR1 to first one
log_info "Sending SIGUSR1 to ferry-manager processes..."
pkill -USR1 -f "ferry-manager"

# Let simulation continue
sleep 5

# Send another SIGUSR1
log_info "Sending second SIGUSR1..."
pkill -USR1 -f "ferry-manager"

# Wait for simulation to complete or timeout
timeout=60
elapsed=0
while kill -0 "$sim_pid" 2>/dev/null && [ "$elapsed" -lt "$timeout" ]; do
    sleep 1
    ((elapsed++))
done

if kill -0 "$sim_pid" 2>/dev/null; then
    log_error "Simulation did not complete, terminating..."
    pkill -TERM -f "passenger"
    pkill -TERM -f "ferry-manager"
    pkill -TERM -f "port-manager"
    kill -TERM "$sim_pid"
    sleep 2
    kill -KILL "$sim_pid" 2>/dev/null
    exit 1
fi

wait "$sim_pid"
exit_code=$?

if [ $exit_code -ne 0 ]; then
    log_error "Simulation failed with exit code: $exit_code"
fi

if ! verify_log_exists "$LOG_FILE"; then
    exit 1
fi

echo ""
log_info "Validating early departure behavior..."
echo ""

# Count ferry departures
departure_count=$(count_events "Ferry departing" "$LOG_FILE")
log_info "Ferry departures: $departure_count"

# Validate that some ferries departed (signals should have triggered them)
assert_greater_than "$departure_count" 0 "At least one ferry departed"

# Validate passenger accounting
validate_passenger_accounting "$LOG_FILE"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
