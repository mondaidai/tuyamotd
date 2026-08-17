#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>

typedef struct {
	char * 		action_log_file;
	char * 		mqtt_host;
	uint32_t	ubus_timeout_ms;
	uint32_t	report_interval_ms;
	uint32_t	telemetry_interval_ms;
	uint16_t	mqtt_port;
	uint16_t	mqtt_keepalive;
	uint16_t	mqtt_failure_timeout_sec;
	uint16_t	mqtt_initial_retry_delay;
	uint16_t	mqtt_backoff_cap_sec;
	int 		log_level;
} app_config_t;

extern app_config_t g_app_config;

#define DEFAULT_SERVER_HOST "m1.tuyacn.com"
#define DEFAULT_SERVER_PORT 8883
#define DEFAULT_SERVER_KEEPALIVE 60
#define DEFAULT_REPORT_INTERVAL_MS 15000
#define DEFAULT_UBUS_TIMEOUT_MS 5000

#define DEFAULT_TELEMETRY_INTERVAL_MS 5000
#define DEFAULT_MQTT_TOTAL_FAILURE_TIMEOUT (6*60*60)
#define DEFAULT_MQTT_INITIAL_RETRY_DELAY_SEC 1
#define DEFAULT_MQTT_BACKOFF_CAP_SEC (30*60)
#define DEFAULT_ACTION_LOG_FILE "/tmp/tuya_action.log"

#define DEFAULT_MQTT_TIMEOUT_MS 3000

int config_load(app_config_t * conf);
int config_flush(app_config_t * conf);
void config_destroy(app_config_t * conf);
int config_validate(const app_config_t *conf);
int config_set_field(const char * field, const char * value);
int config_get_field(const char *field, char **value);
#endif