#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <app_version.h>

#include "hvac.h"


#define SLEEP_TIME_MS   10


void main(void)
{
	hvac_t hvac;
	hvac_cfg_t hvac_cfg;

	hvac.i2c.dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	hvac_cfg.i2c_address = HVAC_SCD40_SLAVE_ADDR;

	LOG_INF("Version: %s", APP_VERSION_FULL);

	hvac_init(&hvac, &hvac_cfg);

	LOG_INF("****************************************");
	LOG_INF("MAIN DONE");
	LOG_INF("****************************************");
}
