#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "common/config.h"
#include "common/state.h"
#include "common/ipc.h"
#include "common/logging.h"
#include "common/messages.h"
#include "common/macros.h"
#include "processes/ferry_manager.h"

#define ROLE ROLE_FERRY_MANAGER

volatile int should_depart = 0;
volatile int is_active = 0;

static void handler(int signal) {
    if (is_active && signal == SIGUSR1) should_depart = 1;
}

int main(int argc, char** argv) {
    int ferry_id;
    int log_queue = -1;
    int queue_ramp;
    int shm_id;
    int sem_state_mutex;
    int sem_current_ferry;
    int sem_ramp_slots;
    SharedState* shared_state;

    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t key_ramp;
    key_t sem_current_ferry_key;
    key_t sem_ramp_slots_key;
    key_t log_queue_key;
    
    struct sigaction sa;
    srand(time(NULL) ^ getpid());

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("[FERRY] Failed to setup signal handler SIGUSR1");
        return 1;
    }
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("[FERRY] Failed to setup signal handler SIGUSR2");
        return 1;
    }
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("[FERRY] Failed to setup signal handler SIGINT");
        return 1;
    }

    if (argc < 3) return 1;
    
    ferry_id = atoi(argv[2]);
    
    // Open IPC resources
    log_queue_key = ftok(argv[1], IPC_KEY_LOG_ID);
    shm_key = ftok(argv[1], IPC_KEY_SHM_ID);
    sem_state_mutex_key = ftok(argv[1], IPC_KEY_SEM_STATE_ID);
    key_ramp = ftok(argv[1], IPC_KEY_QUEUE_RAMP_ID);
    sem_current_ferry_key = ftok(argv[1], IPC_KEY_SEM_CURRENT_FERRY);
    sem_ramp_slots_key = ftok(argv[1], IPC_KEY_SEM_RAMP_SLOTS_ID);
    
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
    
    queue_ramp = queue_open(key_ramp);
    sem_state_mutex = sem_open(sem_state_mutex_key, SEM_STATE_MUTEX_VARIANT_COUNT);
    sem_current_ferry = sem_open(sem_current_ferry_key, 1);
    sem_ramp_slots = sem_open(sem_ramp_slots_key, 2);
    
    if (sem_state_mutex == -1 || sem_current_ferry == -1 || queue_ramp == -1 || sem_ramp_slots == -1) {
        shm_detach(shared_state);
        return 1;
    }
    
    log_message(log_queue, ROLE, ferry_id, "Ferry manager started");

    // Ferry main loop: board passengers, depart, travel, return
    while (1) {
        if (!shared_state->port_open) break;
        log_message(log_queue, ROLE, ferry_id, "Ferry manager waiting for semaphore");
        START_SEMAPHORE(sem_current_ferry, 0)
        is_active = 1;

        if (!shared_state->port_open) {
            log_message(log_queue, ROLE, ferry_id, "Ferry manager - port is closed");
            sem_signal_single(sem_current_ferry,0);
            break;
        }

        START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY);
        log_message(log_queue, ROLE, ferry_id, "Ferry manager updating current ferry state");
        shared_state->current_ferry_id = ferry_id;
        END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY);

        START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
        shared_state->ferries[ferry_id].status = FERRY_BOARDING;
        shared_state->ferries[ferry_id].baggage_weight_total = 0;
        shared_state->ferries[ferry_id].passenger_count = 0;
        log_message(log_queue, ROLE, ferry_id, "Ferry is preparing for boarding (baggage_limit: %d, capacity: %d)",
                    shared_state->ferries[ferry_id].baggage_limit, FERRY_CAPACITY);
        END_SEMAPHORE(sem_state_mutex,SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);

        // Open gate for boarding
        time_t boarding_delay_start = time(NULL);
        int boarding_delay = rand() % FERRY_GATE_MAX_DELAY;
        log_message(log_queue, ROLE, ferry_id, "Ferry gate will open in %d s", boarding_delay);
        while (time(NULL) - boarding_delay_start < boarding_delay) {
            usleep(10 * 1000);
        }
        log_message(log_queue, ROLE, ferry_id, "Ferry is open for boarding");
        sem_set_noundo(sem_ramp_slots, 0, RAMP_CAPACITY_REG);
        sem_set_noundo(sem_ramp_slots, 1, RAMP_CAPACITY_VIP);

        // Wait for boarding to complete or early departure signal
        time_t boarding_start = time(NULL);
        should_depart = 0;
        int usage = 0;

        // Handle ramp queue while active
        while (1) {
            int gate_close;
            RampMessage ramp_msg;

            gate_close = should_depart ||
               (time(NULL) - boarding_start) >= FERRY_DEPARTURE_INTERVAL;

            // Process ramp queue: -RAMP_PRIORITY_REGULAR means receive exit(1), VIP(2), or regular(3) - VIP has priority
            if (shared_state->ferries[ferry_id].passenger_count < FERRY_CAPACITY && msgrcv(queue_ramp, &ramp_msg, MSG_SIZE(ramp_msg), -RAMP_PRIORITY_REGULAR, IPC_NOWAIT) != -1) {
                if (ramp_msg.mtype == RAMP_MESSAGE_EXIT) {
                    // Passenger leaving ramp
                    if (!gate_close) sem_signal_single_noundo(sem_ramp_slots, ramp_msg.is_vip); // Release semaphore slot
                    usage--;
                    int current_count;
                    START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
                    shared_state->ferries[ferry_id].passenger_count++;
                    shared_state->ferries[ferry_id].baggage_weight_total += ramp_msg.weight;
                    current_count = shared_state->ferries[ferry_id].passenger_count;
                    END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
                    log_message(log_queue, ROLE, ferry_id, "Passenger %d left ramp (current_capacity: %d/%d)",
                                ramp_msg.passenger_id, current_count, FERRY_CAPACITY);
                } else {
                    // Grant ramp access
                    log_message(log_queue, ROLE, ferry_id, "Granting ramp to passenger %d (VIP: %d)",
                                ramp_msg.passenger_id, ramp_msg.mtype == RAMP_PRIORITY_VIP);
                    ramp_msg.mtype = ramp_msg.pid;  // Response to specific passenger
                    msgsnd(queue_ramp, &ramp_msg, MSG_SIZE(ramp_msg), 0);
                    usage++;
                }
            }

            int semval = sem_get_val(sem_ramp_slots, 0);
            // Ensure ramp is empty before departing
            if (gate_close && !usage) {
                log_message(log_queue, ROLE, ferry_id, "Sem usage on gate close: %d", semval);
                break;
            }
            
            usleep(10000); // 10ms sleep to avoid busy waiting
        }
        log_message(log_queue, ROLE, ferry_id, "Gate closing");
        START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY);
        log_message(log_queue, ROLE, ferry_id, "Ferry departing (final_passenger_count: %d, baggage_total: %d)",
                    shared_state->ferries[ferry_id].passenger_count,
                    shared_state->ferries[ferry_id].baggage_weight_total);
        shared_state->current_ferry_id = -1;
        END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY);

        START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
        shared_state->ferries[ferry_id].status = FERRY_DEPARTED;
        END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);

        END_SEMAPHORE(sem_current_ferry, 0);
        is_active = 0;

        if (!shared_state->ferries[ferry_id].passenger_count && !shared_state->port_open) {
            log_message(log_queue, ROLE, ferry_id, "Ferry departure - empty");
            break;
        }

        // Travel
        log_message(log_queue, ROLE, ferry_id, "Ferry traveling");
        time_t travel_start = time(NULL);
        while ((time(NULL) - travel_start) < FERRY_TRAVEL_TIME) {
            sleep(1);
            log_message(log_queue, ROLE, ferry_id, "Traveling: time left: %02d s", FERRY_TRAVEL_TIME - (time(NULL) - travel_start));
        }
        log_message(log_queue, ROLE, ferry_id, "Ferry returning");
        travel_start = time(NULL);
        while ((time(NULL) - travel_start) < FERRY_TRAVEL_TIME) {
            sleep(1);
            log_message(log_queue, ROLE, ferry_id, "Returning: time left: %02d s", FERRY_TRAVEL_TIME - (time(NULL) - travel_start));
        }
        
        // Return
        START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
        
        shared_state->ferries[ferry_id].status = FERRY_WAITING_IN_QUEUE;
        shared_state->ferries[ferry_id].passenger_count = 0;
        shared_state->ferries[ferry_id].baggage_weight_total = 0;
        
        log_message(log_queue, ROLE, ferry_id, "Ferry returned to queue");
        
        END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
    }
    log_message(log_queue, ROLE, ferry_id, "Ferry exiting");
    shm_detach(shared_state);
    return 0;
}