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

int monitor_collect(system_info_t * info) {
	struct sysinfo sinfo;
	if(sysinfo(&sinfo) < 0) 
		return -1;
	info->total_ram = sinfo.totalram;
	info->free_ram = sinfo.freeram;
	info->system_uptime = sinfo.uptime;

	info->cpu_usage = get_cpu_usage();
	info->interfaces[0] = '\0';
	int ret = get_network_interfaces(info->interfaces, sizeof(info->interfaces));
	
	if(ret < 0)
		return -2;

	return 0;
}

int get_cpu_stats(CPUStats *stats)
{
	FILE *fp;

	if (stats == NULL)
		return -1;

	fp = fopen("/proc/stat", "r");
	if (fp == NULL)
		return -1;

	int ret = fscanf(fp,
		"cpu %" SCNu64 " %" SCNu64 " %" SCNu64
		" %" SCNu64 " %" SCNu64 " %" SCNu64
		" %" SCNu64 " %" SCNu64,
		&stats->user,
		&stats->nice,
		&stats->system,
		&stats->idle,
		&stats->iowait,
		&stats->irq,
		&stats->softirq,
		&stats->steal);

	fclose(fp);

	return (ret == 8) ? 0 : -1;
}

// usage = (delta_total - delta_idle) / delta_total * 100
uint8_t get_cpu_usage(void)
{
	CPUStats stats;

	if (get_cpu_stats(&stats) != 0)
		return 0;

	uint64_t total = CPU_TOTAL(stats);
	uint64_t idle  = CPU_IDLE(stats);

	uint64_t delta_total = total - prev_total;
	uint64_t delta_idle  = idle - prev_idle;

	prev_total = total;
	prev_idle = idle;

	if (delta_total == 0)
		return 0;

	return (uint8_t)(
		100.0 * (delta_total - delta_idle) /
		delta_total
	);
}
void cpu_usage_init(void)
{
	CPUStats stats;

	if (get_cpu_stats(&stats) != 0)
		return;

	prev_total = CPU_TOTAL(stats);
	prev_idle  = CPU_IDLE(stats);
		
	return;
}

static uint64_t read_counter(const char *path)
{
	uint64_t value = 0;
	FILE *fp = fopen(path, "r");

	if (fp != NULL) {
		if(fscanf(fp, "%" SCNu64, &value) != 1)
			value = 0;
		fclose(fp);
	}

	return value;
}

int get_network_interfaces(char *interfaces, size_t interfaces_size)
{
	int skipped_interfaces = 0;
	struct ifaddrs *ifaddr;
	struct ifaddrs *ifa;
	char ip[INET_ADDRSTRLEN];
	char netmask[INET_ADDRSTRLEN];
	char path[256];
	char entry[256];

	if (interfaces == NULL || interfaces_size == 0)
		return -1;

	interfaces[0] = '\0';

	if (getifaddrs(&ifaddr) != 0)
		return -2;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		uint64_t rx;
		uint64_t tx;
		size_t len;
		int ret;

		if (ifa->ifa_addr == NULL) {
			skipped_interfaces++;
			continue;
		}

		if (ifa->ifa_netmask == NULL) {
			skipped_interfaces++;
			continue;
		}

		if (strcmp(ifa->ifa_name, "lo") == 0) {
			skipped_interfaces++;
			continue;
		}

		if (ifa->ifa_addr->sa_family != AF_INET) {
			skipped_interfaces++;
			continue;
		}

		if (inet_ntop(AF_INET,
					  &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
					  ip,
					  sizeof(ip)) == NULL)
		{
			skipped_interfaces++;
			continue;
		}

		if (inet_ntop(AF_INET,
					  &((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr,
					  netmask,
					  sizeof(netmask)) == NULL)
		{
			skipped_interfaces++;
			continue;
		}

		ret = snprintf(path,
					   sizeof(path),
					   "/sys/class/net/%s/statistics/rx_bytes",
					   ifa->ifa_name);

		if (ret < 0 || (size_t)ret >= sizeof(path)) {
			skipped_interfaces++;
			continue;
		}

		rx = read_counter(path);

		ret = snprintf(path,
					   sizeof(path),
					   "/sys/class/net/%s/statistics/tx_bytes",
					   ifa->ifa_name);

		if (ret < 0 || (size_t)ret >= sizeof(path)) {
			skipped_interfaces++;
			continue;
		}

		tx = read_counter(path);

		ret = snprintf(entry,
				sizeof(entry),
				"%s=%s/%s,%-" PRIu64 ",%-" PRIu64 ";",
				ifa->ifa_name,
				ip,
				netmask,
				rx,
				tx);

		if (ret < 0 || (size_t)ret >= sizeof(entry)) {
			skipped_interfaces++;
			continue;
		}

		len = strlen(interfaces);

		if (len + (size_t)ret >= interfaces_size) {
			skipped_interfaces++;
			break;
		}

		memcpy(interfaces + len, entry, (size_t)ret);
		interfaces[len + ret] = '\0';
	}

	freeifaddrs(ifaddr);

	return skipped_interfaces;
}