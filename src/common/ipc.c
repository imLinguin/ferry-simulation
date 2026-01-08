#include <stdio.h>
#include "common/ipc.h"


int queue_create(key_t queue_key) {
    return msgget(queue_key, IPC_CREAT | 0600);
}

int queue_open(key_t queue_key) {
    return msgget(queue_key, 0);
}

int queue_close(int queue_id) {
    return msgctl(queue_id, IPC_RMID, NULL);
}

void queue_close_if_exists(key_t queue_key) {
    key_t q;
    if ((q = queue_open(queue_key)) != -1) {
        queue_close(q);
    }
}