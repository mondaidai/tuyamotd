#include "config.h"
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <uci.h>

static int check_uci_ptr(int rc, const struct uci_ptr * ptr)
{
	if (rc != UCI_OK)
		return rc;

	if (ptr == NULL || ptr->o == NULL)
		return 1;

	return 0;
}

static int load_string_option(
	struct uci_context *ctx,
	const char *option_name,
	const char *default_value,
	char **out)
{
	char option[128];
	struct uci_ptr ptr = {0};
	int rc;

	snprintf(option, sizeof(option), "%s", option_name);

	rc = uci_lookup_ptr(ctx, &ptr, option, true);

	if (check_uci_ptr(rc, &ptr) || ptr.o->v.string == NULL) {
		syslog(LOG_WARNING,
			   "Invalid config: failed to find option %s, using default %s",
			   option,
			   default_value);

		*out = strdup(default_value);
	} else {
		*out = strdup(ptr.o->v.string);
	}

	return (*out != NULL) ? 0 : -1;
}

static void load_u16_option(
	struct uci_context *ctx,
	const char *option_name,
	uint16_t default_value,
	uint16_t *out)
{
	char option[128];
	struct uci_ptr ptr = {0};
	int rc;
	long val;
	char *end;

	*out = default_value;

	snprintf(option, sizeof(option), "%s", option_name);

	rc = uci_lookup_ptr(ctx, &ptr, option, true);

	if (check_uci_ptr(rc, &ptr)) {
		syslog(LOG_WARNING,
			   "Invalid config: failed to find option %s, using default %u",
			   option,
			   (unsigned)default_value);
		return;
	}

	errno = 0;
	val = strtol(ptr.o->v.string, &end, 10);

	if (errno == ERANGE ||
		end == ptr.o->v.string ||
		*end != '\0' ||
		val < 0 ||
		val > UINT16_MAX)
	{
		syslog(LOG_WARNING,
			   "Invalid config: invalid option value for %s: %s, using default %u",
			   option,
			   ptr.o->v.string,
			   (unsigned)default_value);
		return;
	}

	*out = (uint16_t)val;
}

static void load_log_level_option(
	struct uci_context *ctx,
	const char *option_name,
	int default_value,
	int *out)
{
	char option[128];
	struct uci_ptr ptr = {0};
	int rc;
	char *end = NULL;
	long val;
	const struct {
		const char *name;
		int value;
	} levels[] = {
		{ "LOG_EMERG", LOG_EMERG },
		{ "LOG_ALERT", LOG_ALERT },
		{ "LOG_CRIT", LOG_CRIT },
		{ "LOG_ERR", LOG_ERR },
		{ "LOG_WARNING", LOG_WARNING },
		{ "LOG_NOTICE", LOG_NOTICE },
		{ "LOG_INFO", LOG_INFO },
		{ "LOG_DEBUG", LOG_DEBUG },
	};

	snprintf(option, sizeof(option), "%s", option_name);

	rc = uci_lookup_ptr(ctx, &ptr, option, true);

	if(check_uci_ptr(rc, &ptr)) {
		syslog(LOG_WARNING,
			   "Invalid config: failed to find option %s, using default %d",
			   option,
			   default_value);
		*out = default_value;
		return;
	}

	if(ptr.o->v.string == NULL) {
		syslog(LOG_WARNING,
			   "Invalid config: empty option value for %s, using default %d",
			   option,
			   default_value);
		*out = default_value;
		return;
	}

	for(size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
		if(strcasecmp(ptr.o->v.string, levels[i].name) == 0) {
			*out = levels[i].value;
			return;
		}
	}

	errno = 0;
	val = strtol(ptr.o->v.string, &end, 10);
	if(errno == ERANGE || end == ptr.o->v.string || *end != '\0' || val < LOG_EMERG || val > LOG_DEBUG) {
		syslog(LOG_WARNING,
			   "Invalid config: invalid option value for %s: %s, using default %d",
			   option,
			   ptr.o->v.string,
			   default_value);
		*out = default_value;
		return;
	}

	*out = (int)val;
}

static void load_u32_option(
	struct uci_context *ctx,
	const char *option_name,
	uint32_t default_value,
	uint32_t *out)
{
	char option[128];
	struct uci_ptr ptr = {0};
	int rc;
	unsigned long val;
	char *end;

	*out = default_value;

	snprintf(option, sizeof(option), "%s", option_name);

	rc = uci_lookup_ptr(ctx, &ptr, option, true);

	if (check_uci_ptr(rc, &ptr)) {
		syslog(LOG_WARNING,
			   "Invalid config: failed to find option %s, using default %lu",
			   option,
			   (unsigned long)default_value);
		return;
	}

	errno = 0;
	val = strtoul(ptr.o->v.string, &end, 10);

	if (errno == ERANGE ||
		end == ptr.o->v.string ||
		*end != '\0' ||
		val > UINT32_MAX)
	{
		syslog(LOG_WARNING,
			   "Invalid config: invalid option value for %s: %s, using default %lu",
			   option,
			   ptr.o->v.string,
			   (unsigned long)default_value);
		return;
	}

	*out = (uint32_t)val;
}

int config_load(app_config_t * conf)
{
	struct uci_context * cursor;
	struct uci_package * pkg = NULL;

	if(!conf) return -1;

	conf->mqtt_host = NULL;
	conf->action_log_file = NULL;

	conf->ubus_timeout_ms = DEFAULT_UBUS_TIMEOUT_MS;
	conf->report_interval_ms = DEFAULT_REPORT_INTERVAL_MS;
	conf->mqtt_port = DEFAULT_SERVER_PORT;
	conf->mqtt_keepalive = DEFAULT_SERVER_KEEPALIVE;
	conf->telemetry_interval_ms = DEFAULT_TELEMETRY_INTERVAL_MS;
	conf->mqtt_failure_timeout_sec = DEFAULT_MQTT_TOTAL_FAILURE_TIMEOUT;
	conf->mqtt_initial_retry_delay = DEFAULT_MQTT_INITIAL_RETRY_DELAY_SEC;
	conf->mqtt_backoff_cap_sec = DEFAULT_MQTT_BACKOFF_CAP_SEC;
	conf->log_level = LOG_INFO;

	cursor = uci_alloc_context();
	if(!cursor) return -1;

	if(uci_load(cursor, "tuyamotd", &pkg) != UCI_OK) {
		syslog(LOG_WARNING, "Failed to load UCI config package tuyamotd, using defaults");
		uci_free_context(cursor);
		return -2;
	}

	if(load_string_option(cursor,
						   "tuyamotd.mqtt.host",
						   DEFAULT_SERVER_HOST,
						   &conf->mqtt_host))
		goto error;

	if(load_string_option(cursor,
						   "tuyamotd.log.file",
						   DEFAULT_ACTION_LOG_FILE,
						   &conf->action_log_file))
		goto error;

	load_u16_option(cursor,
					"tuyamotd.mqtt.port",
					DEFAULT_SERVER_PORT,
					&conf->mqtt_port);
	load_u16_option(cursor,
					"tuyamotd.mqtt.keepalive",
					DEFAULT_SERVER_KEEPALIVE,
					&conf->mqtt_keepalive);
	load_u16_option(cursor,
					"tuyamotd.mqtt.failure_timeout_sec",
					DEFAULT_MQTT_TOTAL_FAILURE_TIMEOUT,
					&conf->mqtt_failure_timeout_sec);
	load_u16_option(cursor,
					"tuyamotd.mqtt.initial_retry_delay_sec",
					DEFAULT_MQTT_INITIAL_RETRY_DELAY_SEC,
					&conf->mqtt_initial_retry_delay);
	load_u16_option(cursor,
					"tuyamotd.mqtt.backoff_cap_sec",
					DEFAULT_MQTT_BACKOFF_CAP_SEC,
					&conf->mqtt_backoff_cap_sec);
	load_u32_option(cursor,
					"tuyamotd.telemetry.report_interval_ms",
					DEFAULT_REPORT_INTERVAL_MS,
					&conf->report_interval_ms);
	load_u32_option(cursor,
					"tuyamotd.telemetry.telemetry_interval_ms",
					DEFAULT_TELEMETRY_INTERVAL_MS,
					&conf->telemetry_interval_ms);
	load_u32_option(cursor,
					"tuyamotd.system.ubus_timeout_ms",
					DEFAULT_UBUS_TIMEOUT_MS,
					&conf->ubus_timeout_ms);
	load_log_level_option(cursor,
					"tuyamotd.log.level",
					LOG_INFO,
					&conf->log_level);

	uci_unload(cursor, pkg);
	uci_free_context(cursor);
	return 0;

error:
	if(pkg) uci_unload(cursor, pkg);
	uci_free_context(cursor);

	free(conf->mqtt_host);
	free(conf->action_log_file);

	conf->mqtt_host = NULL;
	conf->action_log_file = NULL;

	return -1;
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

	switch(conf->log_level)
	{
		case LOG_EMERG:
		case LOG_ALERT:
		case LOG_CRIT:
		case LOG_ERR:
		case LOG_WARNING:
		case LOG_NOTICE:
		case LOG_INFO:
		case LOG_DEBUG:
			break;

		default:
			syslog(LOG_ERR,
				"Invalid config: incorrect log_level (%d)",
				conf->log_level);
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

static int config_set_field_internal(struct uci_context * cursor, const char * field, const char * value) 
{
	int rc = 0;
	struct uci_ptr ptr = {0};
	char * option = strdup(field);
	if(!option) return -1;

	rc = uci_lookup_ptr(cursor, &ptr, option, true);
	if(rc != UCI_OK) {
		char *err = NULL;	
		uci_get_errorstr(cursor, &err, NULL);
		syslog(LOG_ERR,
		"UCI failed: %s",
		err ? err : "unknown error");
		free(err);
		goto clean_up;
	}
	if(ptr.o != NULL) {
	syslog(LOG_INFO,
		"Setting value for %s Value -> %s",
		option,
		ptr.o->v.string ? ptr.o->v.string : "<null>");
	} else {
		syslog(LOG_WARNING, "Option doesn't exists. New option will be added to config");
	}

	ptr.value = strdup(value);
	if(!ptr.value) {
		rc = -1;
		goto clean_up;
	}

	rc = uci_set(cursor, &ptr);
	if (rc != UCI_OK)
		goto clean_up;

clean_up:
	free((void *)ptr.value);
	ptr.value = NULL;
	free(option);
	return rc;
}

int config_set_field(const char * field, const char * value)
{
	int rc;
	struct uci_context *cursor;

	if (!field || !value)
		return -EINVAL;

	cursor = uci_alloc_context();
	if (!cursor)
		return -ENOMEM;

	rc = config_set_field_internal(cursor, field, value);
	if (rc != UCI_OK)
		goto cleanup;

	struct uci_ptr ptr = {0};
	char *tmp = strdup(field);

	if (!tmp) {
		rc = -ENOMEM;
		goto cleanup;
	}

	rc = uci_lookup_ptr(cursor, &ptr, tmp, true);
	free(tmp);

	if (rc == UCI_OK) {
		rc = uci_commit(cursor, &ptr.p, false);
	}

cleanup:
	uci_free_context(cursor);
	return rc;
}

int config_flush(app_config_t *conf)
{
	int rc;
	char buf[32];
	struct uci_context *cursor;
	struct uci_package *pkg = NULL;

	if (!conf)
		return -EINVAL;

	rc = config_validate(conf);
	if (rc != 0)
		return rc;

	cursor = uci_alloc_context();
	if (!cursor)
		return -ENOMEM;

	rc = config_set_field_internal(
			cursor,
			"tuyamotd.mqtt.host",
			conf->mqtt_host);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->mqtt_port);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.mqtt.port",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->mqtt_keepalive);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.mqtt.keepalive",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->mqtt_failure_timeout_sec);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.mqtt.failure_timeout_sec",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->mqtt_initial_retry_delay);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.mqtt.initial_retry_delay_sec",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->mqtt_backoff_cap_sec);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.mqtt.backoff_cap_sec",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->report_interval_ms);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.telemetry.report_interval_ms",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->telemetry_interval_ms);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.telemetry.telemetry_interval_ms",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%u", conf->ubus_timeout_ms);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.system.ubus_timeout_ms",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	snprintf(buf, sizeof(buf), "%d", conf->log_level);
	rc = config_set_field_internal(
			cursor,
			"tuyamotd.log.level",
			buf);
	if (rc != UCI_OK)
		goto cleanup;

	rc = config_set_field_internal(
			cursor,
			"tuyamotd.log.file",
			conf->action_log_file);
	if (rc != UCI_OK)
		goto cleanup;

	if (uci_load(cursor, "tuyamotd", &pkg) != UCI_OK) {
		rc = UCI_ERR_NOTFOUND;
		goto cleanup;
	}

	rc = uci_commit(cursor, &pkg, false);

cleanup:
	if (pkg)
		uci_unload(cursor, pkg);

	uci_free_context(cursor);
	return rc;
}

int config_get_field(const char * field, char ** value)
{
    struct uci_context *cursor;
    struct uci_ptr ptr = {0};
    char *option;
    int rc;

    if (!field || !value)
        return -EINVAL;

    *value = NULL;

    cursor = uci_alloc_context();
    if (!cursor)
        return -ENOMEM;

    option = strdup(field);
    if (!option) {
        uci_free_context(cursor);
        return -ENOMEM;
    }

    rc = uci_lookup_ptr(cursor, &ptr, option, true);
    if (rc != UCI_OK) {
        char *err = NULL;

        uci_get_errorstr(cursor, &err, NULL);

        syslog(LOG_ERR,
               "UCI lookup failed: %s",
               err ? err : "unknown error");

        free(err);
        free(option);
        uci_free_context(cursor);
        return -ENOENT;
    }

    if (!ptr.o || ptr.o->type != UCI_TYPE_STRING || !ptr.o->v.string) {
        free(option);
        uci_free_context(cursor);
        return -ENOENT;
    }

    *value = strdup(ptr.o->v.string);

    free(option);
    uci_free_context(cursor);

    if (!*value)
        return -ENOMEM;

    return 0;
}