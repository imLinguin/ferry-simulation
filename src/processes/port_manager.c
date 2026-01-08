#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "common/ipc.h"
#include "common/logging.h"
#include "processes/port_manager.h"

#define ROLE ROLE_FERRY_MANAGER

void handle_signal(int signal) {}

int main(int argc, char** argv) {
    key_t logger_key;
    int log_queue = -1;
    if (argc < 2) return 1;

    signal(SIGINT, handle_signal);

    logger_key = ftok(argv[1], 'L');
    if (logger_key != -1) {
        log_queue = queue_open(logger_key);
    }

    log_message(log_queue, ROLE, "Ferry manager starting up");


    return 0;
}