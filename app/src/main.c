#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <app_version.h>
#include <stdio.h>

#include "hvac.h"
#include "ha.h"
#include "openthread.h"


int main(void)
{
	hvac_t hvac;
	hvac_cfg_t hvac_cfg;

	int ret;
	uint16_t scd4x_serial_words[3];
	char scd4x_serial_string[128];

	LOG_INF("\n\n🐨 MAIN START 🐨\n");

	openthread_enable_ready_flag();

	while (!openthread_is_ready())
		k_sleep(K_MSEC(100));

	// Something else is not ready, not sure what
	k_sleep(K_MSEC(100));

	hvac.i2c.dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	hvac_cfg.i2c_address = HVAC_SCD40_SLAVE_ADDR;

	LOG_INF("Version: %s", APP_VERSION_FULL);

	hvac_init(&hvac, &hvac_cfg);
	hvac_scd40_get_serial_number(&hvac, scd4x_serial_words);

	ret = snprintf(scd4x_serial_string, sizeof(scd4x_serial_string),
		       "%04x%04x%04x",
		       scd4x_serial_words[0],
		       scd4x_serial_words[1],
		       scd4x_serial_words[2]);
	if (ret < 0 && ret >= sizeof(scd4x_serial_string)) {
		LOG_ERR("Could not set scd4x_serial_string");
		return -ENOMEM;
	}

	LOG_INF("SCD4x - Serial Number : %s", scd4x_serial_string);

	ha_start(scd4x_serial_string, "");

	LOG_INF("****************************************");
	LOG_INF("MAIN DONE");
	LOG_INF("****************************************");

	return 0;
}
