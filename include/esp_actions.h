#ifndef ESP_ACTIONS
#define ESP_ACTIONS
#include <libubus.h>

typedef enum {
	ESP_OK = 0,

	ESP_ERR_INVALID_ARG = 1 << 1,
	ESP_ERR_UBUS_CONNECT = 1 << 2,
	ESP_ERR_UBUS_LOOKUP = 1 << 3,
	ESP_ERR_UBUS_INVOKE = 1 << 4,
	ESP_ERR_PARSE = 1 << 5,
	ESP_ERR_CPU_STATS = 1 << 6,
	ESP_ERR_OPEN_FILE = 1 << 7,
	ESP_ERR_INTERFACE = 1 << 8,

	ESP_ERR_MEMORY_OBJECT_MISSING = 1 << 9,
	ESP_ERR_UPTIME_MISSING = 1 << 10,
	ESP_ERR_MEMORY_FIELD_MISSING = 1 << 11,
	ESP_ERR_NOMEM = 1 << 12,
	ESP_ERR_CRITICAL =
		ESP_ERR_UBUS_CONNECT |
		ESP_ERR_UBUS_LOOKUP |
		ESP_ERR_UBUS_INVOKE |
		ESP_ERR_PARSE |
		ESP_ERR_NOMEM,
} esp_status_t;


typedef struct {
	struct ubus_context * uctx;
	uint32_t ubved_id;
    uint32_t ubus_timeout_ms;
} esp_ctx;

typedef struct {
    int pin;
    char * model;
    char * sensor;
    char * portname;
} esp_request_t;

typedef struct {
    char * portname;
    int vid;
    int pid;
} esp_device_t;

typedef struct {
    esp_device_t * edl;
    size_t size;
} esp_device_list_t;

typedef struct {
    esp_status_t esp_status;
    int ubus_status; // check it if esp_status_t is no ESP_OK
    int rc;
    char * msg;
	char * data;
} esp_response_t;

int esp_init(esp_ctx * ectx, struct ubus_context * uctx);
void esp_destroy(esp_ctx * ectx);

esp_status_t esp_turn_on_pin(esp_ctx * ectx, esp_request_t * params, esp_response_t * response);
esp_status_t esp_turn_off_pin(esp_ctx * ectx, esp_request_t * params, esp_response_t * response);
esp_status_t esp_get_data(esp_ctx * ectx, esp_request_t * params, esp_response_t * response);
esp_status_t esp_get_devices(esp_ctx * ectx, esp_device_list_t * response);

void esp_free_response(esp_response_t * response);
void esp_free_request(esp_request_t * request);
void esp_free_device_list(esp_device_list_t * edl);

#endif