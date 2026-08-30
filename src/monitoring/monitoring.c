#include "monitoring.h"
#include <sys/sysinfo.h>
#include <stdio.h>
#include <inttypes.h>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

#define CPU_TOTAL(stats) ((stats).user + (stats).nice + (stats).system + (stats).idle + (stats).iowait + (stats).irq + (stats).softirq + (stats).steal)
#define CPU_IDLE(stats) ((stats).idle + (stats).iowait)

static uint64_t prev_total;
static uint64_t prev_idle;

int gather_system_info(monitoring_ctx * mctx, system_info_t * info) {
	
	struct call_system_info sdata = {0};
	monitoring_status_t status = fetch_system_info(mctx, &sdata);
	if(status != MONITORING_OK) {
		return status;
	}

	info->total_ram = sdata.total_ram;
	info->free_ram = sdata.free_ram;
	info->system_uptime = sdata.uptime;

	network_interface_t * interfaces = NULL;
	uint16_t interface_count = 0;

	status = fetch_interface_info(mctx, &interfaces, &interface_count);
	if(status != MONITORING_OK) {
		return status;
	}

	info->interfaces = interfaces;
	info->num_interfaces = interface_count;

	info->cpu_usage = get_cpu_usage();

	return MONITORING_OK;
}

void free_system_info(system_info_t *info)
{
	if (!info) {
		return;
	}

	free(info->interfaces);
	memset(info, 0, sizeof(*info));
}

static int get_cpu_stats(CPUStats * stats)
{
	FILE *fp;
	uint64_t guest, guest_nice;

	if(stats == NULL)
		return MONITORING_ERR_INVALID_ARG;

	memset(stats, 0, sizeof(*stats));

	fp = fopen("/proc/stat", "r");
	if(fp == NULL)
		return MONITORING_ERR_OPEN_FILE;

	// /proc/stat's first "cpu" line is the aggregate across all cores.
	int ret = fscanf(fp,
		"cpu %" SCNu64 " %" SCNu64 " %" SCNu64
		" %" SCNu64 " %" SCNu64 " %" SCNu64
		" %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
		&stats->user,
		&stats->nice,
		&stats->system,
		&stats->idle,
		&stats->iowait,
		&stats->irq,
		&stats->softirq,
		&stats->steal,
		&guest,
		&guest_nice);

	fclose(fp);

	return (ret >= 8) ? MONITORING_OK : MONITORING_ERR_INVALID_ARG;
}

// usage = (delta_total - delta_idle) / delta_total * 100
uint8_t get_cpu_usage(void)
{
	CPUStats stats;

	if(get_cpu_stats(&stats) != 0)
		return 0;

	uint64_t total = CPU_TOTAL(stats);
	uint64_t idle  = CPU_IDLE(stats);

	uint64_t delta_total = total - prev_total;
	uint64_t delta_idle  = idle - prev_idle;

	prev_total = total;
	prev_idle = idle;

	if(delta_total == 0)
		return MONITORING_OK;

	uint32_t usage_percent = (uint32_t)(100.0 * (double)(delta_total - delta_idle) / (double)delta_total + 0.5);
	if(usage_percent > 100)
		usage_percent = 100;

	return (uint8_t)usage_percent;
}

void cpu_usage_init(void)
{
	CPUStats stats;

	if(get_cpu_stats(&stats) != 0)
		return;

	prev_total = CPU_TOTAL(stats);
	prev_idle  = CPU_IDLE(stats);
		
	return;
}

int monitoring_init(monitoring_ctx * mctx, struct ubus_context *uctx) {
	
	if(!mctx || !uctx) {
		return MONITORING_ERR_INVALID_ARG;
	}

	uint32_t system_id;
	if(ubus_lookup_id(uctx, "system", &system_id)) {
		return MONITORING_ERR_UBUS_LOOKUP;
	}
	uint32_t network_device_id;
	if(ubus_lookup_id(uctx, "network.device", &network_device_id)) {
		return MONITORING_ERR_UBUS_LOOKUP;
	}
	uint32_t network_interface_id;
	if(ubus_lookup_id(uctx, "network.interface", &network_interface_id)) {
		return MONITORING_ERR_UBUS_LOOKUP;
	}

	mctx->uctx = uctx;
	mctx->system_id = system_id;
	mctx->network_device_id = network_device_id;
	mctx->network_interface_id = network_interface_id;
	return MONITORING_OK;
}

void monitoring_destroy(monitoring_ctx * mctx) {
	if (!mctx) {
		return;
	}
	memset(mctx, 0, sizeof(monitoring_ctx));
	return;
}

const char * monitoring_strerr(monitoring_status_t status) {
	switch(status) {
		case MONITORING_OK:
			return "Operation completed successfully";
		case MONITORING_ERR_INVALID_ARG:
			return "Invalid argument provided";
		case MONITORING_ERR_UBUS_CONNECT:
			return "Failed to connect to UBus";
		case MONITORING_ERR_UBUS_LOOKUP:
			return "Failed to lookup UBus object";
		case MONITORING_ERR_UBUS_INVOKE:
			return "Failed to invoke UBus method";
		case MONITORING_ERR_PARSE:
			return "Failed to parse response";
		case MONITORING_ERR_CPU_STATS:
			return "Failed to retrieve CPU statistics";
		case MONITORING_ERR_OPEN_FILE:
			return "Failed to open file";
		case MONITORING_ERR_INTERFACE:
			return "Failed to retrieve network interface information";
		case MONITORING_ERR_MEMORY_OBJECT_MISSING:
			return "Memory object is missing";
		case MONITORING_ERR_UPTIME_MISSING:
			return "Uptime information is missing";
		case MONITORING_ERR_MEMORY_FIELD_MISSING:
			return "Memory field is missing";
		case MONITORING_ERR_NOMEM:
			return "Memory allocation failed";
		default:
			return "Unknown error";
	}
}
