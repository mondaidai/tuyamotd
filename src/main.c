#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <libubox/uloop.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>

#include "signals.h"
#include "monitoring.h"
#include "telemetry.h"
#include "cli.h"
#include "actions.h"
#include "config.h"
#include "tuya_cacert.h"
#include "tuya_error_code.h"
#include "tuyalink_core.h"

#include "ubus_api.h"
#include "daemon_state.h"
#include "tuya_worker.h"

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
	
	int mret = APP_EXIT_SUCCESS;
	int ret = OPRT_OK;
	tuya_mqtt_context_t client;
	monitoring_ctx mctx = {0};
	struct ubus_context * uctx = NULL;
	telemetry_ctx_t telemetry = {0};
	bool worker_started = false;

	daemon_state_t state = {
		.lock = PTHREAD_MUTEX_INITIALIZER
	};

	tuya_worker_ctx_t worker_ctx = {
		.client = &client,
		.state = &state,
		.telemetry = &telemetry,
	};
	pthread_t worker;

	openlog("tuyamotd", LOG_PID | LOG_CONS, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOG_INFO)); // disable debug 
	app_args args;

	if(parse_args(argc, argv, &args) != 0)	
	{
		mret = APP_EXIT_INVALID_ARGS;
		goto cleanup;
	}

	if(args.is_daemon) {
		if(daemonize() != 0) {
			mret = APP_EXIT_DAEMONIZE_FAILED;
			syslog(LOG_ERR, "Failed to create a daemon.");
			goto cleanup;
		}
	}

// ----------- Tuya MQTT Initialization and Connection -----------

	ret = tuya_mqtt_init(&client, &(const tuya_mqtt_config_t) {
		.host = TUYA_SERVER_DEFAULT_HOST,
		.port = TUYA_SERVER_DEFAULT_PORT,
		.cacert = tuya_cacert_pem,
		.cacert_len = sizeof(tuya_cacert_pem),
		.device_id = args.device_id,
		.device_secret = args.device_secret,
		.keepalive = TUYA_SERVER_DEFAULT_KEEPALIVE,
		.timeout_ms = TUYA_MQTT_DEFAULT_TIMEOUT_MS,
		.on_connected = on_connected,
		.on_disconnect = on_disconnect,
		.on_messages = on_messages
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
	
// ----------- UBus Initialization and Monitoring Setup -----------

	uloop_init();

	uctx = ubus_connect(NULL);
	
	if(!uctx) {
		syslog(LOG_ERR, "Failed to connect to ubus");
		mret = APP_EXIT_MONITORING_FAILED;
		goto cleanup_ubus;
	}

	if(monitoring_init(&mctx, uctx) != MONITORING_OK) {
		syslog(LOG_ERR, "Failed to init monitoring module");
		goto cleanup_ubus;
	}

	ubus_add_uloop(uctx);

	if (ubus_api_init(uctx, &telemetry, &state) != 0) {
		syslog(LOG_ERR, "Failed to init ubus API");
		mret = APP_EXIT_MONITORING_FAILED;
		goto cleanup_ubus;
	}

	if (telemetry_init(&telemetry, &mctx, DEFAULT_REPORT_INTERVAL_SEC * 1000) != 0) {
		syslog(LOG_ERR, "Failed to init telemetry");
		mret = APP_EXIT_MONITORING_FAILED;
		goto cleanup_ubus;
	}

	if(pthread_create(&worker, NULL, tuya_worker, &worker_ctx) != 0) {
    	mret = APP_EXIT_THREAD_CREATE_FAILED;
		syslog(LOG_ERR, "Failed to create worker thread");
    	goto cleanup_ubus;
	}
	worker_started = true;

	uloop_run();
	keep_running = 0;
	syslog(LOG_INFO, "Shutting down gracefully");
	goto cleanup_ubus;

cleanup_ubus:
	if (worker_started) {
		keep_running = 0;
		pthread_join(worker, NULL);
		worker_started = false;
	}
	telemetry_destroy(&telemetry);
	monitoring_destroy(&mctx);
	if (uctx) {
		ubus_free(uctx);
		uctx = NULL;
	}
	uloop_done();
cleanup_tuya:
	tuya_mqtt_disconnect(&client);
	tuya_mqtt_deinit(&client);
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