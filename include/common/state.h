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

typedef struct SimulationStats {
    int passengers_spawned;
    int passengers_boarded;
    int passengers_rejected_baggage;
    int total_ferry_trips;
    int passengers_screened;
} SimulationStats;

typedef struct SharedState {
    int port_open;
    int current_ferry_id;
    SimulationStats stats;
    FerryState ferries[];
} SharedState;

#endif
