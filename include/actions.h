#ifndef ACTIONS_H
#define ACTIONS_H

#include "tuyalink_core.h"

int save_action_to_file(const char *str);
void on_connected(tuya_mqtt_context_t *context, void *user_data);
void on_disconnect(tuya_mqtt_context_t *context, void *user_data);
void on_messages(tuya_mqtt_context_t *context, void *user_data, const tuyalink_message_t *msg);
void send_system_info(tuya_mqtt_context_t *client);

#endif