#include "ubus_api.h"
#include "telemetry.h"
#include <stdint.h>
#include <pthread.h>
#include <syslog.h>
#include <time.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>

static telemetry_ctx_t *g_telemetry_ctx = NULL;
static daemon_state_t *g_state = NULL;

enum {
	STATUS_INTERVAL_MSEC,
	__STATUS_SET_MAX,
};

static int status_get(struct ubus_context *ctx, struct ubus_object *obj,
                      struct ubus_request_data *req, const char *method,
                      struct blob_attr *msg);
static int status_set_interval(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg);

static const struct blobmsg_policy status_set_policy[] = {
	[STATUS_INTERVAL_MSEC] = { .name = "interval_msec", .type = BLOBMSG_TYPE_INT32 },
};

static const struct ubus_method status_methods[] = {
	UBUS_METHOD_NOARG("status", status_get),
	UBUS_METHOD("set_interval", status_set_interval, status_set_policy),
};

static struct ubus_object_type status_object_type =
	UBUS_OBJECT_TYPE("tuyamotd", status_methods);

struct ubus_object status_object = {
	.name = "tuyamotd",
	.type = &status_object_type,
	.methods = status_methods,
	.n_methods = ARRAY_SIZE(status_methods),
};

static int status_get(struct ubus_context *ctx, struct ubus_object *obj,
                      struct ubus_request_data *req, const char *method,
                      struct blob_attr *msg)
{
	if (!g_state || !g_telemetry_ctx)
		return UBUS_STATUS_UNKNOWN_ERROR;

	struct blob_buf b = {};
	blob_buf_init(&b, 0);

	bool mqtt_connected;
	time_t last_report_time;
	uint64_t reports_sent;
	uint64_t reports_failed;
	unsigned int interval_msec;

	pthread_mutex_lock(&g_state->lock);
	mqtt_connected = g_state->mqtt_connected;
	last_report_time = g_state->last_report_time;
	reports_sent = g_state->reports_sent;
	reports_failed = g_state->reports_failed;
	pthread_mutex_unlock(&g_state->lock);

	pthread_mutex_lock(&g_telemetry_ctx->lock);
	interval_msec = g_telemetry_ctx->interval_msec;
	pthread_mutex_unlock(&g_telemetry_ctx->lock);

	blobmsg_add_u32(&b, "mqtt_connected", mqtt_connected ? 1 : 0);
	blobmsg_add_u64(&b, "last_report_time", (uint64_t)last_report_time);
	blobmsg_add_u64(&b, "reports_sent", reports_sent);
	blobmsg_add_u64(&b, "reports_failed", reports_failed);
	blobmsg_add_u32(&b, "interval_msec", interval_msec);

	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);

	return 0;
}

static int status_set_interval(struct ubus_context *ctx, struct ubus_object *obj,
                               struct ubus_request_data *req, const char *method,
                               struct blob_attr *msg)
{
	struct blob_attr *tb[__STATUS_SET_MAX];
	struct blob_buf b = {};

	blobmsg_parse(status_set_policy, __STATUS_SET_MAX, tb, blob_data(msg), blob_len(msg));
	if (!tb[STATUS_INTERVAL_MSEC]) {
		return UBUS_STATUS_INVALID_ARGUMENT;
	}

	unsigned int interval = blobmsg_get_u32(tb[STATUS_INTERVAL_MSEC]);
	if (interval == 0) {
		return UBUS_STATUS_INVALID_ARGUMENT;
	}

	if (telemetry_set_interval(g_telemetry_ctx, interval) != 0) {
		return UBUS_STATUS_UNKNOWN_ERROR;
	}

	blob_buf_init(&b, 0);
	blobmsg_add_u32(&b, "interval_msec", interval);
	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);

	return 0;
}

int ubus_api_init(struct ubus_context *uctx, telemetry_ctx_t *telemetry_ctx, daemon_state_t *state)
{
	int ret;

	if (!uctx || !telemetry_ctx || !state) {
		return -1;
	}

	g_telemetry_ctx = telemetry_ctx;
	g_state = state;

	ret = ubus_add_object(uctx, &status_object);
	if (ret != 0) {
		syslog(LOG_ERR, "ubus_add_object failed: %s (%d)", ubus_strerror(ret), ret);
		return -1;
	}

	return 0;
}
