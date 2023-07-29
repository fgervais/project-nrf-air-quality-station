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


static int get_scd4x_serial_as_string(hvac_t *hvac_ctx,
				      char *sn_buf, size_t sn_buf_size)
{
	int ret;
	uint16_t scd4x_serial_words[3];

	hvac_scd40_get_serial_number(hvac_ctx, scd4x_serial_words);

	ret = snprintf(sn_buf, sn_buf_size,
		       "%04x%04x%04x",
		       scd4x_serial_words[0],
		       scd4x_serial_words[1],
		       scd4x_serial_words[2]);
	if (ret < 0 && ret >= sn_buf_size) {
		LOG_ERR("Could not set sn_buf");
		return -ENOMEM;
	}

	LOG_INF("SCD4x serial number: %s", sn_buf);
	return 0;
}

static int get_sps30_serial_as_string(hvac_t *hvac_ctx,
				      char *sn_buf, size_t sn_buf_size)
{
	int ret;

	ret = hvac_sps30_get_serial_number(hvac_ctx, sn_buf, sn_buf_size);

	LOG_INF("SPS30 serial number: %s", sn_buf);
	return 0;
}


static int generate_unique_id(char *uid_buf, size_t uid_buf_size,
			      const char *part_number,
			      const char *sensor_name,
			      const char *serial_number)
{
	int ret;

	ret = snprintf(uid_buf, uid_buf_size,
		       "%s_%s_%s",
		       part_number, serial_number, sensor_name);
	if (ret < 0 && ret >= uid_buf_size) {
		LOG_ERR("Could not set uid_buf");
		return -ENOMEM;
	}

	LOG_INF("📇 unique id: %s", uid_buf);
	return 0;
}

int main(void)
{
	hvac_t hvac;
	hvac_cfg_t hvac_cfg;

	int ret;
	char scd4x_serial_string[128];
	char sps30_serial_string[HVAC_SPS30_MAX_SERIAL_LEN];
	char scd4x_co2_unique_id_string[128];
	char sps30_pm25_unique_id_string[128];

	struct ha_sensor co2_sensor = {
		.name = "CO₂",
		.unique_id = scd4x_co2_unique_id_string,
		.device_class = "carbon_dioxide",
		.state_class = "measurement",
	};

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
	ret = get_scd4x_serial_as_string(&hvac, scd4x_serial_string,
					 sizeof(scd4x_serial_string));
	if (ret < 0) {
		LOG_ERR("Could not get scd4x serial number");
		return ret;
	}

	ret = get_sps30_serial_as_string(&hvac,
					 sps30_serial_string,
					 sizeof(sps30_serial_string));
	if (ret < 0) {
		LOG_ERR("Could not get sps30 serial number");
		return ret;
	}

	ret = generate_unique_id(scd4x_co2_unique_id_string,
				 sizeof(scd4x_co2_unique_id_string),
				 "scd4x", "co2",
				 scd4x_serial_string);
	if (ret < 0) {
		LOG_ERR("Could not generate scd4x unique id");
		return ret;
	}

	ret = generate_unique_id(sps30_pm25_unique_id_string,
				 sizeof(sps30_pm25_unique_id_string),
				 "sps30", "pm25",
				 sps30_serial_string);
	if (ret < 0) {
		LOG_ERR("Could not generate sps30 unique id");
		return ret;
	}

	// COMMENT OUT FOR FIRST TEST
	// ha_start();
	// int ha_register_sensor(&co2_sensor);
	// --------------------------

	hvac_scd40_send_cmd(&hvac, HVAC_START_PERIODIC_MEASUREMENT);
	hvac_sps30_start_measurement (&hvac);

	k_sleep(K_SECONDS(10));

	LOG_INF("🎉 Init done 🎉");

	measuremen_data_t hvac_data;
	mass_and_num_cnt_data_t sps30_data;

	while (1) {
		hvac_scd40_read_measurement(&hvac, &hvac_data);

		LOG_INF("SCD4x");
		LOG_INF("├── CO2 Concentration = %d ppm", hvac_data.co2_concent);
		LOG_INF("├── Temperature = %.2f °C", hvac_data.temperature);
		LOG_INF("└── R. Humidity = %.2f %%", hvac_data.r_humidity);

		hvac_sps30_read_measured_data(&hvac, &sps30_data);

		LOG_INF("SPS30");
		LOG_INF("├── Mass concentration");
		LOG_INF("│   ├── PM 1.0 = %.2f μg/m³", sps30_data.mass_pm_1_0);
		LOG_INF("│   ├── PM 2.5 = %.2f μg/m³", sps30_data.mass_pm_2_5);
		LOG_INF("│   ├── PM 4.0 = %.2f μg/m³", sps30_data.mass_pm_4_0);
		LOG_INF("│   └── PM 10  = %.2f μg/m³", sps30_data.mass_pm_10);

		LOG_INF("└── Number Concentration");
		LOG_INF("    ├── PM 0.5 = %.2f n/cm³", sps30_data.num_pm_0_5);
		LOG_INF("    ├── PM 1.0 = %.2f n/cm³", sps30_data.num_pm_1_0);
		LOG_INF("    ├── PM 2.5 = %.2f n/cm³", sps30_data.num_pm_2_5);
		LOG_INF("    ├── PM 4.0 = %.2f n/cm³", sps30_data.num_pm_4_0);
		LOG_INF("    └── PM 10  = %.2f n/cm³", sps30_data.num_pm_10);

		LOG_INF("💤 End of main loop 💤");
		k_sleep(K_SECONDS(60));
	}

	return 0;
}
