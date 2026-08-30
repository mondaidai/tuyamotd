#include "actions.h"
#include "action_handlers.h"
#include <cjson/cJSON.h>
#include <syslog.h>

static void process_action(tuya_mqtt_context_t * context,  void * user_data, const char * payload);

void on_connected(tuya_mqtt_context_t *context, void *user_data)
{
	(void)context;
	(void)user_data;
	syslog(LOG_INFO, "Connected to Tuya Cloud");
	return;
}
void on_disconnect(tuya_mqtt_context_t *context, void *user_data)
{
	(void)context;
	(void)user_data;
	syslog(LOG_WARNING, "Disconnected from Tuya Cloud");
	return;
}
void on_messages(tuya_mqtt_context_t * context, void * user_data, const tuyalink_message_t * msg)
{
	if(!msg) {
		syslog(LOG_WARNING, "on_messages: null message received");
		return;
	}

	switch(msg->type) {
	case THING_TYPE_ACTION_EXECUTE:
		if(msg->data_string)
			process_action(context, user_data, msg->data_string);
		else
			syslog(LOG_WARNING, "on_messages: THING_TYPE_ACTION_EXECUTE with no data_string");
		break;
	default:
		syslog(LOG_DEBUG, "on_messages: ignoring message type %d", msg->type);
		break;
	}
	return;
}

static void process_action(tuya_mqtt_context_t * context,  void * user_data, const char * payload)
{
	action_error_t ret = ACTION_OK;
	cJSON * root = NULL;
	cJSON * action = NULL;
	cJSON * params = NULL;

	if(!payload) {
		syslog(LOG_WARNING, "process_action: null payload");
		return;
	}

	root = cJSON_Parse(payload);
	if(!root) {
		syslog(LOG_WARNING, "process_action: invalid JSON payload: %s", payload);
		return;
	}

	action = cJSON_GetObjectItemCaseSensitive(root, "actionCode");
	params = cJSON_GetObjectItemCaseSensitive(root, "inputParams");

	if(cJSON_IsString(action) && action->valuestring) {
		if(strcmp(action->valuestring, "switch_pin") == 0) {
			ret = handle_switch_pin(context, user_data, params);
		} else if(strcmp(action->valuestring, "get_measurements") == 0) {
			ret = handle_get_measurements(context, user_data, params);
		} else if(strcmp(action->valuestring, "get_devices") == 0) {
			ret = handle_get_devices(context, user_data, params);
		} else {
			ret = ACTION_ERR_UNKNOWN_ACTION;
		}
	} else {
		ret = ACTION_ERR_INVALID_ACTION_CODE;
	}

	if(ret != ACTION_OK) {
		syslog(LOG_ERR, "process_action: action %s failed: %s",
			cJSON_IsString(action) && action->valuestring ? action->valuestring : "<unknown>",
			action_error_to_string(ret));
	}

	cJSON_Delete(root);
	return;
}