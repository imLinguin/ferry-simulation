#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "common/ipc.h"

// Handle old GLIBC versions that do not have semun defined according to semctl manual
#if defined(_SEM_SEMUN_UNDEFINED) || !defined(__GLIBC__)
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
    struct seminfo* __buf;
};
#endif


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
    int q = queue_open(queue_key);
    if (q != -1) {
        queue_close(q);
    }
}

int sem_create(key_t sem_key, int semaphore_count, const unsigned short* initial_values) {
    int sem_id = semget(sem_key, semaphore_count, IPC_CREAT | 0600);
    if (sem_id == -1) return -1;

    if (initial_values != NULL) {
        union semun arg;
        arg.array = (unsigned short*)initial_values;
        if (semctl(sem_id, 0, SETALL, arg) == -1) {
            semctl(sem_id, 0, IPC_RMID);
            return -1;
        }
    }

    return sem_id;
}

int sem_open(key_t sem_key) {
    return semget(sem_key, 0, 0);
}

int sem_close(int sem_id) {
    return semctl(sem_id, 0, IPC_RMID);
}

void sem_close_if_exists(key_t sem_key) {
    int sem_id = sem_open(sem_key);
    if (sem_id != -1) {
        sem_close(sem_id);
    }
}

int sem_wait_single(int sem_id, unsigned short sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    return semop(sem_id, &op, 1);
}

int sem_signal_single(int sem_id, unsigned short sem_num) {
    struct sembuf op = {sem_num, 1, 0};
    return semop(sem_id, &op, 1);
}

int shm_create(key_t shm_key, size_t size) {
    return shmget(shm_key, size, IPC_CREAT | 0600);
}

int shm_open(key_t shm_key) {
    return shmget(shm_key, 0, 0);
}

int shm_close(int shm_id) {
    return shmctl(shm_id, IPC_RMID, NULL);
}

void shm_close_if_exists(key_t shm_key) {
    int shm_id = shm_open(shm_key);
    if (shm_id != -1) {
        shm_close(shm_id);
    }
}

void* shm_attach(int shm_id) {
    return shmat(shm_id, NULL, 0);
}

int shm_detach(const void* addr) {
    return shmdt(addr);
}