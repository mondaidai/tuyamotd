#ifndef DAEMON_STATE_H
#define DAEMON_STATE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    pthread_mutex_t lock;

    bool mqtt_connected;
    time_t last_report_time;

    uint64_t reports_sent;
    uint64_t reports_failed;
} daemon_state_t;

#endif