#!/bin/bash
# Port closure test - validates SIGUSR2 prevents new check-ins and ensures graceful shutdown

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"

echo "========================================"
echo "Port Closure Test (SIGUSR2)"
echo "========================================"
echo "Send SIGUSR2 to port-manager and verify graceful closure"
echo ""

rm -f "$LOG_FILE"

# Configuration for port closure test
export PASSENGER_COUNT=200  # More passengers than can board quickly
export FERRY_COUNT=3
export FERRY_CAPACITY=40
export RAMP_CAPACITY_REG=3
export RAMP_CAPACITY_VIP=2
export FERRY_DEPARTURE_INTERVAL=10
export FERRY_TRAVEL_TIME=2
export PASSENGER_SECURITY_TIME_MIN=10
export PASSENGER_SECURITY_TIME_MAX=20
export PASSENGER_BOARDING_TIME=2000
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

# Wait for some passengers to spawn
sleep 8

# Send SIGUSR2 to close the port
log_info "Sending SIGUSR2 to port-manager (closing port)..."
pkill -USR2 -f "port-manager"

log_info "Port closure signal sent, waiting for graceful shutdown..."

# Wait for simulation to complete or timeout
timeout=90
elapsed=0
while kill -0 "$sim_pid" 2>/dev/null && [ "$elapsed" -lt "$timeout" ]; do
    sleep 1
    ((elapsed++))
done

if kill -0 "$sim_pid" 2>/dev/null; then
    log_error "Simulation did not complete gracefully, terminating..."
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

log_info "Simulation completed with exit code: $exit_code"

if ! verify_log_exists "$LOG_FILE"; then
    exit 1
fi

echo ""
log_info "Validating port closure behavior..."
echo ""

spawned=$(get_stat_passengers_spawned "$LOG_FILE")
boarded=$(get_stat_passengers_boarded "$LOG_FILE")

log_info "Passengers spawned before closure: $spawned"
log_info "Passengers boarded: $boarded"

# Not all passengers should have boarded (port closed early)
if [ "$boarded" -lt "$spawned" ]; then
    log_info "✓ Port closure prevented some passengers from boarding ($boarded < $spawned)"
    ((TESTS_PASSED++))
else
    log_warning "All passengers boarded (port may not have closed early enough)"
fi

# Validate passenger accounting
validate_passenger_accounting "$LOG_FILE"

# Validate capacity constraints
validate_ferry_capacity "$LOG_FILE" "$FERRY_CAPACITY"

# Check for errors
check_for_errors "$LOG_FILE"

print_test_summary
exit $TESTS_FAILED
