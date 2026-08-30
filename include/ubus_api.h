#ifndef UBUS_API_H
#define UBUS_API_H

#include <libubus.h>
#include "telemetry.h"
#include "daemon_state.h"

int ubus_api_init(struct ubus_context *uctx, telemetry_ctx_t * telemetry_ctx, daemon_state_t * state);

#endif