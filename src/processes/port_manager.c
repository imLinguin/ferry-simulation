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

int sem_state_mutex;
SharedState* shared_state;

/**
 * Signal handler for port manager.
 * SIGINT: Initiates graceful shutdown by notifying all processes and closing the port.
 */
static void handle_signal(int signal) {
    if (signal == SIGINT) {
        kill(0, SIGUSR2);
        kill(0, SIGUSR1);
        sem_wait_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_PORT);
        shared_state->port_open = 0;
        sem_signal_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_PORT);
    }
}

/**
 * Port Manager Process Entry Point.
 * 
 * Main orchestrator for the ferry simulation:
 * 1. Initializes IPC resources (shared memory, semaphores, message queues)
 * 2. Spawns all ferry managers and passenger processes
 * 3. Spawns security manager for passenger screening
 * 4. Monitors process completion and manages graceful shutdown
 * 
 * Responsible for:
 * - Process lifecycle management
 * - Coordinating port closure when all passengers have boarded
 * - Ensuring all child processes terminate properly
 * 
 * @param argc Argument count (expects at least 2)
 * @param argv Arguments: [0]=program path (used to derive child process paths), [1]=IPC key path
 */
int main(int argc, char** argv) {
    key_t logger_key;
    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_ramp_key;

    int log_queue = -1;
    int shm_id;
    int sem_ramp;
    struct sigaction sa;

    if (argc < 2) return 1;
    srand(time(NULL) ^ getpid());

    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("PORTMANAGER Failed to register SIGINT");
    }
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("PORTMANAGER Failed to register SIGUSR2");
    }

    // Initialize IPC resources: message queues, shared memory, and semaphores
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

    // Determine executable paths for child processes based on current binary location
    char* bin_dir = dirname(strdup(argv[1]));
    char ferry_manager_path[255];
    char passenger_path[255];
    char ferry_id_arg[16];
    char passenger_id_arg[16];

    snprintf(ferry_manager_path, sizeof(ferry_manager_path), "%s/ferry-manager", bin_dir);
    snprintf(passenger_path, sizeof(passenger_path), "%s/passenger", bin_dir);

    pid_t ferry_pids[FERRY_COUNT];
    pid_t security_manager;

    // Spawn security manager process for passenger screening
    security_manager = fork();
    if (security_manager == -1) {
        perror("Failed to spawn security manager");
    }
    else if (security_manager == 0) {
        return run_security_manager(argv[1]);
    }

    // Spawn all ferry manager processes (one per ferry)
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

    // Spawn all passenger processes
    for (int i = 0; i < PASSENGER_COUNT; i++) {
        snprintf(passenger_id_arg, sizeof(passenger_id_arg), "%d", i);
        int passpid = fork();
        if (passpid == -1) {
            perror("Failed to spawn passenger");
        } else if (passpid == 0) {
            if (execl(passenger_path, passenger_path, argv[1], passenger_id_arg, NULL) == -1) {
                perror("Failed to exec passenger");
            }
            return 1;
        }
    }

    log_message(log_queue, ROLE, -1, "Spawned all ferries and passengers");

    // Monitor child processes: wait for all passengers to complete, then close port
    int counter = 0;
    int ferry_counter = 0;
    while (counter < PASSENGER_COUNT) {
        for (int i=0 ;i < FERRY_COUNT;i++) {
            if (waitpid(ferry_pids[i], NULL, WNOHANG) > 0) {
                ferry_counter++;
            }
        }
        if (waitpid(0, NULL, WNOHANG) > 0) {
            counter++;
            continue;
        }
        usleep(10000);
    }

    // All passengers have boarded or exited - signal port closure
    log_message(log_queue, ROLE, -1, "All passengers exited. Marking port as closed.");

    sem_wait_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_PORT);
    shared_state->port_open = 0;
    sem_signal_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_PORT);

    // Wait for all ferries to complete their final trips and exit
    while (ferry_counter < FERRY_COUNT) {
        for (int i=0 ;i < FERRY_COUNT; i++) {
            if (waitpid(ferry_pids[i], NULL, WNOHANG) > 0) {
                ferry_counter++;
            }
        }
        usleep(10000);
    }

    log_message(log_queue, ROLE, -1, "Port manager exiting");
    shm_detach(shared_state);

    return 0;
}

/**
 * Attempts to assign a passenger to an available security station.
 * 
 * Security stations are gender-segregated. The function searches for:
 * 1. Empty stations (any gender)
 * 2. Stations with matching gender and available slots
 * 
 * @param securityStations Array of security station states
 * @param msg Security message containing passenger info (gender, PID, passenger ID)
 * @return 1 if passenger was assigned to a station, 0 if no slot found
 */
int security_try_insert(SecurityStationState *securityStations, SecurityMessage *msg) {
    int found = 0;
    int variation = (rand() % (PASSENGER_SECURITY_TIME_MAX - PASSENGER_SECURITY_TIME_MIN + 1)) + PASSENGER_SECURITY_TIME_MIN;

    // Search for available security station matching passenger's gender
    for (int station = 0; station < SECURITY_STATIONS; station++) {
        // First look if the station is not occupied
        if (securityStations[station].usage == 0) {
            // Initialize empty station with passenger's gender and assign first slot
            securityStations[station].gender = msg->gender;
            securityStations[station].slots[0].pid = msg->pid;
            securityStations[station].slots[0].finish_timestamp = time(NULL) + variation;
            securityStations[station].slots[0].passenger_id = msg->passenger_id;
            securityStations[station].usage++;
            found = 1;
        }
        // Check if passenger's gender matches station and a slot is available
        else if (securityStations[station].usage == 1 && msg->gender == (Gender)securityStations[station].gender) {
            // Find an empty slot
            for (int slot = 0; slot < SECURITY_STATION_CAPACITY; slot++) {
                if (securityStations[station].slots[slot].pid == 0) {
                    securityStations[station].slots[slot].pid = msg->pid;
                    securityStations[station].slots[slot].finish_timestamp = time(NULL) + variation;
                    securityStations[station].slots[slot].passenger_id = msg->passenger_id;
                    securityStations[station].usage++;
                    // Note: Log moved to caller for station tracking
                    found = 1;
                    break;
                }
            }
        }

        if (found) break;
    }
    return found;
}

/**
 * Security Manager Process.
 * 
 * Manages passenger screening through gender-segregated security stations:
 * - Receives security requests from passengers via message queue
 * - Assigns passengers to appropriate stations based on gender
 * - Implements frustration mechanism: passengers overtaken multiple times get priority
 * - Notifies passengers when screening is complete
 * 
 * Uses an internal queue for passengers waiting when no matching station is available.
 * 
 * @param ipc_key Path used to generate IPC keys
 * @return 0 on success, 1 on error
 */
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

    srand(time(NULL) ^ getpid());

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    queue_security_key = ftok(ipc_key, IPC_KEY_QUEUE_SECURITY_ID);
    queue_log_key = ftok(ipc_key, IPC_KEY_LOG_ID);

    if (queue_security_key == -1) {
        perror("Security manager: Failed to open semaphores");
        return 1;
    }

    queue_security = queue_open(queue_security_key);
    queue_log = queue_open(queue_log_key);

    // Initialize security state: no pending requests, all stations empty
    pending.pid = 0;
    internal_queue.pid = 0;
    memset(security_stations, 0, sizeof(SecurityStationState) * SECURITY_STATIONS);

    // Main security processing loop: receive requests, assign stations, complete screenings
    while(1) {
        if (capacity == 0) goto reap_stations;
        if (pending.pid) goto try_insert;
        // Use non-blocking mode if there are pending operations to process
        int no_block = pending.pid + internal_queue.pid != 0 || capacity != initial_capacity;
        if(msgrcv(queue_security, &msg, MSG_SIZE(msg), 1, no_block ? IPC_NOWAIT : 0) == -1) {
            if (errno == EINVAL || errno == EIDRM) break;
            if (errno == EINTR) continue;
            if (errno == ENOMSG) goto try_insert;
            perror("Security manager: msgrcv failed");
        }
        log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "Receiving security queue request");
        pending = msg;

    try_insert:
        // Try to insert internal queue passenger first (frustration mechanism)
        if (internal_queue.pid) {
            if (security_try_insert(security_stations, &internal_queue)) {
                internal_queue.pid = 0;
                capacity--;
            } else {
                if (internal_queue.frustration == 3) goto reap_stations;
            }
        }

        // Process pending passenger request (with frustration increment if overtaken)
        if (pending.pid && (!internal_queue.pid || internal_queue.frustration < SECURITY_MAX_FRUSTRATION)) {
            log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "Attempting to insert pending passenger_id: %d (gender: %s)",
                        pending.passenger_id, pending.gender == GENDER_MAN ? "MALE" : "FEMALE");
            if (security_try_insert(security_stations, &pending)) {
                log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "Passenger %d assigned to security station (gender: %s)",
                            pending.passenger_id, pending.gender == GENDER_MAN ? "MALE" : "FEMALE");
                pending.pid = 0;
                capacity--;
                if(internal_queue.pid) {
                    internal_queue.frustration++;
                    log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "FRUSTRATION_INCREMENT - passenger %d overtaken (frustration: %d)",
                                internal_queue.passenger_id, internal_queue.frustration);
                }
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
        usleep(10000);
        // Check all stations for passengers who have completed security screening
        for (int station = 0; station < SECURITY_STATIONS; station++) {
            if (security_stations[station].usage == 0) continue;
            for (int slot = 0; slot < SECURITY_STATION_CAPACITY; slot++) {
                if (security_stations[station].slots[slot].pid != 0
                    && security_stations[station].slots[slot].finish_timestamp < time(NULL)) {
                    msg.mtype = security_stations[station].slots[slot].pid;
                    msg.passenger_id = security_stations[station].slots[slot].passenger_id;
                    msg.gender = security_stations[station].gender;

                    log_message(queue_log, ROLE_SECURITY_MANAGER, -1, "Passenger %d passed the security (station: %d, gender: %s)",
                                msg.passenger_id, station, msg.gender == GENDER_MAN ? "MALE" : "FEMALE");
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