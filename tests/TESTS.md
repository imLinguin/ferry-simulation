# Ferry Simulation Test Suite - Quick Reference

See the main [README.md](../README.md#testing) for full test documentation, including test script details, shared library functions, and troubleshooting.

## Running Tests

All scripts accept an optional binary path (defaults to `../buildDir/ferry-simulation`).

```bash
# Run all tests
cd tests && ./test_all.sh

# With custom binary
cd tests && ./test_all.sh /path/to/ferry-simulation

# Run specific test
cd tests && ./test_basic_flow.sh
```

## Test Scripts

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
