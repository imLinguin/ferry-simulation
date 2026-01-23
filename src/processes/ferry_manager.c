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

volatile int should_depart = 0;

static void handler(int signal) {
    if (signal == SIGUSR1) should_depart = 1;
}

int main(int argc, char** argv) {
    int ferry_id;
    int log_queue = -1;
    int shm_id;
    int sem_state_mutex;
    int sem_ramp;
    int sem_current_ferry;
    SharedState* shared_state;

    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_ramp_key;
    key_t sem_current_ferry_key;
    key_t log_queue_key;
    
    struct sigaction sa;

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("[FERRY] Failed to setup signal handler SIGUSR1");
        return 1;
    }
    

    if (argc < 3) return 1;
    
    ferry_id = atoi(argv[2]);
    
    // Open IPC resources
    log_queue_key = ftok(argv[1], IPC_KEY_LOG_ID);
    shm_key = ftok(argv[1], IPC_KEY_SHM_ID);
    sem_state_mutex_key = ftok(argv[1], IPC_KEY_SEM_STATE_ID);
    sem_ramp_key = ftok(argv[1], IPC_KEY_SEM_RAMP_ID);
    sem_current_ferry_key = ftok(argv[1], IPC_KEY_SEM_CURRENT_FERRY);
    
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
    
    sem_state_mutex = sem_open(sem_state_mutex_key, 1);
    sem_ramp = sem_open(sem_ramp_key, 1);
    sem_current_ferry = sem_open(sem_current_ferry_key, 1);
    
    if (sem_state_mutex == -1 || sem_ramp == -1 || sem_current_ferry == -1) {
        shm_detach(shared_state);
        return 1;
    }
    
    log_message(log_queue, ROLE, ferry_id, "Ferry manager started");
    log_message(log_queue, ROLE, ferry_id, "Ferry manager waiting for semaphore");
    
    // Ferry main loop: board passengers, depart, travel, return
    while (shared_state->port_open) {
        sem_wait_single(sem_current_ferry, 0);
        sem_wait_single(sem_state_mutex, 0);
        
        log_message(log_queue, ROLE, ferry_id, "Ferry manager updating current ferry state");
        shared_state->current_ferry_id = ferry_id;
        shared_state->ferries[ferry_id].status = FERRY_BOARDING;
        log_message(log_queue, ROLE, ferry_id, "Ferry is now boarding");
        
        sem_signal_single(sem_state_mutex, 0);
        
        // Wait for boarding to complete or early departure signal
        time_t boarding_start = time(NULL);
        should_depart = 0;
        
        while (!should_depart && 
               (time(NULL) - boarding_start) < FERRY_DEPARTURE_INTERVAL &&
               shared_state->port_open) {
            sleep(1);
        }
        should_depart = 0;
        
        // Ensure ramp is empty before departing

        sem_wait_single(sem_state_mutex, 0);
        
        log_message(log_queue, ROLE, ferry_id, "Ferry departing");
        shared_state->current_ferry_id = -1;
        shared_state->ferries[ferry_id].status = FERRY_DEPARTED;
        
        sem_signal_single(sem_state_mutex, 0);
        sem_signal_single(sem_current_ferry, 0);
        
        // Travel
        log_message(log_queue, ROLE, ferry_id, "Ferry traveling");
        sleep(FERRY_TRAVEL_TIME);
        log_message(log_queue, ROLE, ferry_id, "Ferry returning");
        sleep(FERRY_TRAVEL_TIME);
        
        // Return
        sem_wait_single(sem_state_mutex, 0);
        
        shared_state->ferries[ferry_id].status = FERRY_WAITING_IN_QUEUE;
        shared_state->ferries[ferry_id].passenger_count = 0;
        shared_state->ferries[ferry_id].baggage_weight_total = 0;
        
        log_message(log_queue, ROLE, ferry_id, "Ferry returned to queue");
        
        sem_signal_single(sem_state_mutex, 0);
    }
    
    shm_detach(shared_state);
    return 0;
}