#ifndef MONITORING_H
#define MONITORING_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
	uint64_t total_ram;
	uint64_t free_ram;
	uint64_t system_uptime;
	uint8_t cpu_usage;
	char interfaces[1024];
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

int monitor_collect(system_info_t * info);

int get_cpu_stats(CPUStats * stats);
void cpu_usage_init(void);
uint8_t get_cpu_usage(void);
int get_network_interfaces(char * interfaces, size_t interfaces_size);
#endif
