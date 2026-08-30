#include "actions.h"
#include "json_writer.h"
#include "config.h"

#include <cjson/cJSON.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>

static int save_action_to_file(const char * str)
{
	if(str == NULL || str[0] == '\0') {
		return EINVAL;
	}

	FILE *fp = fopen(g_app_config.action_log_file, "a");
	if(!fp) {
		return errno;
	}

	if(fprintf(fp, "%s\n", str) < 0) {
		fclose(fp);
		return EIO;
	}

	fclose(fp);
	return 0;
}

static int handle_save_log(cJSON * params)
{
	cJSON *msg = cJSON_GetObjectItemCaseSensitive(params, "message");
	if(!cJSON_IsString(msg) || !msg->valuestring)
		return EINVAL;

	return save_action_to_file(msg->valuestring);
}

static void process_action(const char * payload)
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

		if(strcmp(action->valuestring, "save_log") == 0) {
			
			ret = handle_save_log(params);
			
			if(ret == 0) {
				syslog(LOG_DEBUG, "Action received: payload saved to %s", g_app_config.action_log_file);
			} else if (ret == EINVAL) {
				syslog(LOG_WARNING, "Action %s invalid payload", action->valuestring);
			} else {
				syslog(LOG_ERR, "Action %s failed: %s", action->valuestring, strerror(ret));
			}

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
void on_messages(tuya_mqtt_context_t *context, void * user_data, const tuyalink_message_t * msg)
{
	if (!msg)
		return;

	switch(msg->type) {
	case THING_TYPE_ACTION_EXECUTE:
		if (msg->data_string)
			process_action(msg->data_string);
		break;
	default:
		break;
	}
	return;
}

typedef struct {
	char ip[INET6_ADDRSTRLEN];
	uint8_t netmask;
} grouped_address_t;

typedef struct {
	char name[32];
	uint64_t rx_bytes;
	uint64_t tx_bytes;
	size_t ipv4_count;
	size_t ipv6_count;
	grouped_address_t *ipv4;
	grouped_address_t *ipv6;
} grouped_interface_t;

static int init_grouped_interface(grouped_interface_t *group, size_t max_addresses)
{
	if (!group)
		return EINVAL;

	group->ipv4 = calloc(max_addresses, sizeof(*group->ipv4));
	if (!group->ipv4)
		return ENOMEM;

	group->ipv6 = calloc(max_addresses, sizeof(*group->ipv6));
	if (!group->ipv6) {
		free(group->ipv4);
		group->ipv4 = NULL;
		return ENOMEM;
	}

	group->ipv4_count = 0;
	group->ipv6_count = 0;
	return 0;
}

static void free_grouped_interface(grouped_interface_t *group)
{
	if (!group)
		return;

	free(group->ipv4);
	free(group->ipv6);
	group->ipv4 = NULL;
	group->ipv6 = NULL;
	group->ipv4_count = 0;
	group->ipv6_count = 0;
}

static int find_grouped_interface(const grouped_interface_t *groups, size_t count, const char *name)
{
	for (size_t i = 0; i < count; ++i) {
		if (strcmp(groups[i].name, name) == 0) {
			return (int)i;
		}
	}

	return -1;
}

static size_t collect_grouped_interfaces(const system_info_t *info, grouped_interface_t *groups, size_t max_groups)
{
	size_t count = 0;

	if(!info || !groups) {
		syslog(LOG_ERR, "collect_grouped_interfaces: invalid parameters");
		return 0;
	}

	if(info->num_interfaces == 0 || !info->interfaces) {
		syslog(LOG_DEBUG, "collect_grouped_interfaces: no interfaces to collect");
		return 0;
	}

	for (size_t i = 0; i < info->num_interfaces && count < max_groups; ++i) {
		const network_interface_t *src = &info->interfaces[i];
		int group_index = find_grouped_interface(groups, count, src->name);

		if (group_index < 0) {
			if (count >= max_groups) {
				break;
			}

			memset(&groups[count], 0, sizeof(groups[count]));
			if (init_grouped_interface(&groups[count], max_groups) != 0) {
				syslog(LOG_ERR, "collect_grouped_interfaces: failed to allocate group arrays");
				for (size_t j = 0; j < count; ++j) {
					free_grouped_interface(&groups[j]);
				}
				return 0;
			}

			snprintf(groups[count].name, sizeof(groups[count].name), "%s", src->name);
			groups[count].rx_bytes = src->rx_bytes;
			groups[count].tx_bytes = src->tx_bytes;
			group_index = (int)count;
			count++;
		}

		if (src->ip && src->ip[0] != '\0') {
			if (src->family == FAMILY_IPV6) {
				continue; //temporarily disable ipv6 addresses to prevent tuya cloud complaints
				if (groups[group_index].ipv6_count < max_groups) {
					snprintf(groups[group_index].ipv6[groups[group_index].ipv6_count].ip,
					 sizeof(groups[group_index].ipv6[groups[group_index].ipv6_count].ip),
					 "%s",
					 src->ip);
					groups[group_index].ipv6[groups[group_index].ipv6_count].netmask = src->netmask;
					groups[group_index].ipv6_count++;
				}
			} else {
				if (groups[group_index].ipv4_count < max_groups) {
					snprintf(groups[group_index].ipv4[groups[group_index].ipv4_count].ip,
					 sizeof(groups[group_index].ipv4[groups[group_index].ipv4_count].ip),
					 "%s",
					 src->ip);
					groups[group_index].ipv4[groups[group_index].ipv4_count].netmask = src->netmask;
					groups[group_index].ipv4_count++;
				}
			}
		}
	}

	return count;
}

static int build_interface_json_string(char *buffer, size_t size, const grouped_interface_t *iface)
{
	json_writer_t iface_writer;

	json_init(&iface_writer, buffer, size);

	if (json_object_begin(&iface_writer) != 0 ||
		json_string(&iface_writer, "name", iface->name) != 0 ||
		json_u64(&iface_writer, "rx_bytes", iface->rx_bytes) != 0 ||
		json_u64(&iface_writer, "tx_bytes", iface->tx_bytes) != 0 ||
		json_array_begin(&iface_writer, "ipv4") != 0) {
		return -1;
	}

	for (size_t i = 0; i < iface->ipv4_count; ++i) {
		if (json_object_begin(&iface_writer) != 0 ||
			json_string(&iface_writer, "address", iface->ipv4[i].ip) != 0 ||
			json_u64(&iface_writer, "mask", iface->ipv4[i].netmask) != 0 ||
			json_object_end(&iface_writer) != 0) {
			return -1;
		}
	}

	if (json_array_end(&iface_writer) != 0 ||
		json_array_begin(&iface_writer, "ipv6") != 0) {
		return -1;
	}

	for (size_t i = 0; i < iface->ipv6_count; ++i) {
		if (json_object_begin(&iface_writer) != 0 ||
			json_string(&iface_writer, "address", iface->ipv6[i].ip) != 0 ||
			json_u64(&iface_writer, "mask", iface->ipv6[i].netmask) != 0 ||
			json_object_end(&iface_writer) != 0) {
			return -1;
		}
	}

	if (json_array_end(&iface_writer) != 0 ||
		json_object_end(&iface_writer) != 0) {
		return -1;
	}

	return 0;
}

int send_system_info(tuya_mqtt_context_t * client, system_info_t * info)
{
	char json[4096];
	json_writer_t stats_writer;
	int rc = EIO;

	if(!client || !info) {
		syslog(LOG_ERR, "send_system_info: invalid parameters");
		return EINVAL;
	}

	int max_interfaces = info->num_interfaces;

	json_init(&stats_writer, json, sizeof(json));

	if(json_object_begin(&stats_writer) != 0) {
		syslog(LOG_ERR, "Failed to start JSON object");
		goto cleanup;
	}

	if(json_u64(&stats_writer, "total_ram", info->total_ram) != 0) {
		syslog(LOG_ERR, "Failed to build total_ram property JSON");
		goto cleanup;
	}

	if(json_u64(&stats_writer, "free_ram", info->free_ram) != 0) {
		syslog(LOG_ERR, "Failed to build free_ram property JSON");
		goto cleanup;
	}

	if(json_u64(&stats_writer, "system_uptime", info->system_uptime) != 0) {
		syslog(LOG_ERR, "Failed to build system_uptime property JSON");
		goto cleanup;
	}

	if(json_u64(&stats_writer, "cpu_usage", info->cpu_usage) != 0) {
		syslog(LOG_ERR, "Failed to build cpu_usage property JSON");
		goto cleanup;
	}

	size_t max_groups = info->num_interfaces;
	grouped_interface_t *grouped_interfaces = NULL;
	size_t group_count = 0;

	if (max_groups > 0) {
		grouped_interfaces = calloc(max_groups, sizeof(*grouped_interfaces));
		if (!grouped_interfaces) {
			syslog(LOG_ERR, "Failed to allocate grouped interface buffer");
			goto cleanup;
		}

		group_count = collect_grouped_interfaces(info, grouped_interfaces, max_groups);
	}

	if(json_array_begin(&stats_writer, "interfaces") != 0) {
		syslog(LOG_ERR, "Failed to create interfaces array");
		goto cleanup;
	}

	for(size_t i = 0; i < group_count; i++) {
		char iface_json[2048];

		if(build_interface_json_string(iface_json, sizeof(iface_json), &grouped_interfaces[i]) != 0) {
			syslog(LOG_WARNING, "Failed to serialize interface %s", grouped_interfaces[i].name);
			continue;
		}

		if(json_string_element(&stats_writer, iface_json) != 0) {
			syslog(LOG_ERR, "Failed to append interface JSON string");
			goto cleanup;
		}
	}

	if(json_array_end(&stats_writer) != 0) {
		syslog(LOG_ERR, "Failed to finalize interfaces array JSON");
		goto cleanup;
	}

	if(json_object_end(&stats_writer) != 0) {
		syslog(LOG_ERR, "Failed to finalize JSON payload");
		goto cleanup;
	}

	tuyalink_thing_property_report(client, NULL, json);
	rc = 0;

cleanup:
	if (grouped_interfaces) {
		for (size_t i = 0; i < max_groups; ++i) {
			free_grouped_interface(&grouped_interfaces[i]);
		}
		free(grouped_interfaces);
	}

	return rc;
}