#ifndef CONFIG_H
#define CONFIG_H

#define ACTION_LOG_FILE "/tmp/tuya_action.log"
#define TUYA_MQTT_DEFAULT_TIMEOUT_MS 3000
#define TUYA_SERVER_DEFAULT_HOST "m1.tuyacn.com"
#define TUYA_SERVER_DEFAULT_PORT 8883
#define TUYA_SERVER_DEFAULT_KEEPALIVE 60
#define MQTT_TOTAL_FAILURE_TIMEOUT (6* 60 * 60) // timeout after 6 hours of continuous MQTT connection failures
#define DEFAULT_REPORT_INTERVAL_SEC 15

#endif