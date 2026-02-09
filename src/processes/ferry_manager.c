#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

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

/**
 * Signal handler for ferry manager.
 * SIGUSR1: Triggers early departure when the ferry is active and boarding.
 */
static void handler(int signal) {
    if (is_active && signal == SIGUSR1) should_depart = 1;
}

/**
 * Ferry Manager Process Entry Point.
 * 
 * Manages a single ferry throughout its lifecycle:
 * 1. Waits for turn to dock
 * 2. Opens boarding gate and processes passengers from the ramp queue
 * 3. Handles early departure signals or waits for departure interval
 * 4. Departs with passengers, travels, and returns
 * 5. Repeats until the port closes
 * 
 * @param argc Argument count (expects at least 3)
 * @param argv Arguments: [0]=program name, [1]=IPC key path, [2]=ferry ID
 */
int main(int argc, char** argv) {
    int ferry_id;
    int log_queue = -1;
    int queue_ramp;
    int shm_id;
    int sem_state_mutex;
    int sem_current_ferry;
    int sem_ramp_slots;
    int had_passengers = 0;

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
    
    // Initialize IPC resources: queues, shared memory, and semaphores
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

    // Ferry main loop: wait for turn, board passengers, depart, travel, and return
    while (1) {
        if (!shared_state->port_open) break;
        log_message(log_queue, ROLE, ferry_id, "Ferry manager waiting for semaphore");
        // Wait for turn to become the active ferry at the dock
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

        // Initialize ferry state for boarding
        START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
        shared_state->ferries[ferry_id].status = FERRY_BOARDING;
        shared_state->ferries[ferry_id].baggage_weight_total = 0;
        shared_state->ferries[ferry_id].passenger_count = 0;
        log_message(log_queue, ROLE, ferry_id, "Ferry is preparing for boarding (baggage_limit: %d, capacity: %d)",
                    shared_state->ferries[ferry_id].baggage_limit, FERRY_CAPACITY);
        END_SEMAPHORE(sem_state_mutex,SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);

        // Simulate gate opening delay, then open ramp slots for passenger boarding
        int boarding_delay = rand() % FERRY_GATE_MAX_DELAY;
        log_message(log_queue, ROLE, ferry_id, "Ferry gate will open in %d ms", boarding_delay);
        while (usleep(boarding_delay) == -1) {}
        log_message(log_queue, ROLE, ferry_id, "Ferry is open for boarding");
        sem_signal_noundo(sem_ramp_slots, 0, RAMP_CAPACITY_REG);
        sem_signal_noundo(sem_ramp_slots, 1, RAMP_CAPACITY_VIP);

        // Process boarding: handle ramp queue until departure time or early signal
        time_t boarding_start = time(NULL);
        should_depart = 0;
        int usage = 0;
        int ramp_cleanup = 0;
        int ramp_empty = 0;

        // Process ramp messages: grant access to passengers or handle passenger boarding exits
        while (1) {
            int gate_close;
            RampMessage ramp_msg;
            gate_close = should_depart ||
               (time(NULL) - boarding_start) >= FERRY_DEPARTURE_INTERVAL;

            // Process ramp queue: -RAMP_PRIORITY_REGULAR means receive exit(1), VIP(2), or regular(3) - VIP has priority
            if (msgrcv(queue_ramp, &ramp_msg, MSG_SIZE(ramp_msg), -RAMP_PRIORITY_REGULAR, IPC_NOWAIT) != -1) {
                ramp_empty = 0;
                if (ramp_msg.mtype == RAMP_MESSAGE_EXIT) {
                    // Passenger completed boarding and is leaving the ramp area
                    if (!gate_close && !ramp_cleanup && ((FERRY_CAPACITY - shared_state->ferries[ferry_id].passenger_count) > usage)) sem_signal_single_noundo(sem_ramp_slots, ramp_msg.is_vip); // Release semaphore slot
                    usage--;
                    int current_count;
                    START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
                    shared_state->ferries[ferry_id].passenger_count++;
                    shared_state->ferries[ferry_id].baggage_weight_total += ramp_msg.weight;
                    current_count = shared_state->ferries[ferry_id].passenger_count;
                    END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
                    
                    // Update boarded statistics
                    START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_STATS);
                    shared_state->stats.passengers_boarded++;
                    END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_STATS);
                    
                    log_message(log_queue, ROLE, ferry_id, "Passenger %d left ramp (current_capacity: %d/%d)",
                                ramp_msg.passenger_id, current_count, FERRY_CAPACITY);
                } else {
                    int available_space = FERRY_CAPACITY - shared_state->ferries[ferry_id].passenger_count - usage;

                    if (available_space > 0 && !gate_close) {
                        // Grant ramp access to waiting passenger
                        log_message(log_queue, ROLE, ferry_id, "Granting ramp to passenger %d (VIP: %d)",
                                    ramp_msg.passenger_id, ramp_msg.mtype == RAMP_PRIORITY_VIP);
                        ramp_msg.approved = 1;
                        usage++;
                    } else {
                        log_message(log_queue, ROLE, ferry_id, "Rejecting passenger %d - ferry full or gate closing (capacity: %d/%d, on_ramp: %d)",
                            ramp_msg.passenger_id, shared_state->ferries[ferry_id].passenger_count, 
                            FERRY_CAPACITY, usage);
                        ramp_msg.approved = 0;
                    }
                    // Response to specific passenger
                    ramp_msg.mtype = ramp_msg.pid;
                    msgsnd(queue_ramp, &ramp_msg, MSG_SIZE(ramp_msg), 0);
                }
            } else if (errno == ENOMSG) {
                ramp_empty = 1;
            }

            // Wait until all passengers on ramp have boarded before departing
            if (gate_close && !usage && ramp_empty) {
                struct sembuf op = {0, -1, IPC_NOWAIT};
                while (semop(sem_ramp_slots, &op, 1) != -1 || errno == EINTR) {}
                op.sem_num = 1;
                while (semop(sem_ramp_slots, &op, 1) != -1 || errno == EINTR) {}

                int semval_n = sem_get_val(sem_ramp_slots, 0);
                int semval_v = sem_get_val(sem_ramp_slots, 1);
                log_message(log_queue, ROLE, ferry_id, "Sem usage on gate close: %d and %d", semval_n, semval_v);
                if ((semval_n + semval_v) == 0) break;
                ramp_cleanup = 1;
            }
            usleep(1000); // 1ms sleep to avoid busy waiting
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

        // Ferry travel cycle: depart, travel to destination, and return
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
        
        // Update ferry state to indicate it's back in queue and ready for next trip
        START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
        
        shared_state->ferries[ferry_id].status = FERRY_WAITING_IN_QUEUE;
        had_passengers = shared_state->ferries[ferry_id].passenger_count;
        shared_state->ferries[ferry_id].passenger_count = 0;
        shared_state->ferries[ferry_id].baggage_weight_total = 0;
        
        END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
        
        // Update trip statistics if ferry had passengers
        if (had_passengers > 0) {
            START_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_STATS);
            shared_state->stats.total_ferry_trips++;
            END_SEMAPHORE(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_STATS);
        }
        
        log_message(log_queue, ROLE, ferry_id, "Ferry returned to queue");
    }
    log_message(log_queue, ROLE, ferry_id, "Ferry exiting");
    shm_detach(shared_state);
    return 0;
}