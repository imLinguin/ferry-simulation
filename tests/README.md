# Ferry Simulation Test Suite

Comprehensive test suite for validating the ferry simulation system's correctness, concurrency, timing, and edge case handling.

## Test Structure

### Shared Library: `tests.shlib`

Provides reusable utilities for all test scripts:

- **Process Management**: `run_test_with_timeout()` - runs simulation with automatic deadlock detection (kills passenger/ferry-manager processes on timeout)
- **Log Parsing**: Functions to extract ferry trips, passenger counts, events using grep/awk/sed
- **Validation**: Assert functions to validate capacity limits, passenger accounting, timing constraints
- **Test Reporting**: Color-coded output, test summary generation

### Test Scripts

#### Basic Correctness Tests

1. **`test_basic_flow.sh`**
   - Small passenger count (20)
   - Verifies complete passenger journey
   - Validates basic system functionality

2. **`test_ferry_trips.sh`**
   - Runs simulation via `test_ramp.sh` configuration
   - Validates exactly 10 ferry trips with passengers > 0
   - Validates passenger accounting accuracy
   - **Note:** Requires `test_ramp.sh` to exist in tests directory

3. **`test_passenger_accounting.sh`**
   - 100 passengers
   - Validates: spawned = boarded + rejected_baggage + rejected_security
   - Ensures no passengers are lost or double-counted

#### Concurrency Validation Tests

4. **`test_capacity_limits.sh`**
   - 200 passengers, slower boarding (2ms)
   - Validates ferry capacity never exceeded
   - Validates ramp capacity constraints (max concurrent on ramp)
   - Tracks max observed concurrency vs configured limits

5. **`test_security_segregation.sh`**
   - 150 passengers with longer security times (50-100ms)
   - Validates security station throughput
   - Validates passenger accounting and capacity
   - Notes: Gender segregation requires detailed station logs

6. **`test_vip_priority.sh`**
   - 100 passengers, 30% VIP chance
   - Validates VIP passengers get ramp access
   - Verifies VIP/regular passenger separation in logs

#### Signal Handling Tests

7. **`test_early_departure.sh`**
   - Long departure interval (30s)
   - Sends SIGUSR1 to ferry-manager processes after ~8s and ~13s
   - Validates ferries depart before interval expires

8. **`test_port_closure.sh`**
   - 200 passengers
   - Sends SIGUSR2 to port-manager after ~11s
   - Validates graceful shutdown with remaining passengers
   - Ensures not all passengers board (port closed early)

#### Stress and Edge Cases

9. **`test_stress.sh`**
    - 5000 passengers, 10 ferries
    - Fast operations (3-7ms security, 500μs boarding)
    - 10% VIP passengers
    - Validates no deadlocks (120s timeout)
    - Verifies high throughput (4000+ boarded)

10. **`test_edge_cases.sh`**
    - Small capacity (10 passengers per ferry)
    - Short departure interval (may cause empty ferries)
    - Slow security (20-40ms)
    - Validates empty ferry handling
    - Validates exact capacity matching

### Master Test Runner: `test_all.sh`

Executes all test scripts sequentially and provides summary:

```bash
./tests/test_all.sh
```

## Running Tests

### Run All Tests

```bash
cd tests
./test_all.sh
```

### Run Individual Test

```bash
cd tests
./test_basic_flow.sh
./test_stress.sh
# etc.
```

### Run Existing Configuration Test

```bash
cd tests
./test_ramp.sh ../buildDir/ferry-simulation
```

## Test Output

Each test provides:

- **Colored output**: Green for info/pass, red for errors/failures, yellow for warnings
- **Test assertions**: Individual pass/fail for each validation
- **Statistics**: Passenger counts, ferry trips, capacity usage
- **Summary**: Total tests passed/failed

Example output:

```
========================================
Basic Flow Test
========================================
[INFO] Running simulation...
[INFO] Log file exists and has content
[INFO] Validating results...
[INFO] ✓ PASS: Passenger accounting (expected: 20, got: 20)
[INFO] ✓ PASS: Most passengers boarded (16 > 15)
[INFO] ✓ PASS: Ferry capacity never exceeded (15 <= 15)

========================================
Test Summary
========================================
Total tests: 5
Passed: 5
Failed: 0
========================================
All tests passed!
```

## Deadlock Detection

All tests use `run_test_with_timeout()` with automatic process cleanup:

- Default timeout: 60-120 seconds (varies by test)
- On timeout:
  1. Kills passenger processes (SIGTERM, then SIGKILL)
  2. Kills ferry-manager processes (SIGTERM, then SIGKILL)
  3. Kills port-manager process
  4. Returns exit code 124

This prevents hanging tests from blocking the test suite.

## Log Validation

Tests parse `simulation.log` to validate:

- **Ferry trips**: Count departures where `final_passenger_count > 0`
- **Passenger states**: Boarded, rejected at baggage, rejected at security
- **Capacity constraints**: Max passengers per ferry, max concurrent on ramp
- **Timing**: Ferry departure intervals, travel times
- **Errors**: Search for error messages, failures, segfaults

### Key Log Patterns

```
Ferry departing (final_passenger_count: N, baggage_total: M)
Passenger N Boarded successfully
BAGGAGE_REJECTED - bag: X exceeds ferry_limit: Y
Granting ramp to passenger N (VIP: 0/1)
Passenger N left ramp (current_capacity: X/Y)
Gate closing
```

## Configuration Parameters

Tests use environment variables to configure simulation:

```bash
PASSENGER_COUNT=100           # Number of passengers
FERRY_COUNT=3                 # Number of ferries
FERRY_CAPACITY=50            # Max passengers per ferry
RAMP_CAPACITY_REG=3          # Regular ramp slots
RAMP_CAPACITY_VIP=2          # VIP ramp slots
FERRY_DEPARTURE_INTERVAL=10  # Seconds before departure
FERRY_TRAVEL_TIME=1          # Seconds to destination
PASSENGER_SECURITY_TIME_MIN=5    # Milliseconds
PASSENGER_SECURITY_TIME_MAX=10
PASSENGER_BOARDING_TIME=1000     # Microseconds
FERRY_GATE_MAX_DELAY=10000       # Milliseconds
FERRY_BAGGAGE_LIMIT_MIN=50       # Kilograms
FERRY_BAGGAGE_LIMIT_MAX=60
PASSENGER_BAG_WEIGHT_MIN=5
PASSENGER_BAG_WEIGHT_MAX=50
PASSENGER_BAG_WEIGHT_MIN=5
PASSENGER_BAG_WEIGHT_MAX=50
DANGEROUS_ITEM_CHANCE=0           # Percentage (0-100)
VIP_CHANCE=0                      # Percentage (0-100)
```

## Extending the Test Suite

To add a new test:

1. Create `test_<name>.sh` in `tests/` directory
2. Source `tests.shlib` for utilities
3. Set test configuration (environment variables)
4. Run simulation with `run_test_with_timeout()`
5. Validate results using log parsing functions
6. Use assert functions for validation
7. Call `print_test_summary()` at the end
8. Add test to `test_all.sh` array
9. Make executable: `chmod +x test_<name>.sh`

Example template:

```bash
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/buildDir"

source "$SCRIPT_DIR/tests.shlib"

LOG_FILE="simulation.log"
rm -f "$LOG_FILE"

# Set configuration
export PASSENGER_COUNT=50
# ... other config ...

# Run test
run_test_with_timeout 60 "$BUILD_DIR/ferry-simulation"

# Validate
verify_log_exists "$LOG_FILE"
validate_passenger_accounting "$LOG_FILE"
# ... other validations ...

print_test_summary
exit $TESTS_FAILED
```

## Notes

- Tests clean up `simulation.log` between runs
- All tests are standalone and can run independently
- Tests assume build directory is at `../buildDir/`
- Signal tests (SIGUSR1/SIGUSR2) may require manual verification
- VIP and dangerous items features enabled via `VIP_CHANCE` and `DANGEROUS_ITEM_CHANCE`
