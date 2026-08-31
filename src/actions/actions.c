#include "actions.h"
#include "action_handlers.h"
#include <cjson/cJSON.h>
#include <syslog.h>
#include <time.h>

static int send_action_response(tuya_mqtt_context_t *context,
		const tuyalink_message_t * request,
		int code,
		const char * error,
		cJSON * result)
{
	cJSON *response = cJSON_CreateObject();
	cJSON *data_copy = NULL;
	char *payload;
	char topic[TUYA_MQTT_TOPIC_MAXLEN + TUYA_MQTT_DEVICE_ID_MAXLEN + 64];

	if(!context || !request || !response)
		return -1;
	cJSON_AddStringToObject(response, "msgId", request->msgid != NULL ? request->msgid : "");
	cJSON_AddNumberToObject(response, "time", (double)time(NULL) * 1000);
	cJSON_AddNumberToObject(response, "code", code);
	if(error)
		cJSON_AddStringToObject(response, "error", error);
	if(result) {
		data_copy = cJSON_Duplicate(result, true);
		if(data_copy)
			cJSON_AddItemToObject(response, "data", data_copy);
	}

	payload = cJSON_PrintUnformatted(response);
	cJSON_Delete(response);
	if(!payload)
		return -1;
	syslog(LOG_INFO, "ACTION RESPONSE: %s", payload);
	snprintf(topic, sizeof(topic), "tylink/%s/thing/action/execute_response",
			request->device_id ? request->device_id : context->config.device_id);
	uint16_t msgid = mqtt_client_publish(context->mqtt_client, topic,
			(const uint8_t *)payload, strlen(payload), MQTT_QOS_1);
	free(payload);
	return msgid ? 0 : -1;
}

static void process_action(tuya_mqtt_context_t * context,  void * user_data, const tuyalink_message_t * request);

void on_connected(tuya_mqtt_context_t *context, void *user_data)
{
	(void)context;
	/* `user_data` is expected to point to `esp_ctx` */
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

static void process_action(tuya_mqtt_context_t *context,
                           void *user_data,
                           const tuyalink_message_t *request)
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
        syslog(LOG_WARNING,
               "process_action: payload is not an object");
        cJSON_Delete(root);
        return;
    }
	
	action = cJSON_GetObjectItemCaseSensitive(root, "actionCode");
	params = cJSON_GetObjectItemCaseSensitive(root, "inputParams");

    if (!cJSON_IsString(action) || !action->valuestring) {
        syslog(LOG_WARNING,
               "process_action: missing actionCode");
        cJSON_Delete(root);
        return;
    }

    if (params && !cJSON_IsObject(params)) {
        syslog(LOG_WARNING,
               "process_action: inputParams is not an object");
        cJSON_Delete(root);
        return;
    }

    syslog(LOG_INFO,
           "process_action: actionCode=%s",
           action->valuestring);

    if (strcmp(action->valuestring, "switch_pin") == 0) {
        ret = handle_switch_pin(ectxp, params, &result);
    } else if (strcmp(action->valuestring, "get_measurements") == 0) {
        ret = handle_get_measurements(ectxp, params, &result);
    } else if (strcmp(action->valuestring, "get_devices") == 0) {
        ret = handle_get_devices(ectxp, params, &result);
    } else {
        syslog(LOG_WARNING,
               "process_action: unknown action '%s'",
               action->valuestring);
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

    if (send_action_response(context, request, code, error, result) != 0) {
        syslog(LOG_ERR, "process_action: failed to send response");
    }

    cJSON_Delete(root);
}