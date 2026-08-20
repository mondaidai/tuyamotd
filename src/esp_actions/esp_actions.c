#include "esp_actions.h"
#include <libubox/blobmsg_json.h>
#include <libubus.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    esp_status_t esp_status;
    void *response;
} esp_response_wrapper_t;

int esp_init(esp_ctx * ectx, struct ubus_context * uctx)
{
    uint32_t ubved_id = 0;

    if (!ectx || !uctx)
        return ESP_ERR_INVALID_ARG;

    if (ubus_lookup_id(uctx, "ubved", &ubved_id))
        return ESP_ERR_UBUS_LOOKUP;

    ectx->uctx = uctx;
    ectx->ubved_id = ubved_id;
    ectx->ubus_timeout_ms = 5000;
    return ESP_OK;
}

void esp_destroy(esp_ctx * ectx)
{
    if (!ectx)
        return;

    memset(ectx, 0, sizeof(*ectx));
}

void esp_free_response(esp_response_t * response)
{
    if (!response)
        return;

    free(response->msg);
    free(response->data);
    memset(response, 0, sizeof(*response));
}

void esp_free_request(esp_request_t * request)
{
    if (!request)
        return;

    free(request->model);
    free(request->sensor);
    free(request->portname);
    memset(request, 0, sizeof(*request));
}

void esp_free_device_list(esp_device_list_t * edl)
{
    size_t i;

    if (!edl)
        return;

    if (edl->edl) {
        for (i = 0; i < edl->size; ++i)
            free(edl->edl[i].portname);
        free(edl->edl);
    }

    memset(edl, 0, sizeof(*edl));
}

static void esp_turn_on_off_pin_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    esp_response_wrapper_t *wrapper;
    esp_response_t *response;
    static const struct blobmsg_policy policy[] = {
        { .name = "rc", .type = BLOBMSG_TYPE_INT32 },
        { .name = "msg", .type = BLOBMSG_TYPE_STRING },
    };
    struct blob_attr *tb[2];

    (void)type;

    if (!req || !req->priv)
        return;

    wrapper = req->priv;
    response = wrapper->response;
    if (!response) {
        wrapper->esp_status = ESP_ERR_INVALID_ARG;
        return;
    }

    if (!msg) {
        wrapper->esp_status = ESP_ERR_PARSE;
        return;
    }

    memset(tb, 0, sizeof(tb));
    blobmsg_parse(policy, 2, tb, blobmsg_data(msg), blobmsg_data_len(msg));

    response->rc = tb[0] ? (int)blobmsg_get_u32(tb[0]) : -1;
    if (response->msg) {
        free(response->msg);
        response->msg = NULL;
    }
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
    esp_response_wrapper_t *wrapper;
    esp_response_t *response;
    static const struct blobmsg_policy policy[] = {
        { .name = "rc", .type = BLOBMSG_TYPE_INT32 },
        { .name = "msg", .type = BLOBMSG_TYPE_STRING },
        { .name = "data", .type = BLOBMSG_TYPE_TABLE },
    };
    struct blob_attr *tb[3];

    (void)type;

    if (!req || !req->priv)
        return;

    wrapper = req->priv;
    response = wrapper->response;
    if (!response) {
        wrapper->esp_status = ESP_ERR_INVALID_ARG;
        return;
    }

    if (!msg) {
        wrapper->esp_status = ESP_ERR_PARSE;
        return;
    }

    memset(tb, 0, sizeof(tb));
    blobmsg_parse(policy, 3, tb, blobmsg_data(msg), blobmsg_data_len(msg));

    response->rc = tb[0] ? (int)blobmsg_get_u32(tb[0]) : -1;
    if (response->msg) {
        free(response->msg);
        response->msg = NULL;
    }
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

    if (response->data) {
        free(response->data);
        response->data = NULL;
    }

    if (tb[2]) {
        static const struct blobmsg_policy data_policy[] = {
            { .name = "humidity", .type = BLOBMSG_TYPE_INT32 },
            { .name = "temperature", .type = BLOBMSG_TYPE_DOUBLE },
        };
        struct blob_attr *dtb[2];
        int humidity = 0;
        double temperature = 0.0;
        int have_h = 0;
        int have_t = 0;

        memset(dtb, 0, sizeof(dtb));
        blobmsg_parse(data_policy, 2, dtb, blobmsg_data(tb[2]), blobmsg_data_len(tb[2]));

        if (dtb[0]) {
            humidity = (int)blobmsg_get_u32(dtb[0]);
            have_h = 1;
        }
        if (dtb[1]) {
            temperature = blobmsg_get_double(dtb[1]);
            have_t = 1;
        }

        if (have_h || have_t) {
            int len;
            if (have_h && have_t) {
                len = snprintf(NULL, 0, "{\"humidity\":%d,\"temperature\":%.6f}", humidity, temperature);
                response->data = malloc((size_t)len + 1);
                if (!response->data) {
                    wrapper->esp_status = ESP_ERR_NOMEM;
                    return;
                }
                snprintf(response->data, (size_t)len + 1, "{\"humidity\":%d,\"temperature\":%.6f}", humidity, temperature);
            } else if (have_h) {
                len = snprintf(NULL, 0, "{\"humidity\":%d}", humidity);
                response->data = malloc((size_t)len + 1);
                if (!response->data) {
                    wrapper->esp_status = ESP_ERR_NOMEM;
                    return;
                }
                snprintf(response->data, (size_t)len + 1, "{\"humidity\":%d}", humidity);
            } else {
                len = snprintf(NULL, 0, "{\"temperature\":%.6f}", temperature);
                response->data = malloc((size_t)len + 1);
                if (!response->data) {
                    wrapper->esp_status = ESP_ERR_NOMEM;
                    return;
                }
                snprintf(response->data, (size_t)len + 1, "{\"temperature\":%.6f}", temperature);
            }
        }
    }

    wrapper->esp_status = ESP_OK;
}

static void esp_get_devices_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    esp_response_wrapper_t *data;
    esp_device_list_t *response;
    struct blob_attr *devs_attr;
    int rem;
    size_t count = 0;

    (void)type;

    if (!req || !req->priv)
        return;

    data = req->priv;
    response = data->response;
    if (!response) {
        data->esp_status = ESP_ERR_INVALID_ARG;
        return;
    }

    if (!msg) {
        data->esp_status = ESP_ERR_PARSE;
        return;
    }

    blobmsg_for_each_attr(devs_attr, msg, rem) {
        const char *name = blobmsg_name(devs_attr);
        struct blob_attr *entry;
        int rem2;
        size_t idx;

        if (!name || strcmp(name, "devices") != 0)
            continue;
        if (blobmsg_type(devs_attr) != BLOBMSG_TYPE_ARRAY)
            continue;

        count = 0;
        blobmsg_for_each_attr(entry, blobmsg_data(devs_attr), rem2) {
            if (blobmsg_type(entry) == BLOBMSG_TYPE_TABLE)
                ++count;
        }

        response->size = count;
        response->edl = NULL;
        if (count == 0) {
            data->esp_status = ESP_OK;
            return;
        }

        response->edl = calloc(count, sizeof(*response->edl));
        if (!response->edl) {
            response->size = 0;
            data->esp_status = ESP_ERR_NOMEM;
            return;
        }

        idx = 0;
        blobmsg_for_each_attr(entry, blobmsg_data(devs_attr), rem2) {
            static const struct blobmsg_policy info_policy[] = {
                { .name = "port", .type = BLOBMSG_TYPE_STRING },
                { .name = "vendor_id", .type = BLOBMSG_TYPE_STRING },
                { .name = "product_id", .type = BLOBMSG_TYPE_STRING },
            };
            struct blob_attr *tb[3];
            const char *port = NULL;
            const char *vids = NULL;
            const char *pids = NULL;

            if (blobmsg_type(entry) != BLOBMSG_TYPE_TABLE)
                continue;

            memset(tb, 0, sizeof(tb));
            blobmsg_parse(info_policy, ARRAY_SIZE(tb), tb, blobmsg_data(entry), blobmsg_data_len(entry));

            port = tb[0] ? blobmsg_get_string(tb[0]) : NULL;
            vids = tb[1] ? blobmsg_get_string(tb[1]) : NULL;
            pids = tb[2] ? blobmsg_get_string(tb[2]) : NULL;

            response->edl[idx].portname = port ? strdup(port) : NULL;
            response->edl[idx].vid = vids ? (int)strtol(vids, NULL, 0) : 0;
            response->edl[idx].pid = pids ? (int)strtol(pids, NULL, 0) : 0;

            if (!response->edl[idx].portname && port) {
                esp_free_device_list(response);
                data->esp_status = ESP_ERR_NOMEM;
                return;
            }

            idx++;
        }

        data->esp_status = ESP_OK;
        return;
    }

    response->edl = NULL;
    response->size = 0;
    data->esp_status = ESP_OK;
}

esp_status_t esp_turn_on_pin(esp_ctx * ectx, esp_request_t * params, esp_response_t * response)
{
    esp_status_t result = ESP_OK;
    int ubus_ret;
    struct blob_buf b = {0};
    esp_response_wrapper_t erw;

    if (!ectx || !params || !response)
        return ESP_ERR_INVALID_ARG;
    if (!params->portname || params->pin < 0)
        return ESP_ERR_INVALID_ARG;

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "port", params->portname);
    blobmsg_add_u32(&b, "pin", (uint32_t)params->pin);

    erw.response = response;
    erw.esp_status = ESP_OK;

    if ((ubus_ret = ubus_invoke(ectx->uctx, ectx->ubved_id, "on", b.head, esp_turn_on_off_pin_cb, &erw, ectx->ubus_timeout_ms)) != UBUS_STATUS_OK) {
        response->ubus_status = ubus_ret;
        result = ESP_ERR_UBUS_INVOKE;
    }

    blob_buf_free(&b);
    return result;
}

esp_status_t esp_turn_off_pin(esp_ctx * ectx, esp_request_t * params, esp_response_t * response)
{
    esp_status_t result = ESP_OK;
    int ubus_ret;
    struct blob_buf b = {0};
    esp_response_wrapper_t erw;

    if (!ectx || !params || !response)
        return ESP_ERR_INVALID_ARG;
    if (!params->portname || params->pin < 0)
        return ESP_ERR_INVALID_ARG;

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "port", params->portname);
    blobmsg_add_u32(&b, "pin", (uint32_t)params->pin);

    erw.response = response;
    erw.esp_status = ESP_OK;

    if ((ubus_ret = ubus_invoke(ectx->uctx, ectx->ubved_id, "off", b.head, esp_turn_on_off_pin_cb, &erw, ectx->ubus_timeout_ms)) != UBUS_STATUS_OK) {
        response->ubus_status = ubus_ret;
        result = ESP_ERR_UBUS_INVOKE;
    }

    blob_buf_free(&b);
    return result;
}

esp_status_t esp_get_data(esp_ctx * ectx, esp_request_t * params, esp_response_t * response)
{
    esp_status_t result = ESP_OK;
    int ubus_ret;
    struct blob_buf b = {0};
    esp_response_wrapper_t erw;

    if (!ectx || !params || !response)
        return ESP_ERR_INVALID_ARG;

    blob_buf_init(&b, 0);

    if (params->portname)
        blobmsg_add_string(&b, "port", params->portname);
    if (params->model)
        blobmsg_add_string(&b, "model", params->model);
    if (params->sensor)
        blobmsg_add_string(&b, "sensor", params->sensor);
    if (params->pin >= 0)
        blobmsg_add_u32(&b, "pin", (uint32_t)params->pin);

    erw.response = response;
    erw.esp_status = ESP_OK;

    if ((ubus_ret = ubus_invoke(ectx->uctx, ectx->ubved_id, "get", b.head, esp_get_data_cb, &erw, ectx->ubus_timeout_ms)) != UBUS_STATUS_OK) {
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

    if (!ectx || !response)
        return ESP_ERR_INVALID_ARG;

    memset(response, 0, sizeof(*response));
    erw.response = response;
    erw.esp_status = ESP_OK;

    if ((ubus_ret = ubus_invoke(ectx->uctx, ectx->ubved_id, "get", NULL, esp_get_devices_cb, &erw, ectx->ubus_timeout_ms)) != UBUS_STATUS_OK) {
        result = ESP_ERR_UBUS_INVOKE;
    } else {
        result = erw.esp_status;
    }

    return result;
}
