#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <uci.h>

int config_load(app_config_t * conf)
{
	if(!conf) return -1;

	conf->action_log_file = strdup(DEFAULT_ACTION_LOG_FILE);
	conf->mqtt_host = strdup(DEFAULT_SERVER_HOST);
	conf->ubus_timeout_ms = DEFAULT_UBUS_TIMEOUT_MS;
	conf->report_interval_ms = DEFAULT_REPORT_INTERVAL_MS;
	conf->mqtt_port = DEFAULT_SERVER_PORT;
	conf->mqtt_keepalive = DEFAULT_SERVER_KEEPALIVE;
	conf->telemetry_interval_ms = DEFAULT_TELEMETRY_INTERVAL_MS;
	conf->mqtt_failure_timeout_sec = DEFAULT_MQTT_TOTAL_FAILURE_TIMEOUT;
	conf->mqtt_initial_retry_delay = DEFAULT_MQTT_INITIAL_RETRY_DELAY_MS;
	conf->mqtt_backoff_cap_sec = DEFAULT_MQTT_BACKOFF_CAP_SEC;
	conf->log_level = LOG_INFO;
	
	if(!conf->action_log_file || !conf->mqtt_host) {
		return -1;
	}
	// TODO: Implement loading configuration with uci
	return 0;
}
int config_flush(app_config_t * conf) {
	// TODO: Implement with uci
	return 0;
}

void config_destroy(app_config_t * conf)
{
	if(!conf) return;
	free(conf->mqtt_host);
	free(conf->action_log_file);
	memset(conf, 0, sizeof(app_config_t));
	return;
}
int config_validate(const app_config_t * conf)
{
	if(!conf) return -1;

	if(conf->action_log_file == NULL) {
		syslog(LOG_ERR, "Invalid config: action_log_file is missing");
		return -1;
	}
		
	FILE * fp = fopen(conf->action_log_file, "a");
	if(fp == NULL) {
		syslog(LOG_ERR, "Invalid configuration (cannot open log file %s): %s", 
			   conf->action_log_file, strerror(errno));
		return -2;
	}
	fclose(fp);
	if(!(conf->log_level >= LOG_EMERG && conf->log_level <= LOG_DEBUG)) {
		syslog(LOG_ERR, "Invalid config: incorrect log_level (%d)", conf->log_level);
		return -3;
	}

	if(conf->mqtt_host == NULL || conf->mqtt_host[0] == '\0') {
		syslog(LOG_ERR, "Invalid config: mqtt_host must not be empty");
		return -4;
	}
		
	if(conf->mqtt_initial_retry_delay == 0) {
		syslog(LOG_ERR, "Invalid config: mqtt_initial_retry_delay cannot be 0");
		return -5;
	}

	if(conf->mqtt_initial_retry_delay > conf->mqtt_backoff_cap_sec) {
		syslog(LOG_ERR, "Invalid config: initial retry delay (%u) cannot exceed backoff cap (%u)",
			   conf->mqtt_initial_retry_delay, conf->mqtt_backoff_cap_sec);
		return -6;
	}

	if(conf->mqtt_failure_timeout_sec < conf->mqtt_initial_retry_delay) {
		syslog(LOG_ERR, "Invalid config: failure timeout (%u) is shorter than initial retry delay (%u)",
			   conf->mqtt_failure_timeout_sec, conf->mqtt_initial_retry_delay);
		return -7;
	}

	return 0;
}