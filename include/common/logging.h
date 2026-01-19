#ifndef FERRY_COMMON_LOGGING_H
#define FERRY_COMMON_LOGGING_H

#include "common/messages.h"

typedef enum Role {
    ROLE_PASSENGER = 1,
    ROLE_PORT_MANAGER,
    ROLE_FERRY_MANAGER,
    ROLE_PASSENGER_GENERATOR,
} Role;

char* ROLE_NAMES[] = {
    "PASSENGER",
    "PORT_MANAGER",
    "FERRY_MANAGER",
    "PASSENGER_GENERATOR"
};

void log_message(int queue, Role role, const char* message);
void log_message_with_id(int queue, Role role, const char* message, int identifier);

#endif