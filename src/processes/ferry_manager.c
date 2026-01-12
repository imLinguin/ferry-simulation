#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "common/config.h"
#include "common/state.h"
#include "common/ipc.h"
#include "common/logging.h"
#include "processes/ferry_manager.h"

#define ROLE ROLE_FERRY_MANAGER

int ferry_id;
int log_queue;
int shm_id;
int sem_state_mutex;
int sem_ramp;
SharedState* shared_state;

volatile int should_depart = 0;

void handle_sigusr1(int signal) {
    should_depart = 1;
}

int main(int argc, char** argv) {
    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_ramp_key;
    key_t log_queue_key;
    
    if (argc < 3) return 1;
    
    ferry_id = atoi(argv[2]);
    
    // Open IPC resources
    log_queue_key = ftok(argv[1], 'L');
    shm_key = ftok(argv[1], 'S');
    sem_state_mutex_key = ftok(argv[1], 'M');
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
    sem_ramp = sem_open(sem_ramp_key);
    
    if (sem_state_mutex == -1 || sem_ramp == -1) {
        shm_detach(shared_state);
        return 1;
    }
    
    signal(SIGUSR1, handle_sigusr1);
    
    log_message_with_id(log_queue, ROLE, "Ferry manager started", ferry_id);
    
    // Ferry main loop: board passengers, depart, travel, return
    time_t last_departure = time(NULL);
    
    while (shared_state->port_open) {
        // Wait until it's this ferry's turn to board
        sem_wait_single(sem_state_mutex, 0);
        
        if (shared_state->ferries[ferry_id].status != FERRY_BOARDING) {
            sem_signal_single(sem_state_mutex, 0);
            sleep(1);
            continue;
        }
        
        // Set ferry to boarding state
        shared_state->ferries[ferry_id].status = FERRY_BOARDING;
        log_message_with_id(log_queue, ROLE, "Ferry is now boarding", ferry_id);
        
        sem_signal_single(sem_state_mutex, 0);
        
        // Wait for boarding to complete or early departure signal
        time_t boarding_start = time(NULL);
        should_depart = 0;
        
        while (!should_depart && 
               (time(NULL) - boarding_start) < FERRY_DEPARTURE_INTERVAL &&
               shared_state->port_open) {
            sleep(1);
        }
        
        // Ensure ramp is empty before departing
        sem_wait_single(sem_state_mutex, 0);
        
        log_message_with_id(log_queue, ROLE, "Ferry departing", ferry_id);
        shared_state->ferries[ferry_id].status = FERRY_DEPARTED;
        shared_state->ramp.active_ferry_id = -1;
        
        sem_signal_single(sem_state_mutex, 0);
        
        // Travel
        log_message_with_id(log_queue, ROLE, "Ferry traveling", ferry_id);
        sleep(FERRY_TRAVEL_TIME);
        
        // Return
        sem_wait_single(sem_state_mutex, 0);
        
        shared_state->ferries[ferry_id].status = FERRY_WAITING_IN_QUEUE;
        shared_state->ferries[ferry_id].passenger_count = 0;
        shared_state->ferries[ferry_id].baggage_weight_total = 0;
        
        log_message_with_id(log_queue, ROLE, "Ferry returned to queue", ferry_id);
        
        sem_signal_single(sem_state_mutex, 0);
        
        last_departure = time(NULL);
    }
    
    shm_detach(shared_state);
    return 0;
}