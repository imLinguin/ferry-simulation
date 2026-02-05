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

// Ramp queue message types
#define RAMP_MESSAGE_EXIT 1        // Passenger leaving ramp
#define RAMP_PRIORITY_VIP 2        // VIP passenger request
#define RAMP_PRIORITY_REGULAR 3    // Regular passenger request

typedef struct RampMessage {
    long mtype;           // Priority: 1=exit, 2=VIP, 3=Regular, or PID for response
    long pid;             // Passenger PID for response
    int passenger_id;
    int weight;
    int is_vip;
} RampMessage;

typedef struct LogMessage {
    long mtype;
    int identifier;
    time_t timestamp;
    char message[1024];

} LogMessage;

#endif