#include "esp_actions.h"
#include <libubox/blobmsg_json.h>
#include <libubus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    esp_status_t esp_status;
    void * response;
} esp_response_wrapper_t;

int esp_init(esp_ctx * ectx, struct ubus_context * uctx)
{
    if(!mctx || !uctx)
		return ESP_ERR_INVALID_ARG;

	uint32_t ubved_id;
	if(ubus_lookup_id(uctx, "ubved", &system_id))
		return ESP_ERR_UBUS_LOOKUP;

	mctx->uctx = uctx;
	mctx->ubved_id = ubved_id;
	return ESP_OK;
}

void esp_destroy(esp_ctx * ectx)
{
	if(!ectx) {
		return;
	}
	memset(ectx, 0, sizeof(esp_ctx));
	return;
}

void esp_free_response(esp_response_t * response)
{
    if(!response)
        return;
    free(response->msg);
	free(response->data);
    return;
}

void esp_free_request(esp_request_t * request)
{
    if(!request)
        return;
    free(request->model);
    free(request->sensor);
    free(request->portname);
    return;
}
void esp_free_device_list(esp_device_list_t * edl)
{
    if(!edl)
        return;
    for(size_t i = 0; i < edl->size)
        free(edl[i].portname);
    memset(edl, 0, sizeof(esp_device_list_t));
    return;
}

static void esp_turn_on_off_pin_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	(void)type;

	esp_response_wrapper_t *wrapper = req->priv;
	if (!wrapper) {
		return;
	}

	esp_response_t *response = wrapper->response;
	if (!response) {
		wrapper->esp_status = ESP_ERR_INVALID_ARG;
		return;
	}

	if (!msg) {
		wrapper->esp_status = ESP_ERR_PARSE;
		return;
	}

	static const struct blobmsg_policy policy[] = {
		{ .name = "rc", .type = BLOBMSG_TYPE_INT32 },
		{ .name = "msg", .type = BLOBMSG_TYPE_STRING },
	};

	struct blob_attr *tb[2];
	memset(tb, 0, sizeof(tb));
	blobmsg_parse(policy, 2, tb, blobmsg_data(msg), blobmsg_data_len(msg));

	if (tb[0])
		response->rc = (int)blobmsg_get_u32(tb[0]);
	else
		response->rc = -1;

	response->msg = NULL;
	if (response->data)
		free(response->data);
	response->data = NULL;
	if (tb[1]) {
		const char *s = blobmsg_get_string(tb[1]);
		if (s) {
			response->msg = strdup(s);
			if (!response->msg) {
				wrapper->esp_status = ESP_ERR_NOMEM;
				return;
			}
		}
	}

	wrapper->esp_status = ESP_OK;
}

static void esp_get_data_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	(void)type;

	esp_response_wrapper_t *wrapper = req->priv;
	if (!wrapper)
		return;

	esp_response_t *response = wrapper->response;
	if (!response) {
		wrapper->esp_status = ESP_ERR_INVALID_ARG;
		return;
	}

	if (!msg) {
		wrapper->esp_status = ESP_ERR_PARSE;
		return;
	}

	static const struct blobmsg_policy policy[] = {
		{ .name = "rc", .type = BLOBMSG_TYPE_INT32 },
		{ .name = "msg", .type = BLOBMSG_TYPE_STRING },
		{ .name = "data", .type = BLOBMSG_TYPE_TABLE },
	};

	struct blob_attr *tb[3];
	memset(tb, 0, sizeof(tb));
	blobmsg_parse(policy, 3, tb, blobmsg_data(msg), blobmsg_data_len(msg));

	if (tb[0])
		response->rc = (int)blobmsg_get_u32(tb[0]);
	else
		response->rc = -1;

	if (response->msg)
		free(response->msg);
	response->msg = NULL;
	if (tb[1]) {
		const char *s = blobmsg_get_string(tb[1]);
		if (s) {
			response->msg = strdup(s);
			if (!response->msg) {
				wrapper->esp_status = ESP_ERR_NOMEM;
				return;
			}
		}
	}

	/* clear any previous data string */
	if (response->data)
		free(response->data);
	response->data = NULL;

	if (tb[2]) {
		static const struct blobmsg_policy data_policy[] = {
			{ .name = "humidity", .type = BLOBMSG_TYPE_INT32 },
			{ .name = "temperature", .type = BLOBMSG_TYPE_DOUBLE },
		};
		struct blob_attr *dtb[2];
		memset(dtb, 0, sizeof(dtb));
		blobmsg_parse(data_policy, 2, dtb, blobmsg_data(tb[2]), blobmsg_data_len(tb[2]));

		int humidity = 0;
		double temperature = 0.0;
		int have_h = 0, have_t = 0;
		if (dtb[0]) {
			humidity = (int)blobmsg_get_u32(dtb[0]);
			have_h = 1;
		}
		if (dtb[1]) {
			temperature = blobmsg_get_double(dtb[1]);
			have_t = 1;
		}

		if (have_h || have_t) {
			/* serialize as simple JSON */
			if (have_h && have_t) {
				int len = snprintf(NULL, 0, "{\"humidity\":%d,\"temperature\":%.6f}", humidity, temperature);
				response->data = malloc(len + 1);
				if (!response->data) {
					wrapper->esp_status = ESP_ERR_NOMEM;
					return;
				}
				snprintf(response->data, len + 1, "{\"humidity\":%d,\"temperature\":%.6f}", humidity, temperature);
			} else if (have_h) {
				int len = snprintf(NULL, 0, "{\"humidity\":%d}", humidity);
				response->data = malloc(len + 1);
				if (!response->data) {
					wrapper->esp_status = ESP_ERR_NOMEM;
					return;
				}
				snprintf(response->data, len + 1, "{\"humidity\":%d}", humidity);
			} else {
				int len = snprintf(NULL, 0, "{\"temperature\":%.6f}", temperature);
				response->data = malloc(len + 1);
				if (!response->data) {
					wrapper->esp_status = ESP_ERR_NOMEM;
					return;
				}
				snprintf(response->data, len + 1, "{\"temperature\":%.6f}", temperature);
			}
		}
	}

	wrapper->esp_status = ESP_OK;
}
static void esp_get_devices_cb(struct ubus_request * req, int type, struct blob_attr * msg)
{
	(void)type;
	esp_response_wrapper_t *data = req->priv;

	if (!data) {
		return;
	}

	esp_device_list_t *response = data->response;
	if (!response) {
		data->esp_status = ESP_ERR_INVALID_ARG;
		return;
	}

	if (!msg) {
		data->esp_status = ESP_ERR_PARSE;
		return;
	}

	struct blob_attr *devs_attr;
	int rem;
	size_t count = 0;

	/* look for top-level "devices" array */
	blobmsg_for_each_attr(devs_attr, msg, rem) {
		const char *name = blobmsg_name(devs_attr);
		if (!name) continue;
		if (strcmp(name, "devices") != 0) continue;
		if (blobmsg_type(devs_attr) != BLOBMSG_TYPE_ARRAY) continue;

		/* first pass: count entries */
		struct blob_attr *entry;
		int rem2;
		blobmsg_for_each_attr(entry, blobmsg_data(devs_attr), rem2) {
			if (blobmsg_type(entry) == BLOBMSG_TYPE_TABLE)
				count++;
		}

		response->size = count;
		if (count == 0) {
			response->edl = NULL;
			data->esp_status = ESP_OK;
			return;
		}

		response->edl = calloc(count, sizeof(esp_device_t));
		if (!response->edl) {
			response->size = 0;
			data->esp_status = ESP_ERR_NOMEM;
			return;
		}

		/* second pass: parse each device */
		size_t idx = 0;
		blobmsg_for_each_attr(entry, blobmsg_data(devs_attr), rem2) {
			if (blobmsg_type(entry) != BLOBMSG_TYPE_TABLE)
				continue;

			static const struct blobmsg_policy info_policy[] = {
				{ .name = "port", .type = BLOBMSG_TYPE_STRING },
				{ .name = "vendor_id", .type = BLOBMSG_TYPE_STRING },
				{ .name = "product_id", .type = BLOBMSG_TYPE_STRING },
			};
			struct blob_attr *tb[3];
			memset(tb, 0, sizeof(tb));
			blobmsg_parse(info_policy, ARRAY_SIZE(tb), tb, blobmsg_data(entry), blobmsg_data_len(entry));

			const char *port = tb[0] ? blobmsg_get_string(tb[0]) : NULL;
			const char *vids = tb[1] ? blobmsg_get_string(tb[1]) : NULL;
			const char *pids = tb[2] ? blobmsg_get_string(tb[2]) : NULL;

			if (port) {
				response->edl[idx].portname = strdup(port);
				if (!response->edl[idx].portname) {
					/* cleanup on OOM */
					for (size_t j = 0; j < idx; j++)
						free(response->edl[j].portname);
					free(response->edl);
					response->edl = NULL;
					response->size = 0;
					data->esp_status = ESP_ERR_NOMEM;
					return;
				}
			} else {
				response->edl[idx].portname = NULL;
			}

			response->edl[idx].vid = 0;
			response->edl[idx].pid = 0;
			if (vids) {
				long v = strtol(vids, NULL, 0);
				response->edl[idx].vid = (int)v;
			}
			if (pids) {
				long p = strtol(pids, NULL, 0);
				response->edl[idx].pid = (int)p;
			}

			idx++;
		}

		data->esp_status = ESP_OK;
		return;
	}

	/* if we reach here, no devices key found */
	response->edl = NULL;
	response->size = 0;
	data->esp_status = ESP_OK;
    return;
}

esp_status_t esp_turn_on_pin(esp_ctx * ectx, esp_request_t * params, esp_response_t * response)
{
    if(!params->portname || params->pin < 0)
        return ESP_ERR_INVALID_ARG;

    esp_status_t result = ESP_OK;
    int ubus_ret;
    
    struct blob_buf b = {0};

    blob_buf_init(&b, 0);

    blobmsg_add_string(&b, "port", params->portname);
    blobmsg_add_u32(&b, "pin", params->pin);
    esp_response_wrapper_t erw;
    erw.response = response;
    erw.esp_status = ESP_OK;
    if((ubus_ret = ubus_invoke(mctx->uctx, mctx->ubved_id, "on", b.head, esp_turn_on_off_pin_cb, &erw, ectx->timeout)) != UBUS_STATUS_OK) {
        response->ubus_status = ubus_ret;
        result = ESP_ERR_UBUS_INVOKE;
	}
    blob_buf_free(&b);
    return result;
}

esp_status_t esp_turn_off_pin(esp_ctx * ectx, esp_request_t * params, esp_response_t * response)
{
    if(!params->portname || params->pin < 0)
        return ESP_ERR_INVALID_ARG;

    esp_status_t result = ESP_OK;
    int ubus_ret;
    
    struct blob_buf b = {0};

    blob_buf_init(&b, 0);

    blobmsg_add_string(&b, "port", params->portname);
    blobmsg_add_u32(&b, "pin", params->pin);
    
    esp_response_wrapper_t erw;
    erw.response = response;
    erw.esp_status = ESP_OK;
    if((ubus_ret = ubus_invoke(mctx->uctx, mctx->ubved_id, "off", b.head, esp_turn_on_off_pin_cb, &erw, ectx->timeout)) != UBUS_STATUS_OK) {
        response->ubus_status = ubus_ret;
        result = ESP_ERR_UBUS_INVOKE;
	}
    blob_buf_free(&b);
    return result;
}

esp_status_t esp_turn_on_pin(esp_ctx * ectx, esp_request_t * params, esp_response_t * response)
{
    if(!params->model || !params->sensor || !params->portname || params->pin < 0 )
        return ESP_ERR_INVALID_ARG;

    esp_status_t result = ESP_OK;
    int ubus_ret;
    
    struct blob_buf b = {0};

    blob_buf_init(&b, 0);

    blobmsg_add_string(&b, "port", params->portname);
    blobmsg_add_string(&b, "model", params->model);
    blobmsg_add_string(&b, "sensor", params->sensor);
    blobmsg_add_u32(&b, "pin", params->pin);

    esp_response_wrapper_t erw;
    erw.response = response;
    erw.esp_status = ESP_OK;
    if((ubus_ret = ubus_invoke(mctx->uctx, mctx->ubved_id, "get", b.head, esp_get_data_cb, &erw, ectx->timeout)) != UBUS_STATUS_OK) {
        response->ubus_status = ubus_ret;
        result = ESP_ERR_UBUS_INVOKE;
	}
    blob_buf_free(&b);
    return result;
}
esp_status_t esp_get_devices(esp_ctx * ectx, esp_device_list_t * response)
{
    esp_status_t result = ESP_OK;
    int ubus_ret;
    esp_response_wrapper_t erw;
    erw.response = response;
    erw.esp_status = ESP_OK;
    if((ubus_ret = ubus_invoke(mctx->uctx, mctx->ubved_id, "get", NULL, esp_get_devices_cb, &erw, ectx->timeout)) != UBUS_STATUS_OK) {
        response->ubus_status = ubus_ret;
        result = ESP_ERR_UBUS_INVOKE;
	}
    blob_buf_free(&b);
    result = erw->esp_status;
    return result;
}
