#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "common/config.h"
#include "common/state.h"
#include "common/ipc.h"
#include "common/logging.h"
#include "processes/passenger.h"

#define ROLE ROLE_PASSENGER

int main(int argc, char** argv) {
    int passenger_id;
    int log_queue = -1;
    int shm_id;
    int sem_state_mutex;
    int sem_security;
    int sem_ramp;
    SharedState* shared_state;
    PassengerTicket ticket;
    
    key_t log_queue_key;
    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_security_key;
    key_t sem_ramp_key;
    
    if (argc < 3) return 1;
    
    passenger_id = atoi(argv[2]);
    
    // Open IPC resources
    log_queue_key = ftok(argv[1], 'L');
    shm_key = ftok(argv[1], 'S');
    sem_state_mutex_key = ftok(argv[1], 'M');
    sem_security_key = ftok(argv[1], 'E');
    sem_ramp_key = ftok(argv[1], 'R');
    
    if (log_queue_key != -1) {
        log_queue = queue_open(log_queue_key);
    }
    
    shm_id = shm_open(shm_key);
    if (shm_id == -1) {
        return 1;
    }
    
    shared_state = (SharedState*)shm_attach(shm_id);
    if (shared_state == (void*)-1) {
        return 1;
    }
    
    sem_state_mutex = sem_open(sem_state_mutex_key);
    sem_security = sem_open(sem_security_key);
    sem_ramp = sem_open(sem_ramp_key);
    
    if (sem_state_mutex == -1 || sem_security == -1 || sem_ramp == -1) {
        shm_detach(shared_state);
        return 1;
    }
    
    // Initialize passenger ticket
    ticket.state = PASSENGER_CHECKIN;
    ticket.gender = rand() % 2;
    ticket.vip = (rand() % 10 < 2) ? 1 : 0;
    ticket.bag_weight = PASSENGER_BAG_WEIGHT_MIN + 
                        (rand() % (PASSENGER_BAG_WEIGHT_MAX - PASSENGER_BAG_WEIGHT_MIN + 1));
    
    log_message_with_id(log_queue, ROLE, "Passenger created", passenger_id);
    
    // Check-in: wait for port to be open
    while (!shared_state->port_open) {
        sleep(1);
    }
    ticket.state = PASSENGER_BAG_CHECK;
    log_message_with_id(log_queue, ROLE, "At baggage check", passenger_id);
    
    // Baggage check: find a ferry that can fit this baggage
    int valid_ferry = -1;
    sem_wait_single(sem_state_mutex, 0);
    for (int i = 0; i < FERRY_COUNT; i++) {
        if (ticket.bag_weight <= shared_state->ferries[i].baggage_limit) {
            valid_ferry = i;
            break;
        }
    }
    sem_signal_single(sem_state_mutex, 0);
    
    if (valid_ferry == -1) {
        log_message_with_id(log_queue, ROLE, "Baggage too heavy, rejected", passenger_id);
        shm_detach(shared_state);
        return 0;
    }
    
    ticket.state = PASSENGER_WAITING;
    log_message_with_id(log_queue, ROLE, "Passed baggage check", passenger_id);
    
    // Security check: use semaphore to ensure gender constraint
    int assigned_station = -1;
    
    for (int attempt = 0; attempt < 3; attempt++) {
        sem_wait_single(sem_state_mutex, 0);
        
        // Find an available security station with matching gender or empty
        for (int i = 0; i < SECURITY_STATIONS; i++) {
            if (shared_state->stations[i].occupancy < SECURITY_STATION_CAPACITY) {
                if (shared_state->stations[i].occupancy == 0 || 
                    shared_state->stations[i].gender == ticket.gender) {
                    assigned_station = i;
                    break;
                }
            }
        }
        
        sem_signal_single(sem_state_mutex, 0);
        
        if (assigned_station != -1) break;
        sleep(1);
    }
    
    if (assigned_station == -1) {
        log_message_with_id(log_queue, ROLE, "Security check timeout, frustrated", passenger_id);
        shm_detach(shared_state);
        return 1;
    }
    
    log_message_with_id(log_queue, ROLE, "At security station", passenger_id);
    
    // Enter security station
    sem_wait_single(sem_state_mutex, 0);
    shared_state->stations[assigned_station].occupancy++;
    if (shared_state->stations[assigned_station].occupancy == 1) {
        shared_state->stations[assigned_station].gender = ticket.gender;
    }
    sem_signal_single(sem_state_mutex, 0);
    
    // Simulate security check
    sleep(PASSENGER_SECURITY_TIME);
    
    // Exit security station
    sem_wait_single(sem_state_mutex, 0);
    shared_state->stations[assigned_station].occupancy--;
    if (shared_state->stations[assigned_station].occupancy == 0) {
        shared_state->stations[assigned_station].gender = -1;
    }
    sem_signal_single(sem_state_mutex, 0);
    
    ticket.state = PASSENGER_BOARDING;
    log_message_with_id(log_queue, ROLE, "Passed security, waiting to board", passenger_id);
    
    // Wait for ramp access
    sem_wait_single(sem_ramp, 0);
    
    log_message_with_id(log_queue, ROLE, "Boarding ferry", passenger_id);
    
    // Update ferry state
    sem_wait_single(sem_state_mutex, 0);
    shared_state->ramp.occupancy++;
    shared_state->ferries[valid_ferry].passenger_count++;
    shared_state->ferries[valid_ferry].baggage_weight_total += ticket.bag_weight;
    sem_signal_single(sem_state_mutex, 0);
    
    // Simulate boarding time
    sleep(PASSENGER_BOARDING_TIME);
    
    // Release ramp slot
    sem_wait_single(sem_state_mutex, 0);
    shared_state->ramp.occupancy--;
    sem_signal_single(sem_state_mutex, 0);
    
    sem_signal_single(sem_ramp, 0);
    
    ticket.state = PASSENGER_BOARDED;
    log_message_with_id(log_queue, ROLE, "Boarded successfully", passenger_id);
    
    shm_detach(shared_state);
    return 0;
}