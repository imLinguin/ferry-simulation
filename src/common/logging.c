#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "common/ipc.h"
#include "common/logging.h"
#include "common/macros.h"


void log_message(int queue, Role role, int identifier, const char* message, ...) {
    if (queue == -1) return;
    LogMessage msg;
    va_list args;
    va_start(args, message);
    
    msg.mtype = (long)role;
    msg.identifier = identifier;
    msg.timestamp = time(NULL);
    strcpy(msg.message, message);

    vsnprintf(msg.message, sizeof(msg.message), message, args);

    va_end(args);

    while (msgsnd(queue, &msg, MSG_SIZE(msg), 0) == -1 && errno == EINTR) {}
}