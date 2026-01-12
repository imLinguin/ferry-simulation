#ifndef FERRY_COMMON_STATE_H
#define FERRY_COMMON_STATE_H

#include "common/config.h"

typedef enum FerryStatus {
    FERRY_WAITING_IN_QUEUE = 1,
    FERRY_BOARDING,
    FERRY_DEPARTED,
    FERRY_TRAVELING
} FerryStatus;

typedef struct FerryState {
    int ferry_id;
    int baggage_limit;
    int passenger_count;
    int baggage_weight_total;
    FerryStatus status;
} FerryState;

typedef struct SecurityStationState {
    int occupancy;
    int gender; /* -1 means empty, 0 male, 1 female */
    int frustration_counter;
} SecurityStationState;

typedef struct RampState {
    int active_ferry_id;
    int occupancy;
} RampState;

typedef struct PassengerQueue {
    int slots[QUEUE_CAPACITY];
    int head;
    int tail;
    int size;
} PassengerQueue;

typedef struct SharedState {
    int port_open;
    FerryState ferries[FERRY_COUNT];
    SecurityStationState stations[SECURITY_STATIONS];
    RampState ramp;
    PassengerQueue vip_queue;
    PassengerQueue regular_queue;
} SharedState;

#endif
