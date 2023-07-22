#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <app_version.h>

#include "hvac.h"
#include "ha.h"
#include "openthread.h"


int main(void)
{
	hvac_t hvac;
	hvac_cfg_t hvac_cfg;

	uint16_t ser_num[3];

	LOG_INF("\n\n🐨 MAIN START 🐨\n");

	openthread_enable_ready_flag();

	while (!openthread_is_ready())
		k_sleep(K_MSEC(100));

	// Something else is not ready, not sure what
	k_sleep(K_MSEC(100));

	ha_start("", "");

	hvac.i2c.dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	hvac_cfg.i2c_address = HVAC_SCD40_SLAVE_ADDR;

	LOG_INF("Version: %s", APP_VERSION_FULL);

	hvac_init(&hvac, &hvac_cfg);
	hvac_scd40_get_serial_number(&hvac, ser_num);
	LOG_INF("SCD40 - Serial Number : %.4d-%.4d-%.4d", 
		ser_num[0], ser_num[1], ser_num[2]);

	LOG_INF("****************************************");
	LOG_INF("MAIN DONE");
	LOG_INF("****************************************");

	return 0;
}
