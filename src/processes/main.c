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
#include "common/logging.h"
#include "common/ipc.h"

void handle_signal(int signal) {}

int main(int argc, char **argv) {
    char* bin_dir;
    pid_t manager_pid;
    pid_t logger_pid;
    pid_t passenger_generator;
    
    key_t control_queue;
    key_t log_queue_key;

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
    
    // Initialize configuration

    signal(SIGINT, handle_signal);
    // Initialize queues
    control_queue = ftok(argv[0], 'C');
    log_queue_key = ftok(argv[0], 'L');
    
    if (control_queue == -1 || log_queue_key == -1) {
        perror("Failed to initialize queues");
        return 1;
    }
    queue_close_if_exists(control_queue);
    queue_close_if_exists(log_queue_key);
    
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

    // Initialize passenger generator
    passenger_generator = fork();
    if (passenger_generator == -1) {
        perror("Passenger generator failed");
    } else if (passenger_generator == 0) {

        return 0;
    }

    waitpid(manager_pid, NULL, 0);
    waitpid(passenger_generator, NULL, 0);
    waitpid(logger_pid, NULL, 0);

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

        fprintf(log_file, "[%s] %s\n", ROLE_NAMES[msg.role], msg.message);
    }

    queue_close(queue_id);
    fflush(log_file);
    fclose(log_file);

    return status;
}