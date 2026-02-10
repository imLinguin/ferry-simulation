#ifndef FERRY_PROCESSES_MAIN_H
#define FERRY_PROCESSES_MAIN_H
#include <sys/ipc.h>

int logger_loop(int queue_id, int shm_id);

#endif