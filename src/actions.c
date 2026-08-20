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
	esp_status_t st;
	cJSON *out;
	char *s;

	if (!context) {
		syslog(LOG_ERR, "handle_get_measurements: null Tuya context");
		return -EINVAL;
	}

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

	st = esp_get_data(NULL, &req, &resp);
	out = cJSON_CreateObject();
	if (st != ESP_OK) {
		syslog(LOG_ERR, "handle_get_measurements: esp_get_data failed: %d", st);
		cJSON_AddStringToObject(out, "msg", "esp_get_data failed");
	}

	cJSON_AddNumberToObject(out, "rc", resp.rc);
	if (resp.msg)
		cJSON_AddStringToObject(out, "msg", resp.msg);
	if (resp.data) {
		cJSON *data = cJSON_Parse(resp.data);
		if (data) {
			char *data_json = cJSON_PrintUnformatted(data);
			if (data_json) {
				cJSON_AddStringToObject(out, "data", data_json);
				free(data_json);
			} else {
				cJSON_AddStringToObject(out, "data", resp.data);
				syslog(LOG_WARNING, "handle_get_measurements: cJSON_PrintUnformatted failed for data payload");
			}
			cJSON_Delete(data);
		} else {
			cJSON_AddStringToObject(out, "data", resp.data);
			syslog(LOG_WARNING, "handle_get_measurements: invalid JSON in resp.data: %s", resp.data);
		}
	} else {
		syslog(LOG_WARNING, "handle_get_measurements: missing resp.data");
	}

	s = cJSON_PrintUnformatted(out);
	if (s) {
		int report_rc = tuyalink_thing_property_report(context, NULL, s);
		syslog(LOG_INFO, "handle_get_measurements: property report rc=%d payload=%s", report_rc, s);
		free(s);
	} else {
		syslog(LOG_ERR, "handle_get_measurements: cJSON_PrintUnformatted failed for response");
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
	cJSON *out;
	char *s;
	bool action_taken = false;

	if (!context) {
		syslog(LOG_ERR, "handle_switch_pin: null Tuya context");
		return -EINVAL;
	}

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
				action_taken = true;
				if (esp_turn_on_pin(NULL, &req, &resp) != ESP_OK) {
					syslog(LOG_ERR, "handle_switch_pin: esp_turn_on_pin failed");
				}
			} else {
				action_taken = true;
				if (esp_turn_off_pin(NULL, &req, &resp) != ESP_OK) {
					syslog(LOG_ERR, "handle_switch_pin: esp_turn_off_pin failed");
				}
			}
		} else {
			/* default: toggle on */
			action_taken = true;
			if (esp_turn_on_pin(NULL, &req, &resp) != ESP_OK) {
				syslog(LOG_ERR, "handle_switch_pin: esp_turn_on_pin default failed");
			}
		}
	}

	if (!action_taken) {
		syslog(LOG_WARNING, "handle_switch_pin: no valid state or pin received");
	}

	out = cJSON_CreateObject();
	cJSON_AddNumberToObject(out, "rc", resp.rc);
	if (resp.msg)
		cJSON_AddStringToObject(out, "msg", resp.msg);
	s = cJSON_PrintUnformatted(out);
	if (s) {
		int report_rc = tuyalink_thing_property_report(context, NULL, s);
		syslog(LOG_INFO, "handle_switch_pin: property report rc=%d payload=%s", report_rc, s);
		free(s);
	} else {
		syslog(LOG_ERR, "handle_switch_pin: cJSON_PrintUnformatted failed for response");
	}
	cJSON_Delete(out);
	esp_free_request(&req);
	esp_free_response(&resp);
	return 0;
}
int handle_get_devices(tuya_mqtt_context_t * context, cJSON * params) {

	esp_device_list_t dl = {0};
	esp_status_t st;
	cJSON *out;
	char *s;

	if (!context) {
		syslog(LOG_ERR, "handle_get_devices: null Tuya context");
		return -EINVAL;
	}

	st = esp_get_devices(NULL, &dl);
	if (st != ESP_OK) {
		syslog(LOG_ERR, "handle_get_devices: esp_get_devices failed: %d", st);
	}

	out = cJSON_CreateObject();
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

		char *devices_json = cJSON_PrintUnformatted(arr);
		if (devices_json) {
			cJSON_AddStringToObject(out, "devices", devices_json);
			free(devices_json);
		} else {
			syslog(LOG_WARNING, "handle_get_devices: cJSON_PrintUnformatted failed for device array");
			cJSON_AddStringToObject(out, "devices", "[]");
		}
		cJSON_Delete(arr);
	} else {
		syslog(LOG_WARNING, "handle_get_devices: no devices returned by esp_get_devices");
		cJSON_AddStringToObject(out, "devices", "[]");
	}

	s = cJSON_PrintUnformatted(out);
	if (s) {
		int report_rc = tuyalink_thing_property_report(context, NULL, s);
		syslog(LOG_INFO, "handle_get_devices: property report rc=%d payload=%s", report_rc, s);
		free(s);
	} else {
		syslog(LOG_ERR, "handle_get_devices: cJSON_PrintUnformatted failed for response");
	}
	cJSON_Delete(out);
	esp_free_device_list(&dl);
	(void)st;
	return 0;
}

static void process_action(tuya_mqtt_context_t * context, const char * payload)
{
	if (!payload) {
		syslog(LOG_WARNING, "process_action: null payload");
		return;
	}

	cJSON *root = cJSON_Parse(payload);
	if(!root) {
		syslog(LOG_WARNING, "process_action: invalid JSON payload: %s", payload);
		return;
	}

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

		if (ret != 0) {
			syslog(LOG_ERR, "process_action: action %s failed with rc=%d", action->valuestring, ret);
		}
	} else {
		syslog(LOG_WARNING, "process_action: missing or invalid actionCode in payload");
	}

	cJSON_Delete(root);
	return;
}

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
	if (!msg) {
		syslog(LOG_WARNING, "on_messages: null message received");
		return;
	}

	switch(msg->type) {
	case THING_TYPE_ACTION_EXECUTE:
		if (msg->data_string)
			process_action(context, msg->data_string);
		else
			syslog(LOG_WARNING, "on_messages: THING_TYPE_ACTION_EXECUTE with no data_string");
		break;
	default:
		syslog(LOG_DEBUG, "on_messages: ignoring message type %d", msg->type);
		break;
	}
	return;
}
