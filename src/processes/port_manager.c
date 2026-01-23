#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "common/config.h"
#include "common/state.h"
#include "common/ipc.h"
#include "common/logging.h"
#include "common/messages.h"
#include "common/macros.h"
#include "processes/port_manager.h"

#define ROLE ROLE_PORT_MANAGER

void handle_signal(int signal) {
    kill(getpid(), SIGUSR2);
}

int main(int argc, char** argv) {
    key_t logger_key;
    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_ramp_key;
    
    int log_queue = -1;
    int shm_id;
    int sem_state_mutex;
    int sem_ramp;
    SharedState* shared_state;
    struct sigaction sa;

    if (argc < 2) return 1;

    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("PORTMANAGER Failed to register SIGINT");
    }

    // Open IPC resources
    logger_key = ftok(argv[1], IPC_KEY_LOG_ID);
    shm_key = ftok(argv[1], IPC_KEY_SHM_ID);
    sem_state_mutex_key = ftok(argv[1], IPC_KEY_SEM_STATE_ID);
    sem_ramp_key = ftok(argv[1], IPC_KEY_SEM_RAMP_ID);
    
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
    
    sem_state_mutex = sem_open(sem_state_mutex_key, 1);
    sem_ramp = sem_open(sem_ramp_key, 1);
    
    if (sem_state_mutex == -1 || sem_ramp == -1) {
        perror("Port manager: Failed to open semaphores");
        shm_detach(shared_state);
        return 1;
    }

    log_message(log_queue, ROLE, -1, "Port manager starting up");

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
    pid_t security_manager;

    security_manager = fork();
    if (security_manager == -1) {
        perror("Failed to spawn security manager");
    }
    else if (security_manager == 0) {
        return run_security_manager(argv[1]);
    }
    
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
    
    log_message(log_queue, ROLE, -1, "Spawned all ferries and passengers");
    
    // Wait for all ferry managers and passengers
    for (int i = 0; i < FERRY_COUNT; i++) {
        waitpid(ferry_pids[i], NULL, 0);
    }
    for (int i = 0; i < PASSENGER_COUNT; i++) {
        waitpid(passenger_pids[i], NULL, 0);
    }
    waitpid(security_manager, NULL, 0);

    shm_detach(shared_state);

    return 0;
}

int security_try_insert(SecurityStationState *securityStations, SecurityMessage *msg) {
    int found = 0;
    int variation = (rand() % (PASSENGER_SECURITY_TIME_MAX - PASSENGER_SECURITY_TIME_MIN + 1)) + PASSENGER_SECURITY_TIME_MIN;

    for (int station = 0; station < SECURITY_STATIONS; station++) {
        if (securityStations[station].usage == 0) {
            securityStations[station].gender = msg->gender;
            securityStations[station].slots[0].pid = msg->pid;
            securityStations[station].slots[0].finish_timestamp = time(NULL) + variation;
            securityStations[station].slots[0].passenger_id = msg->passenger_id;
            securityStations[station].usage++;
            found = 1;
        }
        else if (securityStations[station].usage == 1 && msg->gender == (Gender)securityStations[station].gender) {
            // Find an empty slot
            for (int slot = 0; slot < SECURITY_STATION_CAPACITY; slot++) {
                if (securityStations[station].slots[slot].pid == 0) {
                    securityStations[station].slots[slot].pid = msg->pid;
                    securityStations[station].slots[slot].finish_timestamp = time(NULL) + variation;
                    securityStations[station].slots[slot].passenger_id = msg->passenger_id;
                    securityStations[station].usage++;
                    found = 1;
                    break;
                }
            }
        }

        if (found) break;
    }
    return found;
}

int run_security_manager(const char* ipc_key) {
    key_t queue_security_key;
    key_t queue_log_key;
    int queue_security;
    int queue_log;

    int initial_capacity = SECURITY_STATIONS * SECURITY_STATION_CAPACITY;
    int capacity = initial_capacity;
    SecurityStationState security_stations[SECURITY_STATIONS];
    SecurityMessage msg;
    SecurityMessage pending;
    SecurityMessage internal_queue;
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    
    queue_security_key = ftok(ipc_key, IPC_KEY_QUEUE_SECURITY_ID);
    queue_log_key = ftok(ipc_key, IPC_KEY_LOG_ID);

    if (queue_security_key == -1) {
        perror("Security manager: Failed to open semaphores");
        return 1;
    }

    queue_security = queue_open(queue_security_key);
    queue_log = queue_open(queue_log_key);

    pending.pid = 0;
    internal_queue.pid = 0;
    memset(security_stations, 0, sizeof(SecurityStationState) * SECURITY_STATIONS);

    while(1) {
        if (capacity == 0) goto reap_stations;
        if (pending.pid) goto try_insert;
        int no_block = pending.pid + internal_queue.pid != 0 || capacity != initial_capacity;
        if(msgrcv(queue_security, &msg, sizeof(msg) - sizeof(msg.mtype), 1, no_block ? IPC_NOWAIT : 0) == -1) {
            if (errno == EINTR) continue;
            if (errno == ENOMSG) goto try_insert;
            perror("Security manager: msgrcv failed");
        }
        log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "Receiving security queue request");
        pending = msg;

    try_insert:
        // Try to insert
        if (internal_queue.pid) {
            if (security_try_insert(security_stations, &internal_queue)) {
                internal_queue.pid = 0;
                capacity--;
            } else {
                if (internal_queue.frustration == 3) goto reap_stations;
            }
        }

        if (pending.pid && (!internal_queue.pid || internal_queue.frustration < SECURITY_MAX_FRUSTRATION)) {
            log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "Attempting to insert pending passenger_id: %d", pending.passenger_id);
            if (security_try_insert(security_stations, &pending)) {
                pending.pid = 0;
                capacity--;
                if(internal_queue.pid) internal_queue.frustration++;
            }
            else if (!internal_queue.pid) {
                log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "No slot found, adding to internal queue");
                internal_queue = pending;
                pending.pid = 0;
            }
            else {
                log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "No slot found");
            }
        }
    reap_stations:
        usleep(100);
        // Clean and notify completed passengers
        for (int station = 0; station < SECURITY_STATIONS; station++) {
            if (security_stations[station].usage == 0) continue;
            for (int slot = 0; slot < SECURITY_STATION_CAPACITY; slot++) {
                if (security_stations[station].slots[slot].pid != 0
                    && security_stations[station].slots[slot].finish_timestamp < time(NULL)) {
                    msg.mtype = security_stations[station].slots[slot].pid;
                    msg.passenger_id = security_stations[station].slots[slot].passenger_id;
                    msg.gender = security_stations[station].gender;
                    
                    log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "Passenger %d passed the security", msg.passenger_id);
                    if (msgsnd(queue_security, &msg, MSG_SIZE(msg), 0)) {
                        perror("Failed to send message back to user");
                    }
                    security_stations[station].usage--;
                    security_stations[station].slots[slot].pid = 0;
                    capacity++;
                }
            }
        }
    }

    return 0;
}