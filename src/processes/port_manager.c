#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "common/config.h"
#include "common/state.h"
#include "common/ipc.h"
#include "common/logging.h"
#include "processes/port_manager.h"

#define ROLE ROLE_PORT_MANAGER

void handle_signal(int signal) {}
void handle_sigusr2(int signal) {}

int main(int argc, char** argv) {
    key_t logger_key;
    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_security_key;
    key_t sem_ramp_key;
    
    int log_queue = -1;
    int shm_id;
    int sem_state_mutex;
    int sem_security;
    int sem_ramp;
    SharedState* shared_state;
    
    pid_t passenger_pids[PASSENGER_COUNT];

    if (argc < 2) return 1;

    signal(SIGINT, handle_signal);
    signal(SIGUSR2, handle_sigusr2);

    // Open IPC resources
    logger_key = ftok(argv[1], 'L');
    shm_key = ftok(argv[1], 'S');
    sem_state_mutex_key = ftok(argv[1], 'M');
    sem_security_key = ftok(argv[1], 'E');
    sem_ramp_key = ftok(argv[1], 'R');
    
    if (logger_key != -1) {
        log_queue = queue_open(logger_key);
    }
    
    shm_id = shm_open(shm_key);
    if (shm_id == -1) {
        perror("Port manager: Failed to open shared memory");
        return 1;
    }
    
    shared_state = (SharedState*)shm_attach(shm_id);
    if (shared_state == (void*)-1) {
        perror("Port manager: Failed to attach shared memory");
        return 1;
    }
    
    sem_state_mutex = sem_open(sem_state_mutex_key);
    sem_security = sem_open(sem_security_key);
    sem_ramp = sem_open(sem_ramp_key);
    
    if (sem_state_mutex == -1 || sem_security == -1 || sem_ramp == -1) {
        perror("Port manager: Failed to open semaphores");
        shm_detach(shared_state);
        return 1;
    }

    log_message(log_queue, ROLE, "Port manager starting up");

    
    // Spawn passengers
    for (int i = 0; i < PASSENGER_COUNT; i++) {
        passenger_pids[i] = fork();
        if (passenger_pids[i] == -1) {
            perror("Failed to start passenger");
        } else if (passenger_pids[i] == 0) {
            // execl();
        }
    }
    
    shm_detach(shared_state);

    return 0;
}