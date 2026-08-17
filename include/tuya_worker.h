#ifndef TUYA_WORKER_H
#define TUYA_WORKER_H

#include <pthread.h>
#include <stdbool.h>

#include "config.h"
#include "telemetry.h"
#include "daemon_state.h"
#include "tuyalink_core.h"

typedef struct {
	tuya_mqtt_context_t *client;
	daemon_state_t *state;
	telemetry_ctx_t *telemetry;
	app_config_t *config;
} tuya_worker_ctx_t;

void * tuya_worker(void *arg);

#endif