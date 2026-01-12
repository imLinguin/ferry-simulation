#ifndef FERRY_COMMON_LOGGING_H
#define FERRY_COMMON_LOGGING_H

typedef enum Role {
    ROLE_PASSENGER,
    ROLE_PORT_MANAGER,
    ROLE_FERRY_MANAGER,
    ROLE_PASSENGER_GENERATOR,
} Role;

typedef struct LogMessage {
    long mtype;
    int identifier;
    char message[1024];

} LogMessage;

char* ROLE_NAMES[] = {
    "PASSENGER",
    "PORT_MANAGER",
    "FERRY_MANAGER",
    "PASSENGER_GENERATOR"
};

void log_message(int queue, Role role, const char* message);
void log_message_with_id(int queue, Role role, const char* message, int identifier);

#endif