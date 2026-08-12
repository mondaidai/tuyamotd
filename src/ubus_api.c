#include "ubus_api.h"
#include "telemetry.h"
#include "config.h"

#include <stdint.h>
#include <pthread.h>
#include <syslog.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <uci.h>

#include <libubox/blobmsg_json.h>
#include <libubus.h>

static telemetry_ctx_t * g_telemetry_ctx = NULL;
static daemon_state_t * g_state = NULL;

enum {
	CFG_NAME,
	CFG_VALUE,
	__CFG_MAX,
};

enum {
	SETCFG_MQTT_HOST,
	SETCFG_MQTT_PORT,
	SETCFG_MQTT_KEEPALIVE,
	SETCFG_MQTT_FAILURE_TIMEOUT_SEC,
	SETCFG_MQTT_INITIAL_RETRY_DELAY_SEC,
	SETCFG_MQTT_BACKOFF_CAP_SEC,
	SETCFG_REPORT_INTERVAL_MS,
	SETCFG_TELEMETRY_INTERVAL_MS,
	SETCFG_UBUS_TIMEOUT_MS,
	SETCFG_LOG_LEVEL,
	SETCFG_ACTION_LOG_FILE,
	__SETCFG_MAX,
};

static const struct blobmsg_policy cfg_policy[] = {
	[CFG_NAME] = {
		.name = "name",
		.type = BLOBMSG_TYPE_STRING
	},
	[CFG_VALUE] = {
		.name = "value",
		.type = BLOBMSG_TYPE_STRING
	},
};

static const struct blobmsg_policy set_config_policy[] = {
	[SETCFG_MQTT_HOST] = {
		.name = "mqtt_host",
		.type = BLOBMSG_TYPE_STRING
	},
	[SETCFG_MQTT_PORT] = {
		.name = "mqtt_port",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_MQTT_KEEPALIVE] = {
		.name = "mqtt_keepalive",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_MQTT_FAILURE_TIMEOUT_SEC] = {
		.name = "mqtt_failure_timeout_sec",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_MQTT_INITIAL_RETRY_DELAY_SEC] = {
		.name = "mqtt_initial_retry_delay_sec",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_MQTT_BACKOFF_CAP_SEC] = {
		.name = "mqtt_backoff_cap_sec",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_REPORT_INTERVAL_MS] = {
		.name = "report_interval_ms",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_TELEMETRY_INTERVAL_MS] = {
		.name = "telemetry_interval_ms",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_UBUS_TIMEOUT_MS] = {
		.name = "ubus_timeout_ms",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_LOG_LEVEL] = {
		.name = "log_level",
		.type = BLOBMSG_TYPE_INT32
	},
	[SETCFG_ACTION_LOG_FILE] = {
		.name = "action_log_file",
		.type = BLOBMSG_TYPE_STRING
	},
};

static int status_get(struct ubus_context *ctx,
					  struct ubus_object *obj,
					  struct ubus_request_data *req,
					  const char *method,
					  struct blob_attr *msg);

static int get_setting(struct ubus_context *ctx,
					   struct ubus_object *obj,
					   struct ubus_request_data *req,
					   const char *method,
					   struct blob_attr *msg);

static int set_setting(struct ubus_context *ctx,
					   struct ubus_object *obj,
					   struct ubus_request_data *req,
					   const char *method,
					   struct blob_attr *msg);

static int set_config(struct ubus_context *ctx,
					  struct ubus_object *obj,
					  struct ubus_request_data *req,
					  const char *method,
					  struct blob_attr *msg);

static int get_config(struct ubus_context *ctx,
					  struct ubus_object *obj,
					  struct ubus_request_data *req,
					  const char *method,
					  struct blob_attr *msg);

static const struct ubus_method status_methods[] = {
	UBUS_METHOD_NOARG("status", status_get),
	UBUS_METHOD_NOARG("get_config", get_config),
	UBUS_METHOD("get_setting", get_setting, cfg_policy),
	UBUS_METHOD("set_setting", set_setting, cfg_policy),
	UBUS_METHOD("set_config", set_config, set_config_policy),
};

static struct ubus_object_type status_object_type =
	UBUS_OBJECT_TYPE("tuyamotd", status_methods);

static struct ubus_object status_object = {
	.name = "tuyamotd",
	.type = &status_object_type,
	.methods = status_methods,
	.n_methods = ARRAY_SIZE(status_methods),
};


static int status_get(struct ubus_context * ctx, struct ubus_object * obj,
					  struct ubus_request_data * req, const char * method,
					  struct blob_attr * msg)
{
	(void)obj;
	(void)method;
	(void)msg;

	if (!g_state || !g_telemetry_ctx)
		return UBUS_STATUS_UNKNOWN_ERROR;

	struct blob_buf b = {};

	bool mqtt_connected;
	time_t last_report_time;
	uint64_t reports_sent;
	uint64_t reports_failed;
	unsigned int interval_msec;

	blob_buf_init(&b, 0);

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
	blobmsg_add_u32(&b, "telemetry_interval_ms", interval_msec);

	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);

	return 0;
}

static int get_setting(struct ubus_context * ctx, struct ubus_object * obj,
					   struct ubus_request_data * req, const char * method,
					   struct blob_attr * msg)
{
	(void)obj;
	(void)method;

	struct blob_attr *tb[__CFG_MAX] = {};
	struct blob_buf b = {};
	char *value = NULL;
	int rc;

	if (!msg)
		return UBUS_STATUS_INVALID_ARGUMENT;

	blobmsg_parse(cfg_policy,
				  __CFG_MAX,
				  tb,
				  blob_data(msg),
				  blob_len(msg));

	if (!tb[CFG_NAME])
		return UBUS_STATUS_INVALID_ARGUMENT;

	const char *name = blobmsg_get_string(tb[CFG_NAME]);

	rc = config_get_field(name, &value);
	if (rc)
		return UBUS_STATUS_NOT_FOUND;

	blob_buf_init(&b, 0);

	blobmsg_add_string(&b, "name", name);
	blobmsg_add_string(&b, "value", value ? value : "");

	ubus_send_reply(ctx, req, b.head);

	free(value);
	blob_buf_free(&b);

	return 0;
}

static int set_setting(struct ubus_context * ctx, struct ubus_object * obj,
					   struct ubus_request_data * req, const char *method,
					   struct blob_attr * msg)
{
	(void)obj;
	(void)method;

	struct blob_attr *tb[__CFG_MAX] = {};
	struct blob_buf b = {};
	int rc;

	if (!msg)
		return UBUS_STATUS_INVALID_ARGUMENT;

	blobmsg_parse(cfg_policy, __CFG_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[CFG_NAME] || !tb[CFG_VALUE])
		return UBUS_STATUS_INVALID_ARGUMENT;

	const char *name = blobmsg_get_string(tb[CFG_NAME]);
	const char *value = blobmsg_get_string(tb[CFG_VALUE]);

	rc = config_set_field(name, value);
	if (rc != UCI_OK)
		return UBUS_STATUS_UNKNOWN_ERROR;

	blob_buf_init(&b, 0);

	blobmsg_add_string(&b, "name", name);
	blobmsg_add_string(&b, "value", value);

	ubus_send_reply(ctx, req, b.head);

	blob_buf_free(&b);

	return 0;
}

static bool is_integer_type(struct blob_attr * attr)
{
	int type = blobmsg_type(attr);

	return type == BLOBMSG_TYPE_INT8 ||
		   type == BLOBMSG_TYPE_INT16 ||
		   type == BLOBMSG_TYPE_INT32 ||
		   type == BLOBMSG_TYPE_INT64;
}

static int set_config(struct ubus_context * ctx,
					  struct ubus_object * obj,
					  struct ubus_request_data * req,
					  const char * method,
					  struct blob_attr * msg)
{
	(void)ctx;
	(void)obj;
	(void)req;
	(void)method;

	app_config_t conf;
	struct blob_attr *tb[__SETCFG_MAX] = {};
	int rem;

	if (!msg)
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (config_load(&conf))
		return UBUS_STATUS_UNKNOWN_ERROR;

	blobmsg_parse(set_config_policy,
				  __SETCFG_MAX,
				  tb,
				  blob_data(msg),
				  blob_len(msg));

	if (tb[SETCFG_MQTT_HOST]) {
		const char *value = blobmsg_get_string(tb[SETCFG_MQTT_HOST]);
		if (!value) {
			config_destroy(&conf);
			return UBUS_STATUS_INVALID_ARGUMENT;
		}

		char *tmp = strdup(value);
		if (!tmp) {
			config_destroy(&conf);
			return UBUS_STATUS_UNKNOWN_ERROR;
		}

		free(conf.mqtt_host);
		conf.mqtt_host = tmp;
	}

	if (tb[SETCFG_MQTT_PORT]) {
		conf.mqtt_port = blobmsg_get_u32(tb[SETCFG_MQTT_PORT]);
	}

	if (tb[SETCFG_MQTT_KEEPALIVE]) {
		conf.mqtt_keepalive = blobmsg_get_u32(tb[SETCFG_MQTT_KEEPALIVE]);
	}

	if (tb[SETCFG_MQTT_FAILURE_TIMEOUT_SEC]) {
		conf.mqtt_failure_timeout_sec = blobmsg_get_u32(tb[SETCFG_MQTT_FAILURE_TIMEOUT_SEC]);
	}

	if (tb[SETCFG_MQTT_INITIAL_RETRY_DELAY_SEC]) {
		conf.mqtt_initial_retry_delay = blobmsg_get_u32(tb[SETCFG_MQTT_INITIAL_RETRY_DELAY_SEC]);
	}

	if (tb[SETCFG_MQTT_BACKOFF_CAP_SEC]) {
		conf.mqtt_backoff_cap_sec = blobmsg_get_u32(tb[SETCFG_MQTT_BACKOFF_CAP_SEC]);
	}

	if (tb[SETCFG_REPORT_INTERVAL_MS]) {
		conf.report_interval_ms = blobmsg_get_u32(tb[SETCFG_REPORT_INTERVAL_MS]);
	}

	if (tb[SETCFG_TELEMETRY_INTERVAL_MS]) {
		conf.telemetry_interval_ms = blobmsg_get_u32(tb[SETCFG_TELEMETRY_INTERVAL_MS]);
	}

	if (tb[SETCFG_UBUS_TIMEOUT_MS]) {
		conf.ubus_timeout_ms = blobmsg_get_u32(tb[SETCFG_UBUS_TIMEOUT_MS]);
	}

	if (tb[SETCFG_LOG_LEVEL]) {
		conf.log_level = blobmsg_get_u32(tb[SETCFG_LOG_LEVEL]);
	}

	if (tb[SETCFG_ACTION_LOG_FILE]) {
		const char *value = blobmsg_get_string(tb[SETCFG_ACTION_LOG_FILE]);
		if (!value) {
			config_destroy(&conf);
			return UBUS_STATUS_INVALID_ARGUMENT;
		}

		char *tmp = strdup(value);
		if (!tmp) {
			config_destroy(&conf);
			return UBUS_STATUS_UNKNOWN_ERROR;
		}

		free(conf.action_log_file);
		conf.action_log_file = tmp;
	}

	if (config_flush(&conf) != UCI_OK) {
		config_destroy(&conf);
		return UBUS_STATUS_UNKNOWN_ERROR;
	}

	config_destroy(&conf);

	return 0;
}

static int get_config(struct ubus_context *ctx, struct ubus_object *obj,
					  struct ubus_request_data * req, const char * method,
					  struct blob_attr *msg)
{
	(void)obj;
	(void)method;
	(void)msg;

	app_config_t conf;
	struct blob_buf b = {};

	if (config_load(&conf))
		return UBUS_STATUS_UNKNOWN_ERROR;

	blob_buf_init(&b, 0);

	blobmsg_add_string(&b, "mqtt_host", conf.mqtt_host ? conf.mqtt_host : "");
	blobmsg_add_u32(&b, "mqtt_port", conf.mqtt_port);
	blobmsg_add_u32(&b, "mqtt_keepalive", conf.mqtt_keepalive);
	blobmsg_add_u32(&b, "mqtt_failure_timeout_sec", conf.mqtt_failure_timeout_sec);
	blobmsg_add_u32(&b, "mqtt_initial_retry_delay_sec", conf.mqtt_initial_retry_delay);
	blobmsg_add_u32(&b, "mqtt_backoff_cap_sec", conf.mqtt_backoff_cap_sec);
	blobmsg_add_u32(&b, "report_interval_ms", conf.report_interval_ms);
	blobmsg_add_u32(&b, "telemetry_interval_ms", conf.telemetry_interval_ms);
	blobmsg_add_u32(&b, "ubus_timeout_ms", conf.ubus_timeout_ms);
	blobmsg_add_u32(&b, "log_level", conf.log_level);
	blobmsg_add_string(&b, "action_log_file", conf.action_log_file ? conf.action_log_file : "");

	ubus_send_reply(ctx, req, b.head);

	blob_buf_free(&b);
	config_destroy(&conf);

	return 0;
}

int ubus_api_init(struct ubus_context *uctx, telemetry_ctx_t *telemetry_ctx, daemon_state_t *state)
{
	int ret;

	if (!uctx || !telemetry_ctx || !state)
		return -1;

	g_telemetry_ctx = telemetry_ctx;
	g_state = state;

	ret = ubus_add_object(uctx, &status_object);
	if (ret) {
		syslog(LOG_ERR,
			   "ubus_add_object failed: %s (%d)",
			   ubus_strerror(ret),
			   ret);
		return -1;
	}

	return 0;
}
