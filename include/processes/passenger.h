#ifndef FERRY_PROCESSES_PASSENGER_H
#define FERRY_PROCESSES_PASSENGER_H

typedef enum Gender {
    GENDER_MAN = 1,
    GENDER_WOMAN
} Gender;

typedef enum PassengerState {
    PASSENGER_CHECKIN,
    PASSENGER_BAG_CHECK,
    PASSENGER_WAITING,
    PASSENGER_BOARDING,
    PASSENGER_BOARDED
} PassengerState;

typedef struct PassengerTicket {
    PassengerState state;
    Gender gender;
    int vip;        // 1 - VIP
    int bag_weight;
} PassengerTicket;

#endif