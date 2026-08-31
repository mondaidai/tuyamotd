#include "action_handlers.h"
#include "esp_actions.h"
#include <errno.h>
#include <string.h>

const char * action_error_to_string(action_error_t error)
{
	switch (error) {
	case ACTION_OK:
		return "ok";
	case ACTION_ERR_INVALID_CONTEXT:
		return "invalid context";
	case ACTION_ERR_INVALID_ARGUMENT:
		return "invalid argument";
	case ACTION_ERR_NO_MEMORY:
		return "out of memory";
	case ACTION_ERR_ESP_DATA_FAILED:
		return "ESP data collection failed";
	case ACTION_ERR_ESP_PIN_FAILED:
		return "ESP pin operation failed";
	case ACTION_ERR_ESP_DEVICE_FAILED:
		return "ESP device query failed";
	case ACTION_ERR_UNSUPPORTED_STATE:
		return "unsupported switch state";
	case ACTION_ERR_MISSING_STATE:
		return "missing switch state";
	case ACTION_ERR_TUYA_REPORT_FAILED:
		return "Tuya property report failed";
	case ACTION_ERR_INVALID_ACTION_CODE:
		return "invalid action code";
	case ACTION_ERR_UNKNOWN_ACTION:
		return "unknown action";
	case ACTION_ERR_JSON_PARSE_FAILED:
		return "JSON parse failed";
	case ACTION_ERR_INTERNAL:
		return "internal action error";
	default:
		return "unknown error";
	}
}

static void set_action_error(esp_response_t *response, const char *message)
{
	if(!response)
		return;

	response->rc = -999;
	free(response->msg);
	response->msg = strdup(message);
}

static void ensure_action_message(esp_response_t *response)
{
	if(!response || response->msg)
		return;

	response->msg = strdup(response->rc == 0 ? "Success" : "Action failed");
}

action_error_t handle_get_measurements(esp_ctx * ectxp, cJSON * params, cJSON **result) {
	esp_request_t req = {0};
	esp_response_t resp = {0};
	esp_status_t st = ESP_OK;
	cJSON * out = NULL;
	action_error_t ret = ACTION_OK;

	if(!ectxp)
		return ACTION_ERR_INVALID_CONTEXT;

	if(params) {
		cJSON * p = cJSON_GetObjectItemCaseSensitive(params, "port");
		if(cJSON_IsString(p) && p->valuestring)
			req.portname = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "model");
		if(cJSON_IsString(p) && p->valuestring)
			req.model = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "sensor");
		if(cJSON_IsString(p) && p->valuestring)
			req.sensor = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "pin");
		if(cJSON_IsNumber(p))
			req.pin = p->valueint;
	}

	st = esp_get_data(ectxp, &req, &resp);
	if(st != ESP_OK) {
		if(st == ESP_ERR_UBUS_INVOKE && resp.ubus_status == UBUS_STATUS_NOT_FOUND) {
			esp_reinit(ectxp);
			st = esp_get_data(ectxp, &req, &resp);
			if(st == ESP_OK)
				goto next;
		}
		ret = ACTION_ERR_ESP_DATA_FAILED;
		set_action_error(&resp, "Failed to collect esp data.");
	}
next:
	ensure_action_message(&resp);

	out = cJSON_CreateObject();
	if(!out) {
		ret = ACTION_ERR_NO_MEMORY;
		goto cleanup;
	}

	cJSON_AddNumberToObject(out, "rc", resp.rc);
	cJSON_AddStringToObject(out, "msg", resp.msg ? resp.msg : "Action failed");
	if(resp.data) {
		cJSON *data = cJSON_Parse(resp.data);
		if(data) {
			char *data_json = cJSON_PrintUnformatted(data);
			if(data_json) {
				cJSON_AddStringToObject(out, "data", data_json);
				free(data_json);
			} else {
				cJSON_AddStringToObject(out, "data", resp.data);
			}
			cJSON_Delete(data);
		} else {
			cJSON_AddStringToObject(out, "data", resp.data);
		}
	}

cleanup:
	if(result) {
		*result = out;
		out = NULL;
	} else if(out) {
		cJSON_Delete(out);
	}
	esp_free_request(&req);
	esp_free_response(&resp);
	return ret;
}

action_error_t handle_switch_pin(esp_ctx * ectxp, cJSON * params, cJSON **result)
{
	esp_request_t req = {0};
	esp_response_t resp = {0};
	cJSON * out = NULL;
	action_error_t ret = ACTION_OK;
	esp_status_t st;

	if(!ectxp)
		return ACTION_ERR_INVALID_CONTEXT;

	if(params) {
		cJSON * p = cJSON_GetObjectItemCaseSensitive(params, "port");
		if(cJSON_IsString(p) && p->valuestring)
			req.portname = strdup(p->valuestring);
		p = cJSON_GetObjectItemCaseSensitive(params, "pin");
		if(cJSON_IsNumber(p))
			req.pin = p->valueint;
		p = cJSON_GetObjectItemCaseSensitive(params, "state");
		if(cJSON_IsString(p) && p->valuestring) {
			if(strcmp(p->valuestring, "on") == 0) {
				st = esp_turn_on_pin(ectxp, &req, &resp);
				if(st != ESP_OK) {
					if(st == ESP_ERR_UBUS_INVOKE && resp.ubus_status == UBUS_STATUS_NOT_FOUND) {
						esp_reinit(ectxp);
						st = esp_turn_on_pin(ectxp, &req, &resp);
						if(st == ESP_OK)
							goto next1;
					}
					ret = ACTION_ERR_ESP_PIN_FAILED;
					set_action_error(&resp, "Failed switch pin.");
				}
next1:
			} else if(strcmp(p->valuestring, "off") == 0) {
				st = esp_turn_off_pin(ectxp, &req, &resp);
				if(st != ESP_OK) {
					if(st == ESP_ERR_UBUS_INVOKE && resp.ubus_status == UBUS_STATUS_NOT_FOUND) {
						esp_reinit(ectxp);
						st = esp_turn_off_pin(ectxp, &req, &resp);
						if(st == ESP_OK)
							goto next2;
					}
					ret = ACTION_ERR_ESP_PIN_FAILED;
					set_action_error(&resp, "Failed switch pin.");
				}
next2:
			} else {
				ret = ACTION_ERR_UNSUPPORTED_STATE;
				set_action_error(&resp, "Unsupported switch state.");
			}
		} else {
			ret = ACTION_ERR_MISSING_STATE;
			set_action_error(&resp, "Missing switch state.");
		}
	}

	ensure_action_message(&resp);
	out = cJSON_CreateObject();
	if(!out) {
		ret = ACTION_ERR_NO_MEMORY;
		goto cleanup;
	}

	cJSON_AddNumberToObject(out, "rc", resp.rc);
	cJSON_AddStringToObject(out, "msg", resp.msg ? resp.msg : "Action failed");

cleanup:
	if(result) {
		*result = out;
		out = NULL;
	} else if(out) {
		cJSON_Delete(out);
	}
	esp_free_request(&req);
	esp_free_response(&resp);
	return ret;
}
action_error_t handle_get_devices(esp_ctx * ectxp, cJSON *params, cJSON **result) {
	cJSON *out = NULL;
	cJSON *arr = NULL;
	action_error_t ret = ACTION_OK;
	esp_device_list_t dl = {0};
	esp_status_t st;

	if(!ectxp)
		return ACTION_ERR_INVALID_CONTEXT;

	st = esp_get_devices(ectxp, &dl);
	if(st != ESP_OK) {
		if(st == ESP_ERR_UBUS_INVOKE && dl.ubus_status == UBUS_STATUS_NOT_FOUND) {
			esp_reinit(ectxp);
			st = esp_get_devices(ectxp, &dl);
			if(st == ESP_OK)
				goto next;
		}
		ret = ACTION_ERR_ESP_DEVICE_FAILED;
	}
next:

	out = cJSON_CreateObject();
	if(!out) {
		ret = ACTION_ERR_NO_MEMORY;
		esp_free_device_list(&dl);
		return ret;
	}

	arr = cJSON_CreateArray();
	if(arr && dl.size && dl.edl) {
		for (size_t i = 0; i < dl.size; i++) {
			cJSON *it = cJSON_CreateObject();
			if(it) {
				if(dl.edl[i].portname) {
					cJSON_AddStringToObject(it, "port", dl.edl[i].portname);
				}
				cJSON_AddNumberToObject(it, "vendor_id", dl.edl[i].vid);
				cJSON_AddNumberToObject(it, "product_id", dl.edl[i].pid);
				cJSON_AddItemToArray(arr, it);
			}
		}
	}

	if(arr) {
		char *devices_json = cJSON_PrintUnformatted(arr);
		if(devices_json) {
			cJSON_AddStringToObject(out, "devices", devices_json);
			free(devices_json);
		} else {
			cJSON_AddStringToObject(out, "devices", "[]");
			ret = ACTION_ERR_NO_MEMORY;
		}
		cJSON_Delete(arr);
	}

	if(result) {
		*result = out;
		out = NULL;
	} else if(out) {
		cJSON_Delete(out);
	}
	esp_free_device_list(&dl);
	return ret;
}