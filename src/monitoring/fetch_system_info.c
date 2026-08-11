#include "monitoring.h"
#include <libubox/blobmsg_json.h>
#include <libubus.h>

enum {
	TOTAL_MEMORY,
	FREE_MEMORY,
	__MEMORY_MAX,
};
enum {
	UPTIME_DATA,
	MEMORY_DATA,
	__INFO_MAX,
};

static void system_info_cb(struct ubus_request * req, int type, struct blob_attr * msg)
{
	static const struct blobmsg_policy memory_policy[__MEMORY_MAX] = {
		[TOTAL_MEMORY]	  = { .name = "total", .type = BLOBMSG_TYPE_INT64 },
		[FREE_MEMORY]	  = { .name = "free", .type = BLOBMSG_TYPE_INT64 },
	};

	static const struct blobmsg_policy info_policy[__INFO_MAX] = {
		[UPTIME_DATA] = { .name = "uptime", .type = BLOBMSG_TYPE_INT32 },
		[MEMORY_DATA] = { .name = "memory", .type = BLOBMSG_TYPE_TABLE },
	};

	struct call_system_info * info = (struct call_system_info *)req->priv;
	struct blob_attr * tb[__INFO_MAX];
	struct blob_attr * memory[__MEMORY_MAX];

	blobmsg_parse(info_policy, __INFO_MAX, tb, blob_data(msg), blob_len(msg));

	if(!tb[MEMORY_DATA]) {
		info->status_code = MONITORING_ERR_MEMORY_OBJECT_MISSING;
		return;
	}
	
	if(!tb[UPTIME_DATA]) {
		info->status_code = MONITORING_ERR_UPTIME_MISSING;
		return;
	}

	blobmsg_parse(memory_policy, __MEMORY_MAX, memory, blobmsg_data(tb[MEMORY_DATA]), blobmsg_data_len(tb[MEMORY_DATA]));
	

	if(!memory[TOTAL_MEMORY] || !memory[FREE_MEMORY]) {
		info->status_code = MONITORING_ERR_MEMORY_FIELD_MISSING;
		return;
	}

	info->uptime = blobmsg_get_u32(tb[UPTIME_DATA]);
	info->total_ram = blobmsg_get_u64(memory[TOTAL_MEMORY]);
	info->free_ram = blobmsg_get_u64(memory[FREE_MEMORY]);
	
	info->status_code = 0;
	return;
}

monitoring_status_t fetch_system_info(monitoring_ctx * mctx, struct call_system_info * sdata) {
    sdata->status_code = -999;
    if(ubus_invoke(mctx->uctx, mctx->system_id, "info", NULL, system_info_cb, sdata, UBUS_TIMEOUT)) {
		return MONITORING_ERR_UBUS_INVOKE;
	}
	
    if(sdata->status_code != MONITORING_OK) {
		return sdata->status_code;
	}
	return MONITORING_OK;
}