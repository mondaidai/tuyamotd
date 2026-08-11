#include "telemetry.h"

#include <stddef.h>
#include <string.h>
#include <syslog.h>

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#endif

static monitoring_status_t telemetry_refresh(telemetry_ctx_t *ctx)
{
	system_info_t new_info = {0};
	monitoring_status_t status = gather_system_info(ctx->mctx, &new_info);

	if (status != MONITORING_OK) {
		free_system_info(&new_info);
		return status;
	}
	
	pthread_mutex_lock(&ctx->lock);
	free_system_info(&ctx->info);
	ctx->info = new_info;
	ctx->valid = true;
	pthread_mutex_unlock(&ctx->lock);

	return MONITORING_OK;
}

static void system_info_move(system_info_t *dest, system_info_t *src)
{
	if (!dest || !src)
		return;

	*dest = *src;
	memset(src, 0, sizeof(*src));
}

static void telemetry_timer_cb(struct uloop_timeout *timeout)
{
	telemetry_ctx_t *ctx = container_of(timeout, telemetry_ctx_t, timer);

	if (!ctx || !ctx->mctx || ctx->interval_msec == 0) {
		return;
	}

	monitoring_status_t status = telemetry_refresh(ctx);
	if (status != MONITORING_OK) {
		syslog(LOG_WARNING, "Telemetry gather failed: %s", monitoring_strerr(status));
	}

	uloop_timeout_set(timeout, ctx->interval_msec);
}

int telemetry_init(telemetry_ctx_t *ctx, monitoring_ctx *mctx, unsigned int interval_msec)
{
	if (!ctx || !mctx || interval_msec == 0) {
		return -1;
	}

	if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
		return -1;
	}

	memset(&ctx->info, 0, sizeof(ctx->info));
	memset(&ctx->timer, 0, sizeof(ctx->timer));

	ctx->mctx = mctx;
	ctx->valid = false;
	ctx->initialized = true;
	ctx->interval_msec = interval_msec;
	ctx->timer.cb = telemetry_timer_cb;

	if (uloop_timeout_set(&ctx->timer, interval_msec) != 0) {
		pthread_mutex_destroy(&ctx->lock);
		ctx->initialized = false;
		return -1;
	}

	return 0;
}

void telemetry_destroy(telemetry_ctx_t *ctx)
{
	if (!ctx || !ctx->initialized) {
		return;
	}

	uloop_timeout_cancel(&ctx->timer);

	pthread_mutex_lock(&ctx->lock);
	free_system_info(&ctx->info);
	ctx->valid = false;
	pthread_mutex_unlock(&ctx->lock);

	pthread_mutex_destroy(&ctx->lock);
	ctx->initialized = false;
}

bool telemetry_take(system_info_t *dest, telemetry_ctx_t *ctx)
{
	if (!dest || !ctx) {
		return false;
	}

	pthread_mutex_lock(&ctx->lock);
	if (!ctx->valid) {
		pthread_mutex_unlock(&ctx->lock);
		return false;
	}

	system_info_move(dest, &ctx->info);
	ctx->valid = false;
	pthread_mutex_unlock(&ctx->lock);

	return true;
}

int telemetry_set_interval(telemetry_ctx_t *ctx, unsigned int interval_msec)
{
	if (!ctx || !ctx->initialized || interval_msec == 0) {
		return -1;
	}

	pthread_mutex_lock(&ctx->lock);
	ctx->interval_msec = interval_msec;
	int result = uloop_timeout_set(&ctx->timer, interval_msec);
	pthread_mutex_unlock(&ctx->lock);

	return result == 0 ? 0 : -1;
}
