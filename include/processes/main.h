#ifndef FERRY_PROCESSES_MAIN_H
#define FERRY_PROCESSES_MAIN_H
#include <sys/ipc.h>

int logger_loop(key_t queue_key);

#endif