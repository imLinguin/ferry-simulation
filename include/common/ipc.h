#ifndef FERRY_COMMON_IPC_H
#define FERRY_COMMON_IPC_H

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>


int queue_create(key_t queue_key);
int queue_open(key_t queue_key);
int queue_close(int queue_id);
void queue_close_if_exists(key_t queue_key);

#endif