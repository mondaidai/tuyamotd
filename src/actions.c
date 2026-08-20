#include "actions.h"
#include "esp_actions.h"
#include "config.h"

#include <cjson/cJSON.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>

int handle_get_measurements(tuya_mqtt_context_t * context, cJSON * params) {
	esp_request_t req = {0};
	esp_response_t resp = {0};

	if (params) {
		cJSON *p = cJSON_GetObjectItemCaseSensitive(params, "port");
		if (cJSON_IsString(p) && p->valuestring)
			req.portname = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "model");
		if (cJSON_IsString(p) && p->valuestring)
			req.model = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "sensor");
		if (cJSON_IsString(p) && p->valuestring)
			req.sensor = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "pin");
		if (cJSON_IsNumber(p))
			req.pin = p->valueint;
	}

	esp_status_t st = esp_get_data(NULL, &req, &resp);

	cJSON *out = cJSON_CreateObject();
	cJSON_AddNumberToObject(out, "rc", resp.rc);
	if (resp.msg)
		cJSON_AddStringToObject(out, "msg", resp.msg);
	if (resp.data) {
		cJSON *data = cJSON_Parse(resp.data);
		if (data) {
			cJSON_AddItemToObject(out, "data", data);
		} else {
			cJSON_AddStringToObject(out, "data", resp.data);
		}
	}

	char *s = cJSON_PrintUnformatted(out);
	if (s) {
		tuyalink_thing_property_report(context, NULL, s);
		free(s);
	}

	cJSON_Delete(out);
	esp_free_request(&req);
	esp_free_response(&resp);
	(void)st;
	return 0;
}
int handle_switch_pin(tuya_mqtt_context_t * context, cJSON * params) {

	esp_request_t req = {0};
	esp_response_t resp = {0};

	if (params) {
		cJSON *p = cJSON_GetObjectItemCaseSensitive(params, "port");
		if (cJSON_IsString(p) && p->valuestring)
			req.portname = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "model");
		if (cJSON_IsString(p) && p->valuestring)
			req.model = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "sensor");
		if (cJSON_IsString(p) && p->valuestring)
			req.sensor = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "pin");
		if (cJSON_IsNumber(p))
			req.pin = p->valueint;
		p = cJSON_GetObjectItemCaseSensitive(params, "state");
		if (cJSON_IsString(p) && p->valuestring) {
			if (strcmp(p->valuestring, "on") == 0) {
				esp_turn_on_pin(NULL, &req, &resp);
			} else {
				esp_turn_off_pin(NULL, &req, &resp);
			}
		} else {
			/* default: toggle on */
			esp_turn_on_pin(NULL, &req, &resp);
		}
	}

	cJSON *out = cJSON_CreateObject();
	cJSON_AddNumberToObject(out, "rc", resp.rc);
	if (resp.msg)
		cJSON_AddStringToObject(out, "msg", resp.msg);
	char *s = cJSON_PrintUnformatted(out);
	if (s) {
		tuyalink_thing_property_report(context, NULL, s);
		free(s);
	}
	cJSON_Delete(out);
	esp_free_request(&req);
	esp_free_response(&resp);
	return 0;
}
int handle_get_devices(tuya_mqtt_context_t * context, cJSON * params) {

	esp_device_list_t dl = {0};
	esp_status_t st = esp_get_devices(NULL, &dl);

	cJSON *out = cJSON_CreateObject();
	cJSON_AddNumberToObject(out, "rc", st == ESP_OK ? 0 : 1);
	if (dl.size && dl.edl) {
		cJSON *arr = cJSON_CreateArray();
		for (size_t i = 0; i < dl.size; i++) {
			cJSON *it = cJSON_CreateObject();
			if (dl.edl[i].portname)
				cJSON_AddStringToObject(it, "port", dl.edl[i].portname);
			cJSON_AddNumberToObject(it, "vendor_id", dl.edl[i].vid);
			cJSON_AddNumberToObject(it, "product_id", dl.edl[i].pid);
			cJSON_AddItemToArray(arr, it);
		}
		cJSON_AddItemToObject(out, "devices", arr);
	}

	char *s = cJSON_PrintUnformatted(out);
	if (s) {
		tuyalink_thing_property_report(context, NULL, s);
		free(s);
	}
	cJSON_Delete(out);
	esp_free_device_list(&dl);
	(void)st;
	return 0;
}

static void process_action(tuya_mqtt_context_t * context, const char * payload)
{
	if (!payload)
		return;

	cJSON *root = cJSON_Parse(payload);
	if(!root)
		return;

	cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "actionCode");
	cJSON *params = cJSON_GetObjectItemCaseSensitive(root, "inputParams");

	if(cJSON_IsString(action) && action->valuestring) {
		int ret = EINVAL;

		if(strcmp(action->valuestring, "switch_pin") == 0) {
			ret = handle_switch_pin(context, params);
		} else if(strcmp(action->valuestring, "get_measurements") == 0){
			ret = handle_get_measurements(context, params);
		} else if(strcmp(action->valuestring, "get_devices") == 0) {
			ret = handle_get_devices(context, params);
		} else {
			syslog(LOG_WARNING, "Unknown action: %s", action->valuestring);
		}
	}

	cJSON_Delete(root);
	return;
}

void on_connected(tuya_mqtt_context_t *context, void *user_data)
{
	syslog(LOG_INFO, "Connected to Tuya Cloud");
	return;
}
void on_disconnect(tuya_mqtt_context_t *context, void *user_data)
{
	syslog(LOG_WARNING, "Disconnected from Tuya Cloud");
	return;
}
void on_messages(tuya_mqtt_context_t * context, void * user_data, const tuyalink_message_t * msg)
{
	if (!msg)
		return;

	switch(msg->type) {
	case THING_TYPE_ACTION_EXECUTE:
		if (msg->data_string)
			process_action(context, msg->data_string);
		break;
	default:
		break;
	}
	return;
}
