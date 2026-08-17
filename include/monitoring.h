#ifndef MONITORING_H
#define MONITORING_H

#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <libubus.h>

#define UBUS_TIMEOUT 3000

typedef enum {
	MONITORING_OK = 0,

	MONITORING_ERR_INVALID_ARG = 1 << 1,
	MONITORING_ERR_UBUS_CONNECT = 1 << 2,
	MONITORING_ERR_UBUS_LOOKUP = 1 << 3,
	MONITORING_ERR_UBUS_INVOKE = 1 << 4,
	MONITORING_ERR_PARSE = 1 << 5,
	MONITORING_ERR_CPU_STATS = 1 << 6,
	MONITORING_ERR_OPEN_FILE = 1 << 7,
	MONITORING_ERR_INTERFACE = 1 << 8,

	MONITORING_ERR_MEMORY_OBJECT_MISSING = 1 << 9,
	MONITORING_ERR_UPTIME_MISSING = 1 << 10,
	MONITORING_ERR_MEMORY_FIELD_MISSING = 1 << 11,
	MONITORING_ERR_NOMEM = 1 << 12,
	MONITORING_ERR_CRITICAL =
		MONITORING_ERR_UBUS_CONNECT |
		MONITORING_ERR_UBUS_LOOKUP |
		MONITORING_ERR_UBUS_INVOKE |
		MONITORING_ERR_PARSE |
		MONITORING_ERR_NOMEM,
} monitoring_status_t;


enum address_family {
	FAMILY_IPV4 = 4,
	FAMILY_IPV6 = 6,
};

typedef struct {
	char name[32];
	char ip[INET6_ADDRSTRLEN];
	uint8_t family;
	uint8_t netmask;
	uint64_t rx_bytes;
	uint64_t tx_bytes;
} network_interface_t;

typedef struct
{
	uint64_t total_ram;
	uint64_t free_ram;
	uint64_t system_uptime;
	uint8_t cpu_usage;
	uint16_t num_interfaces;
	network_interface_t * interfaces;
} system_info_t;

typedef struct {
	uint64_t user;
	uint64_t nice;
	uint64_t system;
	uint64_t idle;
	uint64_t iowait;
	uint64_t irq;
	uint64_t softirq;
	uint64_t steal;
} CPUStats;

typedef struct {
	struct ubus_context * uctx;
	uint32_t system_id;
	uint32_t network_device_id;
	uint32_t network_interface_id;
} monitoring_ctx;

struct call_system_info {
	uint64_t total_ram;
	uint64_t free_ram;
	uint64_t uptime;
	int status_code;
};

int monitoring_init(monitoring_ctx * mctx, struct ubus_context *uctx);
void monitoring_destroy(monitoring_ctx * mctx);

int gather_system_info(monitoring_ctx * mctx, system_info_t * info);
void free_system_info(system_info_t *info);
// call once before get_cpu_usage to initialize the previous CPU stats
void cpu_usage_init(void);
// returns average CPU usage (0-100) since the last call
uint8_t get_cpu_usage(void);

const char * monitoring_strerr(monitoring_status_t status);

monitoring_status_t fetch_system_info(monitoring_ctx * mctx, struct call_system_info * sdata);
monitoring_status_t fetch_interface_info(monitoring_ctx * mctx, network_interface_t ** interfaces, uint16_t * count);

#endif
