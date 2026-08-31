#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>

typedef struct {
	char * 		mqtt_host;
	uint32_t	ubus_timeout_ms;
	uint16_t	mqtt_port;
	uint16_t	mqtt_keepalive;
	int 		log_level;
} app_config_t;

extern app_config_t g_app_config;

#define DEFAULT_SERVER_HOST "m1.tuyacn.com"
#define DEFAULT_SERVER_PORT 8883
#define DEFAULT_SERVER_KEEPALIVE 60
#define DEFAULT_UBUS_TIMEOUT_MS 5000

#define DEFAULT_MQTT_TIMEOUT_MS 1000

int config_load(app_config_t * conf);
int config_flush(app_config_t * conf);
void config_destroy(app_config_t * conf);
int config_validate(const app_config_t *conf);
int config_set_field(const char * field, const char * value);
int config_get_field(const char *field, char **value);

#endif