#include "actions.h"
#include "action_handlers.h"
#include <cjson/cJSON.h>
#include <syslog.h>
#include <time.h>


static int send_action_response(tuya_mqtt_context_t * context, const tuyalink_message_t * request,
		int code, const char * error, cJSON * result)
{
	cJSON * response = NULL;
	cJSON * data = NULL;
	cJSON * output_params = NULL;
	cJSON * request_root = NULL;
	cJSON * action_code_json = NULL;
	cJSON * item = NULL;
	const char * action_code = NULL;
	char * payload = NULL;
	char topic[TUYA_MQTT_TOPIC_MAXLEN + TUYA_MQTT_DEVICE_ID_MAXLEN + 64];

	if(!context || !request) {
		syslog(LOG_WARNING, "send_action_response: null context or request");
		return -1;
	}

	response = cJSON_CreateObject();
	if(!response) {
		syslog(LOG_ERR, "send_action_response: failed to create response object");
		return -1;
	}
	
	cJSON_AddStringToObject(response, "msgId", request->msgid ? request->msgid : "");
	cJSON_AddNumberToObject(response, "time", (double)((long long)time(NULL) * 1000));
	cJSON_AddNumberToObject(response, "code", code);

	if(request->data_string) {
		request_root = cJSON_Parse(request->data_string);
		if(request_root) {
			action_code_json = cJSON_GetObjectItemCaseSensitive(request_root, "actionCode");
			if(cJSON_IsString(action_code_json) && action_code_json->valuestring) {
				action_code = action_code_json->valuestring;
			}
			cJSON_Delete(request_root);
			request_root = NULL;
		}
	}

	data = cJSON_CreateObject();
	if(!data) {
		syslog(LOG_ERR, "send_action_response: failed to create data object");
		cJSON_Delete(response);
		return -1;
	}

	if(action_code) {
		cJSON_AddStringToObject(data, "actionCode", action_code);
	}	

	if(result) {
		output_params = cJSON_CreateObject();
		if(output_params) {
			if(cJSON_IsObject(result)) {
				cJSON_ArrayForEach(item, result) {
					cJSON_AddItemToObject(output_params, item->string, cJSON_Duplicate(item, true));
				}
			} else {
				cJSON_AddItemToObject(output_params, "value", cJSON_Duplicate(result, true));
			}
			
			cJSON_AddItemToObject(data, "outputParams", output_params);
		}
	}

	if(action_code || result) {
		cJSON_AddItemToObject(response, "data", data);
	} else {
		cJSON_Delete(data);
	}

	snprintf(topic, sizeof(topic), "tylink/%s/thing/action/execute_response",
		request->device_id ? request->device_id : context->config.device_id);

	payload = cJSON_PrintUnformatted(response);
	cJSON_Delete(response);
	
	if(!payload) {
		syslog(LOG_ERR, "send_action_response: failed to print JSON");
		return -1;
	}
	
	syslog(LOG_DEBUG, "ACTION RESPONSE: %s", payload);

	uint16_t msgid = mqtt_client_publish(context->mqtt_client, topic, payload, strlen(payload), MQTT_QOS_1);
	free(payload);
	
	if(!msgid) {
		syslog(LOG_ERR, "send_action_response: failed to publish message");
		return -1;
	}
	
	return 0;
}

static void process_action(tuya_mqtt_context_t * context,  void * user_data, const tuyalink_message_t * request);

void on_connected(tuya_mqtt_context_t *context, void *user_data)
{
	(void)context;
	esp_ctx *ectxp = (esp_ctx *)user_data;
	syslog(LOG_INFO, "Connected to Tuya Cloud");
	return;
}
void on_disconnect(tuya_mqtt_context_t *context, void *user_data)
{
	(void)context;
	esp_ctx *ectxp = (esp_ctx *)user_data;
	syslog(LOG_WARNING, "Disconnected from Tuya Cloud");
	return;
}
void on_messages(tuya_mqtt_context_t * context, void * user_data, const tuyalink_message_t * msg)
{
	if(!msg) {
		syslog(LOG_WARNING, "on_messages: null message received");
		return;
	}
	if(!user_data) {
		syslog(LOG_WARNING, "on_messages: null user_data received");
		return;
	}

	esp_ctx *ectxp = (esp_ctx *)user_data;

	switch(msg->type) {
	case THING_TYPE_ACTION_EXECUTE:
		if(msg->data_string)
			process_action(context, user_data, msg);
		else
			syslog(LOG_WARNING, "on_messages: THING_TYPE_ACTION_EXECUTE with no data_string");
		break;
	default:
		syslog(LOG_DEBUG, "on_messages: ignoring message type %d", msg->type);
		break;
	}
	return;
}

static void process_action(tuya_mqtt_context_t * context, void * user_data, const tuyalink_message_t * request)
{
	action_error_t ret = ACTION_OK;
	cJSON *root = NULL;
	cJSON *action = NULL;
	cJSON *params = NULL;
	cJSON *result = NULL;
	const char *error = NULL;
	int code = 0;
	esp_ctx *ectxp = (esp_ctx *)user_data;

	if (!request || !request->data_string) {
		syslog(LOG_WARNING, "process_action: null request payload");
		return;
	}

	syslog(LOG_INFO, "process_action payload: %s",
		   request->data_string);

	root = cJSON_Parse(request->data_string);
	if (!root) {
		syslog(LOG_WARNING,
			   "process_action: invalid JSON payload: %s",
			   request->data_string);
		return;
	}

	if (!cJSON_IsObject(root)) {
		syslog(LOG_WARNING, "process_action: payload is not an object");
		cJSON_Delete(root);
		return;
	}
	
	action = cJSON_GetObjectItemCaseSensitive(root, "actionCode");
	params = cJSON_GetObjectItemCaseSensitive(root, "inputParams");

	if (!cJSON_IsString(action) || !action->valuestring) {
		syslog(LOG_WARNING, "process_action: missing actionCode");
		cJSON_Delete(root);
		return;
	}

	if (params && !cJSON_IsObject(params)) {
		syslog(LOG_WARNING, "process_action: inputParams is not an object");
		cJSON_Delete(root);
		return;
	}

	syslog(LOG_INFO, "process_action: actionCode=%s", action->valuestring);

	if (strcmp(action->valuestring, "switch_pin") == 0) {
		ret = handle_switch_pin(ectxp, params, &result);
	} else if (strcmp(action->valuestring, "get_measurements") == 0) {
		ret = handle_get_measurements(ectxp, params, &result);
	} else if (strcmp(action->valuestring, "get_devices") == 0) {
		ret = handle_get_devices(ectxp, params, &result);
	} else {
		syslog(LOG_WARNING, "process_action: unknown action '%s'", action->valuestring); 
		ret = ACTION_ERR_UNKNOWN_ACTION;
	}

	switch (ret) {
		case ACTION_OK:
			code = 0;
			break;

		case ACTION_ERR_INVALID_ARGUMENT:
			code = 2001;
			error = "invalid parameters";
			break;

		case ACTION_ERR_ESP_DATA_FAILED:
		case ACTION_ERR_ESP_DEVICE_FAILED:
		case ACTION_ERR_ESP_PIN_FAILED:
			code = 2002;
			error = "not found";
			break;

		case ACTION_ERR_UNKNOWN_ACTION:
		case ACTION_ERR_INVALID_ACTION_CODE:
			code = 2003;
			error = "unsupported action";
			break;

		default:
			code = 2000;
			error = "internal error";
			break;
	}
	if(ret != ACTION_OK)
		syslog(LOG_DEBUG, "DEBUG ACTION: %s", action_error_to_string(ret));

	if (send_action_response(context, request, code, error, result) != 0) {
		syslog(LOG_ERR, "process_action: failed to send response");
	}

	cJSON_Delete(root);
}