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

#include "common/macros.h"

int main(int argc, char **argv) {
    char* bin_dir;
    pid_t manager_pid;
    pid_t logger_pid;
    
    key_t queue_log_key;
    key_t queue_security_key;
    key_t queue_ramp_key;
    key_t shm_key;
    key_t sem_state_mutex_key;
    key_t sem_security_key;
    key_t sem_ramp_key;
    key_t sem_ramp_slots_key;
    key_t sem_current_ferry_key;
    
    int log_queue_id;
    int security_queue_id;
    int ramp_queue_id;
    int shm_id;
    int sem_state_mutex;
    int sem_security;
    int sem_ramp;
    int sem_ramp_slots;
    int sem_current_ferry;
    SharedState* shared_state;
    struct sigaction sa;

    char port_manager_path[255] = "";
    char ferry_manager_path[255] = "";
    char passenger_path[255] = "";
    char log_queue_arg[16];

    bin_dir = dirname(strdup(argv[0]));
    memcpy(port_manager_path, bin_dir, strlen(bin_dir));
    memcpy(ferry_manager_path, bin_dir, strlen(bin_dir));
    memcpy(passenger_path, bin_dir, strlen(bin_dir));
    strcat(port_manager_path, "/port-manager");
    strcat(ferry_manager_path, "/ferry-manager");
    strcat(passenger_path, "/passenger");
    
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to setup signal handler");
        return 1;
    }

    // Initialize IPC keys
    queue_log_key = ftok(argv[0], IPC_KEY_LOG_ID);
    queue_security_key = ftok(argv[0], IPC_KEY_QUEUE_SECURITY_ID);
    queue_ramp_key = ftok(argv[0], IPC_KEY_QUEUE_RAMP_ID);
    shm_key = ftok(argv[0], IPC_KEY_SHM_ID);
    sem_state_mutex_key = ftok(argv[0], IPC_KEY_SEM_STATE_ID);
    sem_security_key = ftok(argv[0], IPC_KEY_SEM_SECURITY_ID);
    sem_ramp_key = ftok(argv[0], IPC_KEY_SEM_RAMP_ID);
    sem_ramp_slots_key = ftok(argv[0], IPC_KEY_SEM_RAMP_SLOTS_ID);
    sem_current_ferry_key = ftok(argv[0], IPC_KEY_SEM_CURRENT_FERRY);
    
    if (queue_log_key == -1 || shm_key == -1 || queue_security_key == 1 || queue_ramp_key == -1 ||
        sem_state_mutex_key == -1 || sem_security_key == -1 || sem_ramp_key == -1 || 
        sem_ramp_slots_key == -1 || sem_current_ferry_key == -1) {
        perror("Failed to initialize IPC keys");
        return 1;
    }
    
    // Clean up existing IPC resources
    queue_close_if_exists(queue_log_key);
    queue_close_if_exists(queue_security_key);
    queue_close_if_exists(queue_ramp_key);

    shm_close_if_exists(shm_key);

    sem_close_if_exists(sem_state_mutex_key);
    sem_close_if_exists(sem_security_key);
    sem_close_if_exists(sem_ramp_key);
    sem_close_if_exists(sem_ramp_slots_key);
    sem_close_if_exists(sem_current_ferry_key);

    printf("Initializing queues\n");
    // Create queues
    if ((log_queue_id = queue_create(queue_log_key)) == -1) {
        perror("Failed to create logger queue");
        return 1;
    }
    if ((security_queue_id = queue_create(queue_security_key)) == -1) {
        perror("Failed to create security queue");
        return 1;
    }
    if ((ramp_queue_id = queue_create(queue_ramp_key)) == -1) {
        perror("Failed to create ramp queue");
        return 1;
    }
    
    printf("Initializing shm\n");
    // Create shared memory
    if ((shm_id = shm_create(shm_key, sizeof(SharedState))) == -1) {
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
    shared_state->current_ferry_id = -1;
  
    for (int i = 0; i < FERRY_COUNT; i++) {
        shared_state->ferries[i].ferry_id = i;
        shared_state->ferries[i].baggage_limit = FERRY_BAGGAGE_LIMIT_MIN + 
            (rand() % (FERRY_BAGGAGE_LIMIT_MAX - FERRY_BAGGAGE_LIMIT_MIN + 1));
        shared_state->ferries[i].passenger_count = 0;
        shared_state->ferries[i].baggage_weight_total = 0;
        shared_state->ferries[i].status = FERRY_WAITING_IN_QUEUE;
    }
    
    printf("Initializing semaphores\n");
    // Create semaphores
    unsigned short state_mutex_init = 1;
    if ((sem_state_mutex = sem_create(sem_state_mutex_key, 1, &state_mutex_init)) == -1) {
        perror("Failed to create state mutex semaphore");
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }
    
    unsigned short security_init = SECURITY_STATIONS * SECURITY_STATION_CAPACITY;
    if ((sem_security = sem_create(sem_security_key, 1, &security_init)) == -1) {
        perror("Failed to create security queue semaphore");
        sem_close(sem_state_mutex);
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }
    
    unsigned short ramp_init = 0;
    if ((sem_ramp = sem_create(sem_ramp_key, 1, &ramp_init)) == -1) {
        perror("Failed to create ramp semaphore");
        sem_close(sem_state_mutex);
        sem_close(sem_security);
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }

    // FIXME: USE a ulimit ?
    unsigned short ramp_slots_init = RAMP_CAPACITY;
    if ((sem_ramp_slots = sem_create(sem_ramp_slots_key, 1, &ramp_slots_init)) == -1) {
        perror("Failed to create ramp slots semaphore");
        sem_close(sem_state_mutex);
        sem_close(sem_security);
        sem_close(sem_ramp);
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }

    unsigned short current_ferry_init = 1;
    if ((sem_current_ferry = sem_create(sem_current_ferry_key, 1, &current_ferry_init)) == -1) {
        perror("Failed to create current ferry semaphore");
        sem_close(sem_state_mutex);
        sem_close(sem_security);
        sem_close(sem_ramp);
        sem_close(sem_ramp_slots);
        shm_detach(shared_state);
        shm_close(shm_id);
        return 1;
    }
    
    shm_detach(shared_state);
    
    // Convert log_queue_id to string for logger process
    snprintf(log_queue_arg, sizeof(log_queue_arg), "%d", log_queue_id);
    
    printf("Staring logger\n");
    // Initialize logger
    logger_pid = fork();
    if (logger_pid == -1) {
        perror("Logger failed");
        queue_close(log_queue_id);
        return 1;
    } else if (logger_pid == 0) {
        return logger_loop(log_queue_id);
    }

    // Initialize port manager process
    printf("Staring port manager\n");
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
    queue_close_if_exists(queue_log_key);
    waitpid(logger_pid, NULL, 0);
    
    // Clean up IPC resources
    sem_close(sem_ramp);
    sem_close(sem_ramp_slots);
    sem_close(sem_security);
    sem_close(sem_state_mutex);
    shm_close(shm_id);
    queue_close_if_exists(queue_security_key);
    queue_close_if_exists(queue_ramp_key);

    return 0;
}


int logger_loop(int queue_id) {
    FILE* log_file;
    LogMessage msg;
    char time_buf[255] = "";
    int status = 0;
    struct tm *time;
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to setup signal handler");
        return 1;
    }

    log_file = fopen("./simulation.log", "w");
    if (!log_file) {
        return 1;
    }

    printf("Logger start\n");

    while (1) {
        if (msgrcv(queue_id, &msg, MSG_SIZE(msg), 0, 0) == -1) {
            if (errno == EINTR) continue;
            status = 1;
            break;
        }
        time = localtime(&msg.timestamp);
        strftime(time_buf, sizeof(time_buf), "(%d-%m-%Y %H:%M:%S)", time);
        if (msg.identifier == -1) {
            printf("%s [%s] %s\n", time_buf, ROLE_NAMES[msg.mtype-1], msg.message);
            fprintf(log_file, "%s [%s] %s\n", time_buf, ROLE_NAMES[msg.mtype-1], msg.message);       
        }
        else {
            printf("%s [%s_%04d] %s\n", time_buf, ROLE_NAMES[msg.mtype-1], msg.identifier, msg.message);
            fprintf(log_file, "%s [%s_%04d] %s\n", time_buf, ROLE_NAMES[msg.mtype-1], msg.identifier, msg.message);
        }
    }

    fflush(log_file);
    fclose(log_file);

    return status;
}