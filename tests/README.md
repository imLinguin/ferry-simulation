# Ferry Simulation Test Suite

Full test documentation has been moved to the main [README.md](../README.md#testing).

## Quick Start

All test scripts accept an optional path to the simulation binary (defaults to `../buildDir/ferry-simulation`).

```bash
# Run all tests (default binary)
cd tests && ./test_all.sh

# Run all tests with custom binary
cd tests && ./test_all.sh /path/to/ferry-simulation

# Run individual test
cd tests && ./test_basic_flow.sh
cd tests && ./test_stress.sh ../buildDir/ferry-simulation
```

## Notes

- Tests clean up `simulation.log` between runs
- All tests are standalone and can run independently
- Tests default to `../buildDir/ferry-simulation` but accept a custom binary path as the first argument
- Signal tests (SIGUSR1/SIGUSR2) may require manual verification
- VIP and dangerous items features enabled via `VIP_CHANCE` and `DANGEROUS_ITEM_CHANCE`

## Configuration Parameters

Tests use environment variables to configure simulation:

```bash
PASSENGER_COUNT=100           # Number of passengers
FERRY_COUNT=3                 # Number of ferries
FERRY_CAPACITY=50             # Max passengers per ferry
RAMP_CAPACITY_REG=3           # Regular ramp slots
RAMP_CAPACITY_VIP=2           # VIP ramp slots
FERRY_DEPARTURE_INTERVAL=10   # Seconds before departure
FERRY_TRAVEL_TIME=1           # Seconds to destination
PASSENGER_SECURITY_TIME_MIN=5     # Milliseconds
PASSENGER_SECURITY_TIME_MAX=10
PASSENGER_BOARDING_TIME=1000      # Microseconds
FERRY_GATE_MAX_DELAY=10000        # Milliseconds
FERRY_BAGGAGE_LIMIT_MIN=50        # Kilograms
FERRY_BAGGAGE_LIMIT_MAX=60
PASSENGER_BAG_WEIGHT_MIN=5
PASSENGER_BAG_WEIGHT_MAX=50
DANGEROUS_ITEM_CHANCE=0           # Percentage (0-100)
VIP_CHANCE=0                      # Percentage (0-100)
```
