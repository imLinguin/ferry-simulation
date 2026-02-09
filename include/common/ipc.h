#ifndef FERRY_COMMON_IPC_H
#define FERRY_COMMON_IPC_H

#include <stddef.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#ifdef _SEM_SEMUN_UNDEFINED 
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
    struct seminfo* __buf; // Linux only
};
#endif

// Queues are lowercase
#define IPC_KEY_LOG_ID 'l'
#define IPC_KEY_QUEUE_SECURITY_ID 's'
#define IPC_KEY_QUEUE_RAMP_ID 'r'
// SEM and SHM uppercase
#define IPC_KEY_SHM_ID 'S'
#define IPC_KEY_SEM_STATE_ID 'M'
#define IPC_KEY_SEM_SECURITY_ID 'E'
#define IPC_KEY_SEM_RAMP_ID 'R'
#define IPC_KEY_SEM_RAMP_SLOTS_ID 'T'
#define IPC_KEY_SEM_CURRENT_FERRY 'F'


typedef enum SemStateMutexVariant {
    SEM_STATE_MUTEX_VARIANT_PORT,
    SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY,
    SEM_STATE_MUTEX_VARIANT_FERRIES_STATE,
    SEM_STATE_MUTEX_VARIANT_STATS,
    SEM_STATE_MUTEX_VARIANT_COUNT
} SemStateMutexVariant;

int queue_create(key_t queue_key);
int queue_open(key_t queue_key);
int queue_close(int queue_id);
int queue_close_if_exists(key_t queue_key);

int sem_create(key_t sem_key, int semaphore_count, unsigned short* initial_values);
int sem_open(key_t sem_key, int semaphore_count);
int sem_close(int sem_id);
void sem_close_if_exists(key_t sem_key);
int sem_get_val(int sem_id, unsigned short sem_num);
int sem_signal_noundo(int sem_id, unsigned short sem_num, int val);
int sem_wait_single_noundo(int sem_id, unsigned short sem_num);
int sem_wait_single(int sem_id, unsigned short sem_num);
int sem_wait_single_nointr(int sem_id, unsigned short sem_num);
int sem_wait_single_nointr_noundo(int sem_id, unsigned short sem_num);
int sem_signal_single_noundo(int sem_id, unsigned short sem_num);
int sem_signal_single(int sem_id, unsigned short sem_num);

int shm_create(key_t shm_key, size_t size);
int shm_open(key_t shm_key);
int shm_close(int shm_id);
void shm_close_if_exists(key_t shm_key);
void* shm_attach(int shm_id);
int shm_detach(const void* addr);

#endif