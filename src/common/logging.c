#include <string.h>
#include <stdio.h>

#include "common/ipc.h"
#include "common/logging.h"

void log_message(int queue, Role role, const char* message) {
    log_message_with_id(queue, role, message, -1);
}

void log_message_with_id(int queue, Role role, const char* message, int identifier) {
    if (queue == -1) return;
    LogMessage msg;
    
    msg.mtype = 1;
    msg.identifier = identifier;
    msg.role = role;
    strcpy(msg.message, message);
    
    msgsnd(queue, &msg, sizeof(msg), 0);
}