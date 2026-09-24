#include "actions.h"
#include "monitoring.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#define ACTION_LOG_FILE "/tmp/tuya_action.log"

int save_action_to_file(const char *str)
{
	if(str == NULL || str[0] == '\0') {
		return EINVAL;
	}
	FILE *fp = fopen(ACTION_LOG_FILE, "a");
	if (!fp) {
		return errno;
	}

	if(fprintf(fp, "%s\n", str) < 0) {
		fclose(fp);
		return EIO;
	}
	
	fclose(fp);
	return 0;
}

void on_connected(tuya_mqtt_context_t * context, void *user_data)
{
	syslog(LOG_INFO, "Connected to Tuya Cloud");
	return;
}
void on_disconnect(tuya_mqtt_context_t * context, void *user_data)
{
	syslog(LOG_WARNING, "Disconnected from Tuya Cloud");
	return;
}
void on_messages(tuya_mqtt_context_t * context, void * user_data, const tuyalink_message_t * msg)
{
	int ret;
	switch (msg->type) {
	case THING_TYPE_ACTION_EXECUTE:
		ret = save_action_to_file(msg->data_string);	 
		if(ret == EINVAL)
			syslog(LOG_WARNING, "Received empty action payload");
		else if(ret != 0)
			syslog(LOG_ERR, "Failed to save action to %s: %s", ACTION_LOG_FILE, strerror(ret));
		else
			syslog(LOG_DEBUG, "Action received: payload saved to %s", ACTION_LOG_FILE);
		break;
	default:
		syslog(LOG_DEBUG, "Unhandled message type %d", msg->type);
		break;
	}
	return;
}
void send_system_info(tuya_mqtt_context_t * client)
{
	char json[1300];
	system_info_t info;

	int ret = monitor_collect(&info);
	if(ret != 0)
	{
		syslog(LOG_ERR, "Failed to collect system information");
		return;
	}

	snprintf(json, sizeof(json),
		"{"
		"\"total_ram\":{\"value\":%lu},"
		"\"free_ram\":{\"value\":%lu},"
		"\"system_uptime\":{\"value\":%lu},"
		"\"cpu_usage\":{\"value\":%u},"
		"\"interfaces\":{\"value\":\"%s\"}"
		"}",
		info.total_ram,
		info.free_ram,
		info.system_uptime,
		info.cpu_usage,
		info.interfaces);

	tuyalink_thing_property_report(client, NULL, json);
	return;	
}