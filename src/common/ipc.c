#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>

#include "common/ipc.h"

/**
 * Creates a new message queue with the specified key.
 * @param queue_key The IPC key for the message queue
 * @return Queue ID on success, -1 on error
 */
int queue_create(key_t queue_key) {
    return msgget(queue_key, IPC_CREAT | IPC_EXCL | 0600);
}

/**
 * Opens an existing message queue.
 * @param queue_key The IPC key for the message queue
 * @return Queue ID on success, -1 on error
 */
int queue_open(key_t queue_key) {
    return msgget(queue_key, 0);
}

/**
 * Removes a message queue from the system.
 * @param queue_id The message queue identifier
 * @return 0 on success, -1 on error
 */
int queue_close(int queue_id) {
    return msgctl(queue_id, IPC_RMID, NULL);
}

/**
 * Closes a message queue if it exists.
 * @param queue_key The IPC key for the message queue
 * @return Result of queue_close if exists, -1 otherwise
 */
int queue_close_if_exists(key_t queue_key) {
    int q = queue_open(queue_key);
    if (q != -1) {
        return queue_close(q);
    }
    return q;
}

/**
 * Creates a new semaphore set with the specified key and count.
 * @param sem_key The IPC key for the semaphore set
 * @param semaphore_count Number of semaphores in the set
 * @param initial_values Array of initial values for semaphores (can be NULL)
 * @return Semaphore set ID on success, -1 on error
 */
int sem_create(key_t sem_key, int semaphore_count, unsigned short* initial_values) {
    int sem_id = semget(sem_key, semaphore_count, IPC_CREAT | IPC_EXCL | 0600);
    if (sem_id == -1) return -1;

    if (initial_values != NULL) {
        union semun arg;
        arg.array = initial_values;
        if (semctl(sem_id, 0, SETALL, arg) == -1) {
            semctl(sem_id, 0, IPC_RMID);
            return -1;
        }
    }

    return sem_id;
}

/**
 * Opens an existing semaphore set or creates it if it doesn't exist.
 * @param sem_key The IPC key for the semaphore set
 * @param semaphore_count Number of semaphores in the set
 * @return Semaphore set ID on success, -1 on error
 */
int sem_open(key_t sem_key, int semaphore_count) {
    return semget(sem_key, semaphore_count, IPC_CREAT | 0600);
}

/**
 * Removes a semaphore set from the system.
 * @param sem_id The semaphore set identifier
 * @return 0 on success, -1 on error
 */
int sem_close(int sem_id) {
    return semctl(sem_id, 0, IPC_RMID);
}

/**
 * Closes a semaphore set if it exists.
 * @param sem_key The IPC key for the semaphore set
 */
void sem_close_if_exists(key_t sem_key) {
    int sem_id = semget(sem_key, 1, 0);
    if (sem_id != -1) {
        sem_close(sem_id);
    }
}

/**
 * Gets the current value of a semaphore with EINTR retry.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @return Semaphore value on success, -1 on error
 */
int sem_get_val(int sem_id, unsigned short sem_num) {
    int retval;
    while ((retval = semctl(sem_id, sem_num, GETVAL)) == -1) {
        if (errno != EINTR) break;
    }
    return retval;
}

/**
 * Sets a semaphore value without SEM_UNDO flag with EINTR retry.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @param value The new value to set
 * @return 0 on success, -1 on error
 */
int sem_set_noundo(int sem_id, unsigned short sem_num, int value) {
    int retval;
    while ((retval = semctl(sem_id, sem_num, SETVAL, value)) == -1) {
        if (errno != EINTR) break;
    }
    return retval;
}

/**
 * Waits on (decrements) a single semaphore without SEM_UNDO flag.
 * Retries on EINTR. Use this when the operation should not be undone on process exit.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @return 0 on success, -1 on error
 */
int sem_wait_single_noundo(int sem_id, unsigned short sem_num) {
    int retval;
    struct sembuf op = {sem_num, -1, 0};
    while ((retval = semop(sem_id, &op, 1)) == -1) {
        if (errno != EINTR) break;
    }
    return retval;
}

/**
 * Waits on (decrements) a single semaphore with SEM_UNDO flag.
 * Retries on EINTR. SEM_UNDO automatically undoes the operation on process exit.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @return 0 on success, -1 on error
 */
int sem_wait_single(int sem_id, unsigned short sem_num) {
    int retval;
    struct sembuf op = {sem_num, -1, SEM_UNDO};
    while ((retval = semop(sem_id, &op, 1)) == -1) {
        if (errno != EINTR) break;
    }
    return retval;
}

/**
 * Waits on (decrements) a single semaphore with SEM_UNDO flag.
 * Does NOT retry on EINTR - returns immediately if interrupted.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @return 0 on success, -1 on error (including EINTR)
 */
int sem_wait_single_nointr(int sem_id, unsigned short sem_num) {
    struct sembuf op = {sem_num, -1, SEM_UNDO};
    return semop(sem_id, &op, 1);
}

/**
 * Waits on (decrements) a single semaphore without SEM_UNDO flag.
 * Does NOT retry on EINTR - returns immediately if interrupted.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @return 0 on success, -1 on error (including EINTR)
 */
int sem_wait_single_nointr_noundo(int sem_id, unsigned short sem_num) {
    struct sembuf op = {sem_num, -1, SEM_UNDO};
    return semop(sem_id, &op, 1);
}

/**
 * Signals (increments) a single semaphore without SEM_UNDO flag.
 * Retries on EINTR.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @return 0 on success, -1 on error
 */
int sem_signal_single_noundo(int sem_id, unsigned short sem_num) {
    int retval;
    struct sembuf op = {sem_num, 1, 0};
    while ((retval = semop(sem_id, &op, 1)) == -1) {
        if (errno != EINTR) break;
    }
    return retval;
}

/**
 * Signals (increments) a single semaphore with SEM_UNDO flag.
 * Retries on EINTR. SEM_UNDO automatically undoes the operation on process exit.
 * @param sem_id The semaphore set identifier
 * @param sem_num The semaphore number within the set
 * @return 0 on success, -1 on error
 */
int sem_signal_single(int sem_id, unsigned short sem_num) {
    int retval;
    struct sembuf op = {sem_num, 1, SEM_UNDO};
    while ((retval = semop(sem_id, &op, 1)) == -1) {
        if (errno != EINTR) break;
    }
    return retval;
}

/**
 * Creates a new shared memory segment.
 * @param shm_key The IPC key for the shared memory
 * @param size Size of the shared memory segment in bytes
 * @return Shared memory ID on success, -1 on error
 */
int shm_create(key_t shm_key, size_t size) {
    return shmget(shm_key, size, IPC_CREAT | 0600);
}

/**
 * Opens an existing shared memory segment.
 * @param shm_key The IPC key for the shared memory
 * @return Shared memory ID on success, -1 on error
 */
int shm_open(key_t shm_key) {
    return shmget(shm_key, 0, 0);
}

/**
 * Marks a shared memory segment for removal.
 * It will be destroyed after all processes detach from it.
 * @param shm_id The shared memory identifier
 * @return 0 on success, -1 on error
 */
int shm_close(int shm_id) {
    return shmctl(shm_id, IPC_RMID, NULL);
}

/**
 * Closes a shared memory segment if it exists.
 * @param shm_key The IPC key for the shared memory
 */
void shm_close_if_exists(key_t shm_key) {
    int shm_id = shm_open(shm_key);
    if (shm_id != -1) {
        shm_close(shm_id);
    }
}

/**
 * Attaches a shared memory segment to the process's address space.
 * @param shm_id The shared memory identifier
 * @return Pointer to the attached memory on success, (void*)-1 on error
 */
void* shm_attach(int shm_id) {
    return shmat(shm_id, NULL, 0);
}

/**
 * Detaches a shared memory segment from the process's address space.
 * @param addr The address of the attached shared memory
 * @return 0 on success, -1 on error
 */
int shm_detach(const void* addr) {
    return shmdt(addr);
}