#include <openthread/thread.h>
#include <zephyr/net/openthread.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openthread, LOG_LEVEL_DBG);

#define CSL_LOW_LATENCY_PERIOD_MS 	10
#define CSL_NORMAL_LATENCY_PERIOD_MS 	500


bool openthread_ready = false;

static void on_thread_state_changed(otChangedFlags flags,
				    struct openthread_context *ot_context,
				    void *user_data)
{
	if (flags & OT_CHANGED_IP6_ADDRESS_ADDED) {
		LOG_INF("openthread ready!");
		openthread_ready = true;
	}
}

static struct openthread_state_changed_cb ot_state_chaged_cb = {
	.state_changed_cb = on_thread_state_changed
};

void openthread_set_csl_period_ms(int period_ms)
{
	otError otErr;
	otInstance *instance = openthread_get_default_instance();

	otErr = otLinkCslSetPeriod(instance,
			period_ms * 1000 / OT_US_PER_TEN_SYMBOLS);
}

bool openthread_is_ready()
{
	return openthread_ready;
}

void openthread_set_low_latency()
{
	otLinkSetPollPeriod(openthread_get_default_instance(), 10);
	// openthread_set_csl_period_ms(CSL_LOW_LATENCY_PERIOD_MS);
}

void openthread_set_normal_latency()
{
	otLinkSetPollPeriod(openthread_get_default_instance(), 0);
	// openthread_set_csl_period_ms(CSL_NORMAL_LATENCY_PERIOD_MS);
}

int openthread_erase_persistent_info(void)
{
	struct openthread_context *ot_context = openthread_get_default_context();
	otError err;

	openthread_api_mutex_lock(ot_context);
	err = otInstanceErasePersistentInfo(ot_context->instance);
	openthread_api_mutex_unlock(ot_context);

	if (err != OT_ERROR_NONE) {
		return -1;
	}

	return 0;
}

int openthread_my_start(void)
{
	int ret;
	struct openthread_context *ot_context = openthread_get_default_context();

	ret = openthread_state_changed_cb_register(ot_context,
						   &ot_state_chaged_cb);
	if (ret < 0) {
		LOG_ERR("Could register callback");
		return ret;
	}

	return openthread_start(ot_context);
}

int openthread_wait_for_ready(void)
{
	while (!openthread_is_ready())
		k_sleep(K_MSEC(100));

	k_sleep(K_MSEC(100)); // Something else is not ready, not sure what

	return 0;
}
