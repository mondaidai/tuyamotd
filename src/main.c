#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "signals.h"
#include "monitoring.h"
#include "cli.h"
#include "actions.h"

#include "tuya_cacert.h"
#include "tuya_error_code.h"
#include "tuyalink_core.h"

bool time_to_report(void)
{
	static time_t last_report = 0;
	time_t now = time(NULL);

	if (last_report == 0 ||
		difftime(now, last_report) >= 15.0)
	{
		last_report = now;
		return true;
	}

	return false;
}

static inline int daemonize(void);

int main(int argc, char * argv[]) 
{
	Signal(SIGINT, handle_signal);
	Signal(SIGTERM, handle_signal);
	Signal(SIGHUP, handle_signal);
	Signal(SIGQUIT, handle_signal);
	
	int mret = 0;
	openlog("tuya_daemon", LOG_PID | LOG_CONS, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOG_INFO)); // disable debug 
	app_args args;

	if(parse_args(argc, argv, &args) != 0)
	{
		mret = 1;
		goto cleanup;
	}
	pid_t pid;
	if(args.is_daemon) {
		if(daemonize() != 0) {
			mret = 5;
			syslog(LOG_ERR, "Failed to create a daemon.");
			goto cleanup;
		}
	}
	int ret = OPRT_OK;
	tuya_mqtt_context_t client;
	
	ret = tuya_mqtt_init(&client, &(const tuya_mqtt_config_t) {
		.host = "m1.tuyacn.com",
		.port = 8883,
		.cacert = tuya_cacert_pem,
		.cacert_len = sizeof(tuya_cacert_pem),
		.device_id = args.device_id,
		.device_secret = args.device_secret,
		.keepalive = 60,
		.timeout_ms = 1000,
		.on_connected = on_connected,
		.on_disconnect = on_disconnect,
		.on_messages = on_messages
	});

	if(ret != OPRT_OK)
	{
		syslog(LOG_ERR, "tuya_mqtt_init failed: %d", ret);
		mret = 3;
		goto cleanup;
	}
	syslog(LOG_INFO, "tuya_mqtt_init successful");

	ret = tuya_mqtt_connect(&client);
	if(ret == OPRT_OK) {
		syslog(LOG_INFO, "tuya_mqtt_connect successful");
	} else {
		syslog(LOG_ERR, "tuya_mqtt_connect failure: %d", ret);
		goto cleanup;
	}

	if (ret != OPRT_OK) {
		mret = 4;
		goto cleanup;
	}
	
	cpu_usage_init();

 	while(keep_running) {
		int loop_ret = tuya_mqtt_loop(&client);
		if (loop_ret != OPRT_OK) {
			syslog(LOG_WARNING, "tuya_mqtt_loop returned %d", loop_ret);
		}
		if(time_to_report()) {
			send_system_info(&client);
		}
 	}
	tuya_mqtt_disconnect(&client);
	tuya_mqtt_deinit(&client);

	syslog(LOG_INFO, "Shutting down gracefully");
cleanup:
	closelog();
	return mret;
}
static inline int daemonize(void)
{
		pid_t pid;	
		pid = fork();
		if(pid < 0)
			return 1;
		if(pid > 0)
			exit(EXIT_SUCCESS);
		if(setsid() < 0)
			return 2;
		pid = fork();
		if(pid < 0)
			return 1;
		if(pid > 0)
			exit(EXIT_SUCCESS);
		umask(0);
		if(chdir("/") != 0)
			return 3;
		int fd = open("/dev/null", O_RDWR);
		if(fd != -1) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if(fd > STDERR_FILENO) {
				close(fd);
			}
		} else return 4;
	return 0;
}