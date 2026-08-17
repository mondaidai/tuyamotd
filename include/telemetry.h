#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <pthread.h>
#include <libubox/uloop.h>
#include "monitoring.h"

typedef struct telemetry_ctx {
	pthread_mutex_t lock;
	system_info_t info;
	bool valid;
	bool initialized;
	monitoring_ctx *mctx;
	struct uloop_timeout timer;
	unsigned int interval_msec;
} telemetry_ctx_t;

int telemetry_init(telemetry_ctx_t *ctx, monitoring_ctx *mctx, unsigned int interval_msec);
void telemetry_destroy(telemetry_ctx_t *ctx);
bool telemetry_take(system_info_t *dest, telemetry_ctx_t *ctx);
int telemetry_set_interval(telemetry_ctx_t *ctx, unsigned int interval_msec);

#endif
