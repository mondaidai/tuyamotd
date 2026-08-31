#ifndef ACTION_HANDLERS_H
#define ACTION_HANDLERS_H

#include <cjson/cJSON.h>
#include "tuyalink_core.h"
#include "esp_actions.h"
typedef enum {
	ACTION_OK = 0,
	ACTION_ERR_INVALID_CONTEXT = 1,
	ACTION_ERR_INVALID_ARGUMENT = 2,
	ACTION_ERR_NO_MEMORY = 3,
	ACTION_ERR_ESP_DATA_FAILED = 4,
	ACTION_ERR_ESP_PIN_FAILED = 5,
	ACTION_ERR_ESP_DEVICE_FAILED = 6,
	ACTION_ERR_UNSUPPORTED_STATE = 7,
	ACTION_ERR_MISSING_STATE = 8,
	ACTION_ERR_TUYA_REPORT_FAILED = 9,
	ACTION_ERR_INVALID_ACTION_CODE = 10,
	ACTION_ERR_UNKNOWN_ACTION = 11,
	ACTION_ERR_JSON_PARSE_FAILED = 12,
	ACTION_ERR_INTERNAL = 13
} action_error_t;

const char * action_error_to_string(action_error_t error);

action_error_t handle_get_measurements(esp_ctx * ectxp, cJSON * params, cJSON **result);
action_error_t handle_switch_pin(esp_ctx * ectxp, cJSON * params, cJSON **result);
action_error_t handle_get_devices(esp_ctx * ectxp, cJSON * params, cJSON **result);

#endif