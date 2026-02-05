"""Integration tests for ferry simulation system.

This module contains pytest-based integration tests that verify the correct
behavior of the ferry simulation system by running one simulation and validating
all requirements from the logs.
"""

import os
import sys
import subprocess
import time
import signal
import pytest
from pathlib import Path
from log_parser import LogParser


# Test configuration
SIMULATION_BINARY = "./ferry-simulation"
SIMULATION_LOG = "simulation.log"
SIMULATION_TIMEOUT = 200  # seconds


class TestFerrySimulation:
    """Integration tests that run simulation once and validate all requirements."""
    
    parser = None  # Shared parser instance across all tests
    
    @classmethod
    def setup_class(cls):
        """Run the simulation once before all tests."""
        print("\n" + "="*60)
        print("Running ferry simulation (this may take up to 2 minutes)...")
        print("="*60)
        
        # Clean up any existing log file
        if os.path.exists(SIMULATION_LOG):
            os.remove(SIMULATION_LOG)
        
        # Verify binary exists
        if not os.path.exists(SIMULATION_BINARY):
            pytest.fail(f"Simulation binary not found: {SIMULATION_BINARY}")
        
        if not os.access(SIMULATION_BINARY, os.X_OK):
            pytest.fail(f"Simulation binary is not executable: {SIMULATION_BINARY}")
        
        # Run simulation
        start_time = time.time()
        try:
            result = subprocess.run(
                [SIMULATION_BINARY],
                stderr=subprocess.STDOUT,
                timeout=SIMULATION_TIMEOUT,
                cwd=os.getcwd()
            )
            returncode = result.returncode
        except subprocess.TimeoutExpired:
            pytest.fail(f"Simulation timed out after {SIMULATION_TIMEOUT} seconds")
        except Exception as e:
            pytest.fail(f"Error running simulation: {e}")
        
        duration = time.time() - start_time
        print(f"Simulation completed in {duration:.1f}s with exit code {returncode}")
        
        if returncode != 0:
            pytest.fail(f"Simulation failed with exit code {returncode}")
        
        if not os.path.exists(SIMULATION_LOG):
            pytest.fail(f"Log file was not created: {SIMULATION_LOG}")
        
        # Parse log file once for all tests
        cls.parser = LogParser()
        if not cls.parser.parse_file(SIMULATION_LOG):
            pytest.fail(f"Failed to parse log file: {SIMULATION_LOG}")
        
        print(f"Parsed {len(cls.parser.events)} log events")
        print("="*60 + "\n")
    
    @classmethod
    def teardown_class(cls):
        """Clean up after all tests."""
        # Optionally keep log file for inspection
        # if os.path.exists(SIMULATION_LOG):
        #     os.remove(SIMULATION_LOG)
        pass
    
    def test_log_parsing_sanity(self):
        """Verify log file was parsed and contains events from all roles."""
        assert len(self.parser.events) > 0, "No events parsed from log file"
        
        passenger_count = self.parser.count_by_role("PASSENGER")
        ferry_count = self.parser.count_by_role("FERRY_MANAGER")
        port_count = self.parser.count_by_role("PORT_MANAGER")
        security_count = self.parser.count_by_role("SECURITY_MANAGER")
        
        assert passenger_count > 0, "No passenger events found"
        assert ferry_count > 0, "No ferry manager events found"
        assert port_count > 0, "No port manager events found"
        assert security_count > 0, "No security manager events found"
    
    def test_chronological_order(self):
        """Verify all events are in chronological order."""
        assert self.parser.verify_chronological(), \
            "Events are not in chronological order"
    
    def test_simulation_duration(self):
        """Verify simulation completes within expected time."""
        duration = self.parser.get_duration_seconds()
        assert duration < 300, \
            f"Simulation took too long: {duration:.1f}s (expected < 300s)"
        print(f"  Simulation duration: {duration:.1f}s")
    
    def test_security_capacity_6_max(self):
        """Verify security station capacity is never exceeded (max 6: 3 stations × 2)."""
        assert self.parser.verify_security_capacity(max_capacity=6), \
            "Security capacity constraint violated (max 6 concurrent passengers)"
    
    def test_ramp_capacity_5_max(self):
        """Verify ramp capacity is never exceeded (max 5: 3 regular + 2 VIP)."""
        assert self.parser.verify_ramp_capacity(max_capacity=5), \
            "Ramp capacity constraint violated (max 5 concurrent passengers)"
    
    def test_ferry_capacity_50_max(self):
        """Verify ferry capacity never exceeds 50 passengers per ferry."""
        is_valid, violations = self.parser.validate_ferry_capacity_limit(max_capacity=50)
        
        if not is_valid:
            print("\nFerry capacity violations detected:")
            for violation in violations:
                print(f"  - {violation}")
        
        assert is_valid, f"Ferry capacity constraint violated: {len(violations)} violations found"
    
    def test_ferry_state_transitions(self):
        """Verify ferries follow proper state machine transitions."""
        assert self.parser.verify_ferry_sequence(), \
            "Invalid ferry state transition detected"
    
    def test_port_lifecycle(self):
        """Verify port opens before closing."""
        assert self.parser.verify_port_lifecycle(), \
            "Port lifecycle violation detected (closed before opening)"
    
    def test_gender_rules_at_security(self):
        """Verify security stations follow gender rules (same gender when 2 people present)."""
        is_valid, violations = self.parser.validate_gender_rules()
        
        if not is_valid:
            print("\nGender rule violations detected:")
            for violation in violations:
                print(f"  - {violation}")
        
        # Note: Current implementation has limited gender tracking
        # This test validates what can be checked from available logs
        assert is_valid, f"Gender rule violations: {len(violations)} found"
    
    def test_frustration_max_3_overtakes(self):
        """Verify no passenger exceeds maximum frustration level (max 3 overtakes)."""
        is_valid, violations = self.parser.validate_frustration_limit(max_frustration=3)
        
        if not is_valid:
            print("\nFrustration limit violations detected:")
            for violation in violations:
                print(f"  - {violation}")
        
        assert is_valid, \
            f"Frustration limit exceeded: {len(violations)} passengers had > 3 overtakes"
    
    def test_baggage_weight_enforcement(self):
        """Verify baggage weight enforcement (passengers wait for compatible ferries)."""
        is_valid, violations = self.parser.validate_baggage_enforcement()
        
        if not is_valid:
            print("\nBaggage enforcement violations detected:")
            for violation in violations:
                print(f"  - {violation}")
        
        assert is_valid, \
            f"Baggage enforcement issues: {len(violations)} violations found"
    
    def test_vip_priority_handling(self):
        """Verify VIP passengers are handled correctly."""
        is_valid, violations = self.parser.validate_vip_priority()
        
        if not is_valid:
            print("\nVIP priority violations detected:")
            for violation in violations:
                print(f"  - {violation}")
        
        # Basic sanity check - VIPs should exist and board
        assert is_valid, f"VIP handling issues: {len(violations)} violations found"
        
        # Additional check: VIPs should exist in simulation
        passengers = self.parser.extract_passenger_attributes()
        vip_count = sum(1 for p in passengers.values() if p['vip'])
        print(f"  VIP passengers in simulation: {vip_count}")
        assert vip_count > 0, "No VIP passengers were generated in simulation"
    
    def test_passenger_attributes_extraction(self):
        """Verify passenger attributes can be extracted from logs."""
        passengers = self.parser.extract_passenger_attributes()
        
        assert len(passengers) > 0, "No passenger attributes extracted"
        
        # Verify attributes are present
        for pid, attrs in passengers.items():
            assert 'gender' in attrs, f"Passenger {pid} missing gender"
            assert 'vip' in attrs, f"Passenger {pid} missing VIP status"
            assert 'bag_weight' in attrs, f"Passenger {pid} missing bag weight"
            assert attrs['gender'] in ['MALE', 'FEMALE'], \
                f"Passenger {pid} has invalid gender: {attrs['gender']}"
        
        print(f"  Extracted attributes for {len(passengers)} passengers")
    
    def test_security_events_tracking(self):
        """Verify security events can be tracked."""
        security_events = self.parser.extract_security_events()
        
        assert len(security_events) > 0, "No security events extracted"
        
        assigned_count = sum(1 for e in security_events if e['event_type'] == 'assigned')
        completed_count = sum(1 for e in security_events if e['event_type'] == 'completed')
        
        print(f"  Security events: {assigned_count} assigned, {completed_count} completed")
        assert assigned_count > 0, "No security assignment events found"
        assert completed_count > 0, "No security completion events found"
    
    def test_frustration_tracking(self):
        """Verify frustration events can be tracked."""
        frustration_events = self.parser.extract_frustration_events()
        
        # Frustration may or may not occur depending on passenger timing
        print(f"  Frustration events recorded: {len(frustration_events)}")
        
        if len(frustration_events) > 0:
            max_frustration = max(e['frustration_level'] for e in frustration_events)
            print(f"  Maximum frustration level reached: {max_frustration}")
            assert max_frustration <= 3, \
                f"Frustration exceeded limit: {max_frustration} > 3"
    
    def test_baggage_rejection_tracking(self):
        """Verify baggage rejection events can be tracked."""
        rejections = self.parser.extract_baggage_rejections()
        
        # Rejections may or may not occur depending on random bag weights and ferry limits
        print(f"  Baggage rejection events: {len(rejections)}")
        
        # Verify rejection logic is correct
        for rejection in rejections:
            assert rejection['bag_weight'] >= rejection['ferry_limit'], \
                f"Invalid rejection: bag {rejection['bag_weight']} < limit {rejection['ferry_limit']}"


# Test discovery and execution
if __name__ == "__main__":
    # Allow running tests directly with: python test_integration.py
    pytest.main([__file__, "-v"])
