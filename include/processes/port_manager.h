#ifndef FERRY_PROCESSES_PORT_MANAGER_H
#define FERRY_PROCESSES_PORT_MANAGER_H

#include <time.h>
#include <common/config.h>

typedef struct SecurityStationOccupant {
    long pid;
    int passenger_id;
    struct timespec finish_timestamp;
} SecurityStationOccupant;

typedef struct SecurityStationState {
    int gender;
    int usage;
    SecurityStationOccupant slots[SECURITY_STATION_CAPACITY];
} SecurityStationState;


int run_security_manager(const char* ipc_key);

#endif