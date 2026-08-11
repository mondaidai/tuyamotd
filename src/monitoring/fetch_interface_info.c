#include "monitoring.h"
#include <libubox/blobmsg_json.h>
#include <libubus.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>

struct interface_stats {
	char name[32];
	uint64_t rx_bytes;
	uint64_t tx_bytes;
};

struct interface_callback_data {
	network_interface_t * interfaces;
	size_t count;
	size_t capacity;

	struct interface_stats *stats;
	size_t stats_count;
	size_t stats_capacity;

	monitoring_status_t status_code;
};

enum {
	DEV_STATISTICS,
	__DEV_MAX,
};

enum {
	STAT_RX_BYTES,
	STAT_TX_BYTES,
	__STAT_MAX,
};

enum {
	IFACE_DEVICE,
	IFACE_INTERFACE,
	IFACE_IPV4,
	IFACE_IPV6,
	__IFACE_MAX,
};

enum {
	ADDR_ADDRESS,
	ADDR_MASK,
	__ADDR_MAX,
};

static int add_device_stats(
	struct interface_callback_data *data,
	const char *name,
	uint64_t rx_bytes,
	uint64_t tx_bytes)
{
	if(!data || !name) {
		return -1;
	}

	if(data->stats_count >= data->stats_capacity) {
		size_t new_capacity = data->stats_capacity ? data->stats_capacity * 2 : 16;
		struct interface_stats *tmp = realloc(data->stats, new_capacity * sizeof(*tmp));
		if(!tmp) {
			return -1;
		}
		data->stats = tmp;
		data->stats_capacity = new_capacity;
	}

	struct interface_stats *entry = &data->stats[data->stats_count++];
	memset(entry, 0, sizeof(*entry));
	strncpy(entry->name, name, sizeof(entry->name) - 1);
	entry->name[sizeof(entry->name) - 1] = '\0';
	entry->rx_bytes = rx_bytes;
	entry->tx_bytes = tx_bytes;
	return 0;
}

static const struct interface_stats *find_device_stats(
	const struct interface_callback_data *data,
	const char *name)
{
	if(!data || !name) {
		return NULL;
	}

	for (size_t i = 0; i < data->stats_count; ++i) {
		if(strcmp(data->stats[i].name, name) == 0) {
			return &data->stats[i];
		}
	}

	return NULL;
}

static int add_interface_entry(
	struct interface_callback_data *data,
	const char *name,
	const char *ip,
	uint8_t netmask,
	uint8_t family,
	uint64_t rx_bytes,
	uint64_t tx_bytes)
{
	if(!data || !name) {
		return -1;
	}

	if(data->count >= data->capacity) {
		size_t new_capacity = data->capacity ? data->capacity * 2 : 16;
		network_interface_t *tmp = realloc(data->interfaces, new_capacity * sizeof(*tmp));
		if(!tmp) {
			return -1;
		}
		data->interfaces = tmp;
		data->capacity = new_capacity;
	}

	network_interface_t *entry = &data->interfaces[data->count++];
	memset(entry, 0, sizeof(*entry));
	strncpy(entry->name, name, sizeof(entry->name) - 1);
	entry->name[sizeof(entry->name) - 1] = '\0';

	if(ip) {
		strncpy(entry->ip, ip, sizeof(entry->ip) - 1);
		entry->ip[sizeof(entry->ip) - 1] = '\0';
	}

	entry->netmask = netmask;
	entry->family = family;

	entry->rx_bytes = rx_bytes;
	entry->tx_bytes = tx_bytes;
	return 0;
}

static int process_address_array(
	struct blob_attr *array,
	struct interface_callback_data *data,
	const char *device_name,
	uint8_t family,
	uint64_t rx_bytes,
	uint64_t tx_bytes,
	bool *added_address)
{
	if(!array || !data || !device_name || !added_address) {
		return -1;
	}

	static const struct blobmsg_policy addr_policy[] = {
		{ .name = "address", .type = BLOBMSG_TYPE_STRING },
		{ .name = "mask", .type = BLOBMSG_TYPE_INT32 },
	};

	struct blob_attr *addr_tb[__ADDR_MAX];
	struct blob_attr *attr;
	int rem;

	blobmsg_for_each_attr(attr, array, rem) {
		if(blobmsg_type(attr) != BLOBMSG_TYPE_TABLE) {
			continue;
		}

		memset(addr_tb, 0, sizeof(addr_tb));
		blobmsg_parse(addr_policy, __ADDR_MAX, addr_tb,
			blobmsg_data(attr), blobmsg_data_len(attr));

		const char *ip = addr_tb[ADDR_ADDRESS] ? blobmsg_get_string(addr_tb[ADDR_ADDRESS]) : NULL;
		uint32_t mask = addr_tb[ADDR_MASK] ? blobmsg_get_u32(addr_tb[ADDR_MASK]) : 0;

		if(ip && *ip) {
			if(add_interface_entry(data, device_name, ip, (uint8_t)mask, family, rx_bytes, tx_bytes) != 0) {
				return -1;
			}
			*added_address = true;
		}
	}

	return 0;
}

static void network_device_status_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	(void)type;

	struct interface_callback_data *data = req->priv;
	struct blob_attr *cur;
	int rem;

	if(!data) {
		return;
	}

	if (!msg) {
		data->status_code = MONITORING_ERR_PARSE;
		return;
	}

	static const struct blobmsg_policy device_policy[] = {
		{ .name = "statistics", .type = BLOBMSG_TYPE_TABLE },
	};
	static const struct blobmsg_policy statistics_policy[] = {
		{ .name = "rx_bytes", .type = BLOBMSG_TYPE_INT64 },
		{ .name = "tx_bytes", .type = BLOBMSG_TYPE_INT64 },
	};

	struct blob_attr *device_tb[__DEV_MAX];
	struct blob_attr *stats_tb[__STAT_MAX];

	blobmsg_for_each_attr(cur, msg, rem) {
		const char *iface_name = blobmsg_name(cur);
		if(!iface_name || blobmsg_type(cur) != BLOBMSG_TYPE_TABLE) {
			continue;
		}

		memset(device_tb, 0, sizeof(device_tb));
		blobmsg_parse(device_policy, __DEV_MAX, device_tb,
			blobmsg_data(cur), blobmsg_data_len(cur));

		uint64_t rx_bytes = 0;
		uint64_t tx_bytes = 0;

		if(device_tb[DEV_STATISTICS]) {
			memset(stats_tb, 0, sizeof(stats_tb));
			blobmsg_parse(statistics_policy, __STAT_MAX, stats_tb,
				blobmsg_data(device_tb[DEV_STATISTICS]), blobmsg_data_len(device_tb[DEV_STATISTICS]));

			if(stats_tb[STAT_RX_BYTES]) {
				rx_bytes = blobmsg_get_u64(stats_tb[STAT_RX_BYTES]);
			}
			if(stats_tb[STAT_TX_BYTES]) {
				tx_bytes = blobmsg_get_u64(stats_tb[STAT_TX_BYTES]);
			}
		}

		if (add_device_stats(data, iface_name, rx_bytes, tx_bytes) != 0) {
			data->status_code = MONITORING_ERR_NOMEM;
			return;
		}
	}

	data->status_code = MONITORING_OK;
}

static void network_interface_dump_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	(void)type;

	struct interface_callback_data *data = req->priv;
	struct blob_attr *cur;
	int rem;

	if (!data) {
		return;
	}

	if (!msg) {
		data->status_code = MONITORING_ERR_PARSE;
		return;
	}

	enum {
		DUMP_INTERFACE,
		__DUMP_MAX,
	};

	static const struct blobmsg_policy dump_policy[] = {
		{ .name = "interface", .type = BLOBMSG_TYPE_ARRAY },
	};
	static const struct blobmsg_policy iface_policy[] = {
		{ .name = "device", .type = BLOBMSG_TYPE_STRING },
		{ .name = "interface", .type = BLOBMSG_TYPE_STRING },
		{ .name = "ipv4-address", .type = BLOBMSG_TYPE_ARRAY },
		{ .name = "ipv6-address", .type = BLOBMSG_TYPE_ARRAY },
	};

	struct blob_attr *tb[__DUMP_MAX];
	struct blob_attr *iface_tb[__IFACE_MAX];

	blobmsg_parse(dump_policy, __DUMP_MAX, tb,
		blobmsg_data(msg), blobmsg_data_len(msg));

	if(!tb[DUMP_INTERFACE]) {
		data->status_code = MONITORING_ERR_PARSE;
		return;
	}

	blobmsg_for_each_attr(cur, tb[DUMP_INTERFACE], rem) {
		if(blobmsg_type(cur) != BLOBMSG_TYPE_TABLE) {
			continue;
		}

		memset(iface_tb, 0, sizeof(iface_tb));
		blobmsg_parse(iface_policy, __IFACE_MAX, iface_tb,
			blobmsg_data(cur), blobmsg_data_len(cur));

		const char * device_name = iface_tb[IFACE_DEVICE] ? blobmsg_get_string(iface_tb[IFACE_DEVICE]) : NULL;
		if(!device_name) {
			device_name = iface_tb[IFACE_INTERFACE] ? blobmsg_get_string(iface_tb[IFACE_INTERFACE]) : NULL;
		}
		if(!device_name || strcmp(device_name, "lo") == 0) {
			continue;
		}

		const struct interface_stats *stats = find_device_stats(data, device_name);
		uint64_t rx_bytes = stats ? stats->rx_bytes : 0;
		uint64_t tx_bytes = stats ? stats->tx_bytes : 0;

		bool added_address = false;

		if(iface_tb[IFACE_IPV4] && process_address_array(iface_tb[IFACE_IPV4], data, device_name, FAMILY_IPV4, rx_bytes, tx_bytes, &added_address) != 0) {
			data->status_code = MONITORING_ERR_NOMEM;
			return;
		}

		if(iface_tb[IFACE_IPV6] && process_address_array(iface_tb[IFACE_IPV6], data, device_name, FAMILY_IPV6, rx_bytes, tx_bytes, &added_address) != 0) {
			data->status_code = MONITORING_ERR_NOMEM;
			return;
		}

		if(!added_address) {
			if(add_interface_entry(data, device_name, "", 0, FAMILY_IPV4, rx_bytes, tx_bytes) != 0) {
				data->status_code = MONITORING_ERR_NOMEM;
				return;
			}
		}
	}

	data->status_code = MONITORING_OK;
}

monitoring_status_t fetch_interface_info(monitoring_ctx * mctx, network_interface_t ** interfaces, uint16_t * count)
{
	if(!mctx || !interfaces || !count) {
		return MONITORING_ERR_INVALID_ARG;
	}

	*interfaces = NULL;
	*count = 0;

	struct interface_callback_data data = {0};
	monitoring_status_t status = MONITORING_ERR_INTERFACE;

	(void)mctx;
	int invoke_ret = ubus_invoke(mctx->uctx, mctx->network_device_id, "status", NULL,
		network_device_status_cb, &data, UBUS_TIMEOUT);
	if (invoke_ret != 0) {
		status = MONITORING_ERR_UBUS_INVOKE;
		goto cleanup;
	}

	if (data.status_code != MONITORING_OK) {
		status = data.status_code;
		goto cleanup;
	}

	data.status_code = MONITORING_ERR_INTERFACE;

	// invoking network.interface dump
	invoke_ret = ubus_invoke(mctx->uctx, mctx->network_interface_id, "dump", NULL,
		network_interface_dump_cb, &data, UBUS_TIMEOUT);
	if (invoke_ret != 0) {
		status = MONITORING_ERR_UBUS_INVOKE;
		goto cleanup;
	}

	if (data.status_code != MONITORING_OK) {
		status = data.status_code;
		goto cleanup;
	}

	*interfaces = data.interfaces;
	*count = data.count;
	status = MONITORING_OK;

cleanup:
    free(data.stats);
    if(status != MONITORING_OK)
        free(data.interfaces);

    return status;
}
