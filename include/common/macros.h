#ifndef FERRY_COMMON_MACROS_H
#define FERRY_COMMON_MACROS_H

#define MSG_SIZE(param) sizeof(param) - sizeof(param.mtype)
#define START_SEMAPHORE(sem, sem_num) \
    sem_wait_single(sem,sem_num); \
    {

#define END_SEMAPHORE(sem, sem_num) \
    sem_signal_single(sem,sem_num); \
    }


#endif