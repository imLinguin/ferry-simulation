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


typedef struct SharedState {
    int port_open;
    int current_ferry_id;
    FerryState ferries[FERRY_COUNT];
} SharedState;

#endif
