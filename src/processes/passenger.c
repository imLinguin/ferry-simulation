#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "common/config.h"
#include "common/state.h"
#include "common/ipc.h"
#include "common/logging.h"
#include "common/macros.h"
#include "processes/passenger.h"

#define ROLE ROLE_PASSENGER

int main(int argc, char** argv) {
    int passenger_id;
    int log_queue = -1;
    int queue_security;
    int sem_state_mutex;
    int sem_security;
    int sem_ramp;
    int shm_id;
    PassengerTicket ticket;
    SecurityMessage security_message;
    SharedState *shm;

    key_t log_queue_key;
    key_t key_security;
    key_t sem_state_mutex_key;
    key_t sem_security_key;
    key_t sem_ramp_key;
    key_t shm_key;
    
    if (argc < 3) return 1;
    
    passenger_id = atoi(argv[2]);
    
    // Open IPC resources
    log_queue_key = ftok(argv[1], IPC_KEY_LOG_ID);
    key_security = ftok(argv[1], IPC_KEY_QUEUE_SECURITY_ID);
    sem_security_key = ftok(argv[1], IPC_KEY_SEM_SECURITY_ID);
    sem_state_mutex_key = ftok(argv[1], IPC_KEY_SEM_STATE_ID);
    sem_ramp_key = ftok(argv[1], IPC_KEY_SEM_RAMP_ID);
    shm_key = ftok(argv[1], IPC_KEY_SHM_ID);
    
    if (log_queue_key != -1) {
        log_queue = queue_open(log_queue_key);
    }
    
    queue_security = queue_open(key_security);
    sem_state_mutex = sem_open(sem_state_mutex_key, 1);
    sem_security = sem_open(sem_security_key, 1);
    sem_ramp = sem_open(sem_ramp_key, 1);
    
    shm_id = shm_open(shm_key);
    shm = shm_attach(shm_id);

    if (sem_state_mutex == -1 || sem_security == -1 || sem_ramp == -1 || log_queue == -1) {
        perror("Failed to init passenger");
        return 1;
    }
    
    // Initialize passenger ticket
    ticket.state = PASSENGER_CHECKIN;
    ticket.gender = (rand() % 1) + 1;
    ticket.vip = (rand() % 10 < 2) ? 1 : 0;
    ticket.bag_weight = PASSENGER_BAG_WEIGHT_MIN + 
                        (rand() % (PASSENGER_BAG_WEIGHT_MAX - PASSENGER_BAG_WEIGHT_MIN + 1));
    
    log_message(log_queue, ROLE, passenger_id, "Passenger created");
    ticket.state = PASSENGER_BAG_CHECK;
    log_message(log_queue, ROLE, passenger_id, "At baggage check");
    
    // Baggage check: is current ferry suitable for us?
    int valid_ferry = -1;
    char msg[255] = "";
    while(1) {
        sem_wait_single(sem_state_mutex, 0);
        if (shm->current_ferry_id != -1) {
            if (shm->ferries[shm->current_ferry_id].baggage_limit > ticket.bag_weight) {
                log_message(log_queue, ROLE, passenger_id, "Baggage meets the limit");
                sem_signal_single(sem_state_mutex, 0);
                break;
            }
            snprintf(msg, 255, "Bag doesnt meet the limit bag: %d of %d", ticket.bag_weight, shm->ferries[shm->current_ferry_id].baggage_limit);
            log_message(log_queue, ROLE, passenger_id, msg);
        }
        sem_signal_single(sem_state_mutex, 0);
        sleep(1);
    }
    shm_detach(shm);
    
    ticket.state = PASSENGER_WAITING;
    log_message(log_queue, ROLE, passenger_id, "Passed baggage check");
    
    // Security check: use semaphore to ensure gender constraint
    int assigned_station = -1;

    log_message(log_queue, ROLE, passenger_id, "Waiting for security");
    sem_wait_single(sem_security, 0);
    security_message.mtype = SECURITY_MESSAGE_MANAGER_ID;
    security_message.gender = ticket.gender;
    security_message.pid = getpid();
    security_message.passenger_id = passenger_id;
    security_message.frustration = 0;
    while(msgsnd(queue_security, &security_message, MSG_SIZE(security_message), 0) == -1) {
        if (errno != EINTR) {
            log_message(log_queue, ROLE, passenger_id, "[ERROR] Failed to put messege to security queue");
            goto cleanup;
        }
    }
    log_message(log_queue, ROLE, passenger_id, "Requested security station allocation");
    while(msgrcv(queue_security, &security_message, MSG_SIZE(security_message), getpid(), 0) == -1) {
        if (errno != EINTR) {
            log_message(log_queue, ROLE, passenger_id, "[ERROR] Failed to get messege from security queue");
            goto cleanup;
        }
    }
    sem_signal_single(sem_security, 0);
    
    ticket.state = PASSENGER_BOARDING;
    log_message(log_queue, ROLE, passenger_id, "Passed security, waiting to board");
    
    // Wait for ramp access
    sem_wait_single(sem_ramp, 0);
    
    log_message(log_queue, ROLE, passenger_id, "Boarding ferry");
    
    // Simulate boarding time
    sleep(PASSENGER_BOARDING_TIME);

    sem_signal_single(sem_ramp, 0);
    
    ticket.state = PASSENGER_BOARDED;
    log_message(log_queue, ROLE, passenger_id, "Boarded successfully");
    
cleanup:
    queue_close(log_queue);
    queue_close(queue_security);
    sem_close(sem_state_mutex);
    sem_close(sem_security);
    sem_close(sem_ramp);
    return 0;
}