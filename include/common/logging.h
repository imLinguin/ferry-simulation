#ifndef FERRY_COMMON_LOGGING_H
#define FERRY_COMMON_LOGGING_H

#include <stdarg.h>
#include "common/messages.h"

typedef enum Role {
    ROLE_PASSENGER = 1,
    ROLE_PORT_MANAGER,
    ROLE_FERRY_MANAGER,
    ROLE_PASSENGER_GENERATOR,
    ROLE_SECURITY_MANAGER
} Role;

static const char* ROLE_NAMES[] = {
    "PASSENGER",
    "PORT_MANAGER",
    "FERRY_MANAGER",
    "PASSENGER_GENERATOR",
    "SECURITY_MANAGER"
};

void log_message(int queue, Role role, int identifier, const char* message, ...);

#endif