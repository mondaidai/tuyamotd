#include "tuya_worker.h"
#include "daemon_state.h"
#include "signals.h"
#include "monitoring.h"
#include "tuya_error_code.h"
#include "tuyalink_core.h"
#include "config.h"
#include "actions.h"
#include <time.h>
#include <syslog.h>

void * tuya_worker(void * arg)
{
	tuya_worker_ctx_t *ctx = arg;
	int ret;

	const app_config_t *cfg = ctx && ctx->config ? ctx->config : NULL;
	const unsigned int mqtt_backoff_cap = cfg && cfg->mqtt_backoff_cap_sec ? cfg->mqtt_backoff_cap_sec : DEFAULT_MQTT_BACKOFF_CAP_SEC;
	unsigned int mqtt_retry_sleep = cfg && cfg->mqtt_initial_retry_delay ? cfg->mqtt_initial_retry_delay : DEFAULT_MQTT_INITIAL_RETRY_DELAY_SEC;
	time_t mqtt_failure_start = 0;

	cpu_usage_init();

	while (keep_running) {
		ret = tuya_mqtt_loop(ctx->client);

		if (ret == OPRT_OK) {
			pthread_mutex_lock(&ctx->state->lock);
			ctx->state->mqtt_connected = true;
			pthread_mutex_unlock(&ctx->state->lock);

			mqtt_failure_start = 0;
			mqtt_retry_sleep = 1;

			if (ctx->telemetry) {
				system_info_t report_info = {0};
				if (telemetry_take(&report_info, ctx->telemetry)) {
					ret = send_system_info(ctx->client, &report_info);
					if (ret == 0) {
						pthread_mutex_lock(&ctx->state->lock);
						ctx->state->reports_sent++;
						ctx->state->last_report_time = time(NULL);
						pthread_mutex_unlock(&ctx->state->lock);
					} else {
						syslog(LOG_WARNING, "Failed to send system information: %d", ret);
						pthread_mutex_lock(&ctx->state->lock);
						ctx->state->reports_failed++;
						pthread_mutex_unlock(&ctx->state->lock);
					}
					free_system_info(&report_info);
				}
			}

			continue;
		}

		pthread_mutex_lock(&ctx->state->lock);
		ctx->state->mqtt_connected = false;
		pthread_mutex_unlock(&ctx->state->lock);

		if (ret == OPRT_INVALID_PARM) {
			syslog(LOG_ERR, "tuya_mqtt_loop invalid parameter");
			break;
		}

		if (mqtt_failure_start == 0) {
			mqtt_failure_start = time(NULL);
		} else if ((time(NULL) - mqtt_failure_start) >= (cfg ? cfg->mqtt_failure_timeout_sec : DEFAULT_MQTT_TOTAL_FAILURE_TIMEOUT)) {
			syslog(LOG_ERR, "MQTT failing for too long");
			break;
		}

		syslog(LOG_WARNING, "tuya_mqtt_loop returned %d, retrying in %u seconds", ret, mqtt_retry_sleep);

		for (unsigned int i = 0; i < mqtt_retry_sleep && keep_running; i++) {
			sleep(1);
		}

		if (!keep_running)
			break;

		if (mqtt_retry_sleep < mqtt_backoff_cap) {
			mqtt_retry_sleep *= 2;
			if (mqtt_retry_sleep > mqtt_backoff_cap)
				mqtt_retry_sleep = mqtt_backoff_cap;
		}
	}

	pthread_mutex_lock(&ctx->state->lock);
	ctx->state->mqtt_connected = false;
	pthread_mutex_unlock(&ctx->state->lock);
	return NULL;
}
