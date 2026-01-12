#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <string.h>
#include <sys/wait.h>

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

    // Derive paths from argv[1] (which contains the path from main's argv[0])
    char* bin_dir = dirname(strdup(argv[1]));
    char ferry_manager_path[255];
    char passenger_path[255];
    char ferry_id_arg[16];
    char passenger_id_arg[16];
    
    snprintf(ferry_manager_path, sizeof(ferry_manager_path), "%s/ferry-manager", bin_dir);
    snprintf(passenger_path, sizeof(passenger_path), "%s/passenger", bin_dir);
    
    pid_t ferry_pids[FERRY_COUNT];
    pid_t passenger_pids[PASSENGER_COUNT];
    
    // Spawn ferry managers
    for (int i = 0; i < FERRY_COUNT; i++) {
        snprintf(ferry_id_arg, sizeof(ferry_id_arg), "%d", i);
        ferry_pids[i] = fork();
        if (ferry_pids[i] == -1) {
            perror("Failed to spawn ferry manager");
        } else if (ferry_pids[i] == 0) {
            if (execl(ferry_manager_path, ferry_manager_path, argv[1], ferry_id_arg, NULL) == -1) {
                perror("Failed to exec ferry manager");
            }
            return 1;
        }
    }
    
    // Spawn passengers
    for (int i = 0; i < PASSENGER_COUNT; i++) {
        snprintf(passenger_id_arg, sizeof(passenger_id_arg), "%d", i);
        passenger_pids[i] = fork();
        if (passenger_pids[i] == -1) {
            perror("Failed to spawn passenger");
        } else if (passenger_pids[i] == 0) {
            if (execl(passenger_path, passenger_path, argv[1], passenger_id_arg, NULL) == -1) {
                perror("Failed to exec passenger");
            }
            return 1;
        }
    }
    
    log_message_with_id(log_queue, ROLE, "Spawned all ferries and passengers", 0);
    
    // Ferry queue management: ensure only one ferry boards at a time
    int current_boarding_ferry = 0;
    sem_wait_single(sem_state_mutex, 0);
    shared_state->ferries[current_boarding_ferry].status = FERRY_BOARDING;
    shared_state->ramp.active_ferry_id = current_boarding_ferry;
    sem_signal_single(sem_state_mutex, 0);
    
    while (shared_state->port_open) {
        sem_wait_single(sem_state_mutex, 0);
        
        // Check if current ferry has departed
        if (shared_state->ferries[current_boarding_ferry].status == FERRY_WAITING_IN_QUEUE ||
            shared_state->ferries[current_boarding_ferry].status == FERRY_DEPARTED) {
            
            // Move to next ferry
            current_boarding_ferry = (current_boarding_ferry + 1) % FERRY_COUNT;
            
            // Set next ferry to boarding
            if (shared_state->ferries[current_boarding_ferry].status == FERRY_WAITING_IN_QUEUE) {
                shared_state->ferries[current_boarding_ferry].status = FERRY_BOARDING;
                shared_state->ramp.active_ferry_id = current_boarding_ferry;
                log_message_with_id(log_queue, ROLE, "Ferry authorized to board", current_boarding_ferry);
            }
        }
        
        sem_signal_single(sem_state_mutex, 0);
        sleep(1);
    }
    
    // Wait for all ferry managers and passengers
    for (int i = 0; i < FERRY_COUNT; i++) {
        waitpid(ferry_pids[i], NULL, 0);
    }
    for (int i = 0; i < PASSENGER_COUNT; i++) {
        waitpid(passenger_pids[i], NULL, 0);
    }
    
    shm_detach(shared_state);

    return 0;
}