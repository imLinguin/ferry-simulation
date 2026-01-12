#include <stdio.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#include "processes/main.h"
#include "common/config.h"
#include "common/state.h"
#include "common/logging.h"
#include "common/ipc.h"
#include <stdlib.h>

void handle_signal(int signal) {}

int main(int argc, char **argv) {
    char* bin_dir;
    pid_t manager_pid;
    pid_t logger_pid;
    
    key_t control_queue;
    key_t log_queue_key;
    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_security_key;
    key_t sem_ramp_key;
    
    int shm_id;
    int sem_state_mutex;
    int sem_security;
    int sem_ramp;
    SharedState* shared_state;

    char port_manager_path[255];
    char ferry_manager_path[255];
    char passenger_path[255];

    bin_dir = dirname(argv[0]);
    memcpy(port_manager_path, bin_dir, strlen(bin_dir));
    memcpy(ferry_manager_path, bin_dir, strlen(bin_dir));
    memcpy(passenger_path, bin_dir, strlen(bin_dir));
    strcat(port_manager_path, "/port-manager");
    strcat(ferry_manager_path, "/ferry-manager");
    strcat(passenger_path, "/passenger");
    
    signal(SIGINT, handle_signal);
    
    // Initialize IPC keys
    control_queue = ftok(argv[0], 'C');
    log_queue_key = ftok(argv[0], 'L');
    shm_key = ftok(argv[0], 'S');
    sem_state_mutex_key = ftok(argv[0], 'M');
    sem_security_key = ftok(argv[0], 'E');
    sem_ramp_key = ftok(argv[0], 'R');
    
    if (control_queue == -1 || log_queue_key == -1 || shm_key == -1 ||
        sem_state_mutex_key == -1 || sem_security_key == -1 || sem_ramp_key == -1) {
        perror("Failed to initialize IPC keys");
        return 1;
    }
    
    // Clean up existing IPC resources
    queue_close_if_exists(control_queue);
    queue_close_if_exists(log_queue_key);
    shm_close_if_exists(shm_key);
    sem_close_if_exists(sem_state_mutex_key);
    sem_close_if_exists(sem_security_key);
    sem_close_if_exists(sem_ramp_key);
    
    // Create shared memory
    shm_id = shm_create(shm_key, sizeof(SharedState));
    if (shm_id == -1) {
        perror("Failed to create shared memory");
        return 1;
    }
    
    shared_state = (SharedState*)shm_attach(shm_id);
    if (shared_state == (void*)-1) {
        perror("Failed to attach shared memory");
        shm_close(shm_id);
        return 1;
    }
    
    // Initialize shared state
    shared_state->port_open = 1;
    shared_state->ramp.active_ferry_id = -1;
    shared_state->ramp.occupancy = 0;
    shared_state->vip_queue.head = 0;
    shared_state->vip_queue.tail = 0;
    shared_state->vip_queue.size = 0;
    shared_state->regular_queue.head = 0;
    shared_state->regular_queue.tail = 0;
    shared_state->regular_queue.size = 0;
    
    for (int i = 0; i < FERRY_COUNT; i++) {
        shared_state->ferries[i].ferry_id = i;
        shared_state->ferries[i].baggage_limit = FERRY_BAGGAGE_LIMIT_MIN + 
            (rand() % (FERRY_BAGGAGE_LIMIT_MAX - FERRY_BAGGAGE_LIMIT_MIN + 1));
        shared_state->ferries[i].passenger_count = 0;
        shared_state->ferries[i].baggage_weight_total = 0;
        shared_state->ferries[i].status = FERRY_WAITING_IN_QUEUE;
    }
    
    for (int i = 0; i < SECURITY_STATIONS; i++) {
        shared_state->stations[i].occupancy = 0;
        shared_state->stations[i].gender = -1;
        shared_state->stations[i].frustration_counter = 0;
    }
    
    // Create semaphores
    // State mutex: 1 semaphore initialized to 1 (binary mutex)
    unsigned short state_mutex_init[1] = {1};
    sem_state_mutex = sem_create(sem_state_mutex_key, 1, state_mutex_init);
    if (sem_state_mutex == -1) {
        perror("Failed to create state mutex semaphore");
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }
    
    // Security stations: SECURITY_STATIONS semaphores, each initialized to SECURITY_STATION_CAPACITY
    unsigned short security_init[SECURITY_STATIONS];
    for (int i = 0; i < SECURITY_STATIONS; i++) {
        security_init[i] = SECURITY_STATION_CAPACITY;
    }
    sem_security = sem_create(sem_security_key, SECURITY_STATIONS, security_init);
    if (sem_security == -1) {
        perror("Failed to create security semaphores");
        sem_close(sem_state_mutex);
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }
    
    // Ramp: 1 counting semaphore initialized to RAMP_CAPACITY
    unsigned short ramp_init[1] = {RAMP_CAPACITY};
    sem_ramp = sem_create(sem_ramp_key, 1, ramp_init);
    if (sem_ramp == -1) {
        perror("Failed to create ramp semaphore");
        sem_close(sem_state_mutex);
        sem_close(sem_security);
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }
    
    shm_detach(shared_state);
    
    // Initialize logger
    logger_pid = fork();
    if (logger_pid == -1) {
        perror("Logger failed");
    } else if (logger_pid == 0) {
        return logger_loop(log_queue_key);
    }

    // Initialize port manager process
    manager_pid = fork();
    if (manager_pid == -1) {
        perror("Manager start failed");
    } else if (manager_pid == 0) {
        if (execl(port_manager_path, port_manager_path, argv[0], NULL) == -1) {
            perror("Failed to start port manager");
        }
        return 0;
    }
    waitpid(manager_pid, NULL, 0);
    waitpid(logger_pid, NULL, 0);
    
    // Clean up IPC resources
    sem_close(sem_ramp);
    sem_close(sem_security);
    sem_close(sem_state_mutex);
    shm_close(shm_id);
    queue_close_if_exists(control_queue);
    queue_close_if_exists(log_queue_key);

    return 0;
}


int logger_loop(key_t queue_key) {
    FILE* log_file;
    LogMessage msg;
    int queue_id;
    int status = 0;
    
    log_file = fopen("./simulation.log", "w");
    if (!log_file) {
        return 1;
    }

    queue_id = queue_create(queue_key);

    while (1) {
        if (msgrcv(queue_id, &msg, sizeof(LogMessage), 0, 0) == -1) {
            // if (errno == EINTR) continue;
            status = 1;
            break;
        }
        fprintf(log_file, "[%s_%04d] %s\n", ROLE_NAMES[msg.mtype], msg.identifier, msg.message);
    }

    queue_close(queue_id);
    fflush(log_file);
    fclose(log_file);

    return status;
}