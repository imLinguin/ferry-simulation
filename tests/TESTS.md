# Ferry Simulation Test Suite - Quick Reference

## Running Tests

```bash
# Run all tests
cd tests && ./test_all.sh

# Run specific test
cd tests && ./test_basic_flow.sh
```

## Test Scripts Summary

| Test Script | Purpose | Passengers | Key Validation |
|------------|---------|------------|----------------|
| `test_basic_flow.sh` | Basic functionality | 20 | Complete journey, all passengers accounted |
| `test_ferry_trips.sh` | Ferry trip count | via test_ramp.sh | Exactly 10 trips with passengers > 0 |
| `test_passenger_accounting.sh` | Accounting accuracy | 100 | spawned = boarded + rejected |
| `test_capacity_limits.sh` | Capacity constraints | 200 | Ferry/ramp capacity never exceeded |
| `test_security_segregation.sh` | Security throughput | 150 | Security station handling |
| `test_vip_priority.sh` | VIP handling | 100 | VIP passengers get priority |
| `test_early_departure.sh` | SIGUSR1 signal | 100 | Early departure on signal (~8s, ~13s) |
| `test_port_closure.sh` | SIGUSR2 signal | 200 | Graceful port closure (~11s) |
| `test_stress.sh` | High load | 5000 | No deadlocks, high throughput |
| `test_edge_cases.sh` | Boundary conditions | 50 | Empty ferries, exact capacity |

## Key Features

✅ **Deadlock Detection**: 60-120s timeout with automatic process cleanup  
✅ **Log Validation**: Parses simulation.log using grep/awk/sed  
✅ **Capacity Validation**: Verifies ferry/ramp limits never exceeded  
✅ **Passenger Accounting**: Ensures all passengers accounted for  
✅ **Signal Testing**: SIGUSR1 (early departure), SIGUSR2 (port closure)  
✅ **Stress Testing**: 5000+ passengers without deadlock  
✅ **Edge Cases**: Empty ferries, exact limits, boundary conditions  

## Test Library (tests.shlib)

Key functions:
- `run_test_with_timeout()` - Run with deadlock detection
- `count_ferry_trips()` - Count trips with passengers > 0
- `validate_passenger_accounting()` - Verify all passengers accounted
- `validate_ferry_capacity()` - Ensure capacity not exceeded
- `validate_ramp_capacity()` - Track max concurrent on ramp
- `validate_ramp_empty_on_departure()` - Verify ramp clear before departure
- `check_for_errors()` - Search for errors in log
- `assert_equals()`, `assert_greater_than()` - Test assertions

## Expected Test Results

All tests should pass with the current implementation:
- ✓ All passengers accounted for (no lost passengers)
- ✓ Capacity constraints never violated
- ✓ No deadlocks detected (completes within timeout)
- ✓ Graceful handling of signals
- ✓ Proper statistics in final output

## Troubleshooting

**Test times out**: Possible deadlock in simulation  
**Passenger accounting fails**: Lost passengers or incorrect statistics  
**Capacity validation fails**: Race condition in capacity enforcement  
**Log file missing**: Simulation crashed before creating log  

See [tests/README.md](README.md) for full documentation.
