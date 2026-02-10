#ifndef FERRY_COMMON_CONFIG_H
#define FERRY_COMMON_CONFIG_H

#define SECURITY_STATIONS 3
#define SECURITY_STATION_CAPACITY 2
#define SECURITY_MAX_FRUSTRATION 3

#define CONFIG_GET_INT(key) atoi(getenv(key))

#define LOG_FILE "simulation.log"

#endif