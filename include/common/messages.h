#ifndef FERRY_COMMON_MESSAGES_H
#define FERRY_COMMON_MESSAGES_H

#include "processes/passenger.h"

#define SECURITY_MESSAGE_MANAGER_ID 1

typedef struct SecurityMessage {
    long mtype;     // this will define a receiver 1 - stations manager, <pid>
    Gender gender;
    long pid;
    int passenger_id;
    int frustration;
} SecurityMessage;

typedef struct LogMessage {
    long mtype;
    int identifier;
    time_t timestamp;
    char message[1024];

} LogMessage;

#endif