#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <libubox/uloop.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>

#include "signals.h"
#include "cli.h"
#include "actions.h"
#include "config.h"
#include "esp_actions.h"

#include "tuya_cacert.h"
#include "tuya_error_code.h"
#include "tuyalink_core.h"

typedef enum {
    APP_EXIT_SUCCESS = 0,

    APP_EXIT_INVALID_ARGS = 1,
    APP_EXIT_DAEMONIZE_FAILED = 2,

    APP_EXIT_MQTT_INIT_FAILED = 3,
    APP_EXIT_MQTT_CONNECT_FAILED = 4,

    APP_EXIT_MQTT_INVALID_PARAM = 5,
    APP_EXIT_MQTT_TIMEOUT = 6,

    APP_EXIT_MONITORING_FAILED = 7,
	APP_EXIT_THREAD_CREATE_FAILED = 8
} app_exit_code_t;


static inline int daemonize(void);

int main(int argc, char * argv[]) 
{
	Signal(SIGINT, handle_signal);
	Signal(SIGTERM, handle_signal);
	Signal(SIGHUP, handle_signal);
	Signal(SIGQUIT, handle_signal);
	
	app_config_t app_config = {0};
	int mret = APP_EXIT_SUCCESS;
	int ret = OPRT_OK;
	tuya_mqtt_context_t client;
	esp_ctx *ectx = NULL;
	struct ubus_context * uctx = NULL;

	ectx = calloc(1, sizeof(esp_ctx));
	if(!ectx) {
		syslog(LOG_ERR, "Failed to allocate esp context");
		mret = APP_EXIT_MONITORING_FAILED;
		goto cleanup;
	}

	openlog("tuyamotd", LOG_PID | LOG_CONS, LOG_DAEMON);
	app_args args;

	if(parse_args(argc, argv, &args) != 0)	
	{
		mret = APP_EXIT_INVALID_ARGS;
		goto cleanup;
	}

	if(config_load(&app_config) != 0 || config_validate(&app_config) != 0) {
		mret = APP_EXIT_INVALID_ARGS;
		goto cleanup;
	}

	setlogmask(LOG_UPTO(app_config.log_level));

	if(args.is_daemon) {
		if(daemonize() != 0) {
			mret = APP_EXIT_DAEMONIZE_FAILED;
			syslog(LOG_ERR, "Failed to create a daemon.");
			goto cleanup;
		}
	}

	uctx = ubus_connect(NULL);
	if(!uctx) {
		syslog(LOG_ERR, "Failed to connect to ubus");
		mret = APP_EXIT_MONITORING_FAILED;
		goto cleanup_ubus;
	}

	if(esp_init(ectx, uctx) != ESP_OK) {
		syslog(LOG_ERR, "Failed to init esp actions module");
		goto cleanup_ubus;
	}

    /* set ubus timeout from config */
    ectx->ubus_timeout_ms = app_config.ubus_timeout_ms;

// ----------- Tuya MQTT Initialization and Connection -----------

	ret = tuya_mqtt_init(&client, &(const tuya_mqtt_config_t) {
		.host = app_config.mqtt_host,
		.port = app_config.mqtt_port,
		.cacert = tuya_cacert_pem,
		.cacert_len = sizeof(tuya_cacert_pem),
		.device_id = args.device_id,
		.device_secret = args.device_secret,
		.keepalive = app_config.mqtt_keepalive,
		.timeout_ms = DEFAULT_MQTT_TIMEOUT_MS,
		.on_connected = on_connected,
		.on_disconnect = on_disconnect,
		.on_messages = on_messages,
		.user_data = (void*)ectx
	});

	if(ret != OPRT_OK)
	{
		syslog(LOG_ERR, "tuya_mqtt_init failed: %d", ret);
		mret = APP_EXIT_MQTT_INIT_FAILED;
		goto cleanup;
	}
	syslog(LOG_INFO, "tuya_mqtt_init successful");

    ret = tuya_mqtt_connect(&client);
	
    if(ret == OPRT_OK) {
        syslog(LOG_INFO, "tuya_mqtt_connect successful");
    } else {
        syslog(LOG_ERR, "tuya_mqtt_connect failure: %d", ret);
        mret = APP_EXIT_MQTT_CONNECT_FAILED;
        goto cleanup_tuya;
    }

	const int mqtt_failure_timeout_sec = 6*60*60;
	const int mqtt_backoff_cap = 30*60; 
	long mqtt_retry_sleep, mqtt_failure_start;

	while(keep_running) {
		ret = tuya_mqtt_loop(&client);

		if(ret == OPRT_OK) {
			mqtt_failure_start = 0;
			mqtt_retry_sleep = 1;
			continue;
		}

		if(ret == OPRT_INVALID_PARM) {
			syslog(LOG_ERR, "tuya_mqtt_loop invalid parameter");
			break;
		}

		if(mqtt_failure_start == 0) {
			mqtt_failure_start = time(NULL);
		} else if((time(NULL) - mqtt_failure_start) >= mqtt_failure_timeout_sec) {
			syslog(LOG_ERR, "MQTT failing for too long");
			break;
		}

		syslog(LOG_WARNING, "tuya_mqtt_loop returned %d, retrying in %u seconds", ret, mqtt_retry_sleep);

		for (unsigned int i = 0; i < mqtt_retry_sleep && keep_running; i++) {
			sleep(1);
		}

		if(!keep_running)
			break;

		if(mqtt_retry_sleep < mqtt_backoff_cap) {
			mqtt_retry_sleep *= 2;
			if(mqtt_retry_sleep > mqtt_backoff_cap)
				mqtt_retry_sleep = mqtt_backoff_cap;
		}
	}
	
cleanup_ubus:

	if(ectx) {
		esp_destroy(ectx);
	}
	if(uctx) {
		ubus_free(uctx);
		uctx = NULL;
	}
	if(ectx) {
		free(ectx);
		ectx = NULL;
	}
cleanup_tuya:
	tuya_mqtt_disconnect(&client);
	tuya_mqtt_deinit(&client);
cleanup:
	config_destroy(&app_config);
	syslog(LOG_INFO, "Shutting down gracefully");
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