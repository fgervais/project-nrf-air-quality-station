#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <app_version.h>


#define SLEEP_TIME_MS   10


void main(void)
{
	int ret;

	LOG_INF("Version: %s", APP_VERSION_FULL);

	LOG_INF("****************************************");
	LOG_INF("MAIN DONE");
	LOG_INF("****************************************");
}
