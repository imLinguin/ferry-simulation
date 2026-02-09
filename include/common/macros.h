#ifndef FERRY_COMMON_MACROS_H
#define FERRY_COMMON_MACROS_H

#define MSG_SIZE(param) sizeof(param) - sizeof(param.mtype)
#define START_SEMAPHORE(sem, sem_num) \
    sem_wait_single(sem,sem_num); \
    {

#define END_SEMAPHORE(sem, sem_num) \
    sem_signal_single(sem,sem_num); \
    }

#define TIMESPEC_DIFF(pred, current) ((double)(current.tv_sec - pred.tv_sec) + (current.tv_nsec - pred.tv_nsec) / 1000000000.0)

#endif