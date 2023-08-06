#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <app_version.h>
#include <stdio.h>

#include "ha.h"
#include "hvac.h"
#include "mqtt.h"
#include "openthread.h"
#include "reset.h"
#include "temphum24.h"


#define SEDONDS_IN_BETWEEN_SENSOR_READING	10
#define NUMBER_OF_READINGS_IN_AVERAGE		6


static int get_hdc302x_serial_as_string(temphum24_t *temphum_ctx,
					char *sn_buf, size_t sn_buf_size)
{
	int ret;
	uint16_t serial_number_words[3];

	ret = temphum24_get_serial_number(temphum_ctx, serial_number_words);
	if (ret < 0) {
		LOG_ERR("temphum24: could not read hdc302x serial number");
		return ret;
	}

	ret = snprintf(sn_buf, sn_buf_size,
		       "%04x%04x%04x",
		       serial_number_words[0],
		       serial_number_words[1],
		       serial_number_words[2]);
	if (ret < 0 && ret >= sn_buf_size) {
		LOG_ERR("Could not set sn_buf");
		return -ENOMEM;
	}

	LOG_INF("HDC302x serial number: %s", sn_buf);
	return 0;
}

static int get_scd4x_serial_as_string(hvac_t *hvac_ctx,
				      char *sn_buf, size_t sn_buf_size)
{
	int ret;
	uint16_t scd4x_serial_words[3];

	ret = hvac_scd40_get_serial_number(hvac_ctx, scd4x_serial_words);
	if (ret < 0) {
		LOG_ERR("hvac: could not read scd4x serial number");
		return ret;
	}

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
	if (ret < 0) {
		LOG_ERR("hvac: could not read sps30 serial number");
		return ret;
	}

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

static void scd4x_reset(hvac_t *hvac)
{
	hvac_scd40_send_cmd(hvac, HVAC_STOP_PERIODIC_MEASUREMENT);
	k_sleep(K_MSEC(500));
	hvac_scd40_send_cmd(hvac, HVAC_REINIT);
	k_sleep(K_MSEC(30));
}

static int temphum24_click_init(temphum24_t *temphum24)
{
	int ret;
	temphum24_cfg_t temphum24_cfg;

	temphum24_cfg_setup(&temphum24_cfg);
	temphum24->i2c.dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	temphum24->rst.port = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	temphum24->alert.port = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	temphum24_cfg.rst = 5;
	temphum24_cfg.alert = 29;
	ret = temphum24_init(temphum24, &temphum24_cfg);
	if (ret < 0) {
		LOG_ERR("Could not initialize hdc302x");
		return ret;
	}

	return 0;
}

static int hvac_click_init(hvac_t *hvac)
{
	int ret;
	hvac_cfg_t hvac_cfg;

	hvac_cfg_setup(&hvac_cfg);
	hvac->i2c.dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	ret = hvac_init(hvac, &hvac_cfg);
	if (ret < 0) {
		LOG_ERR("Could not initialize hvac");
		return ret;
	}

	scd4x_reset(hvac);

	return 0;
}

static int watchdog_new_channel(const struct device *wdt)
{
	int ret;

	struct wdt_timeout_cfg wdt_config = {
		.window.min = 0,
		.window.max = (3 * 60 + 40) * MSEC_PER_SEC,
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};

	ret = wdt_install_timeout(wdt, &wdt_config);
	if (ret < 0) {
		LOG_ERR("watchdog install error");
	}

	return ret;
}

static int watchdog_init(const struct device *wdt,
			 int *main_channel_id, int *mqtt_channel_id)
{
	int ret;

	if (!device_is_ready(wdt)) {
		LOG_ERR("%s: device not ready", wdt->name);
		return -ENODEV;
	}

	ret = watchdog_new_channel(wdt);
	if (ret < 0) {
		LOG_ERR("Could not create a new watchdog channel");
		return ret;
	}

	*main_channel_id = ret;
	LOG_INF("main watchdog channel id: %d", *main_channel_id);

	ret = watchdog_new_channel(wdt);
	if (ret < 0) {
		LOG_ERR("Could not create a new watchdog channel");
		return ret;
	}

	*mqtt_channel_id = ret;
	LOG_INF("mqtt watchdog channel id: %d", *mqtt_channel_id);

	ret = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret < 0) {
		LOG_ERR("watchdog setup error");
		return 0;
	}

	LOG_INF("🐶 watchdog started!");

	return 0;
}

int main(void)
{
	int ret;
	int main_wdt_chan_id = -1, mqtt_wdt_chan_id = -1;

	temphum24_t temphum24;
	hvac_t hvac;

	char hdc302x_serial_string[128];
	char scd4x_serial_string[128];
	char sps30_serial_string[HVAC_SPS30_MAX_SERIAL_LEN];

	char hdc302x_temp_unique_id_string[128];
	char hdc302x_hum_unique_id_string[128];
	char scd4x_co2_unique_id_string[128];
	char sps30_pm25_unique_id_string[128];

	struct ha_sensor temperature_sensor = {
		.name = "Temperature",
		.unique_id = hdc302x_temp_unique_id_string,
		.device_class = "temperature",
		.state_class = "measurement",
		.unit_of_measurement = "°C",
		.suggested_display_precision = 2,
	};

	struct ha_sensor humidity_sensor = {
		.name = "Humidity",
		.unique_id = hdc302x_hum_unique_id_string,
		.device_class = "humidity",
		.state_class = "measurement",
		.unit_of_measurement = "%",
		.suggested_display_precision = 1,
	};

	struct ha_sensor co2_sensor = {
		.name = "CO₂",
		.unique_id = scd4x_co2_unique_id_string,
		.device_class = "carbon_dioxide",
		.state_class = "measurement",
		.unit_of_measurement = "ppm",
		.suggested_display_precision = 0,
	};

	const struct device *wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));

	watchdog_init(wdt, &main_wdt_chan_id, &mqtt_wdt_chan_id);

	LOG_INF("\n\n🚀 MAIN START 🚀\n");

	show_reset_cause();
	clear_reset_cause();

	openthread_enable_ready_flag();

	ret = temphum24_click_init(&temphum24);
	if (ret < 0) {
		LOG_ERR("Could not initialize temphum24 click");
		return ret;
	}

	ret = hvac_click_init(&hvac);
	if (ret < 0) {
		LOG_ERR("Could not initialize hvac click");
		return ret;
	}

	LOG_INF("Version: %s", APP_VERSION_FULL);

	ret = get_hdc302x_serial_as_string(&temphum24,
					   hdc302x_serial_string,
					   sizeof(hdc302x_serial_string));
	if (ret < 0) {
		LOG_ERR("Could not get hdc302x serial number string");
		return ret;
	}

	ret = get_scd4x_serial_as_string(&hvac, scd4x_serial_string,
					 sizeof(scd4x_serial_string));
	if (ret < 0) {
		LOG_ERR("Could not get scd4x serial number string");
		return ret;
	}

	// ret = get_sps30_serial_as_string(&hvac,
	// 				 sps30_serial_string,
	// 				 sizeof(sps30_serial_string));
	// if (ret < 0) {
	// 	LOG_ERR("Could not get sps30 serial number string");
	// 	return ret;
	// }

	ret = generate_unique_id(hdc302x_temp_unique_id_string,
				 sizeof(hdc302x_temp_unique_id_string),
				 "hdc302x", "temp",
				 hdc302x_serial_string);
	if (ret < 0) {
		LOG_ERR("Could not generate hdc302x temperature unique id");
		return ret;
	}

	ret = generate_unique_id(hdc302x_hum_unique_id_string,
				 sizeof(hdc302x_hum_unique_id_string),
				 "hdc302x", "hum",
				 hdc302x_serial_string);
	if (ret < 0) {
		LOG_ERR("Could not generate hdc302x humidity unique id");
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

	// ret = generate_unique_id(sps30_pm25_unique_id_string,
	// 			 sizeof(sps30_pm25_unique_id_string),
	// 			 "sps30", "pm25",
	// 			 sps30_serial_string);
	// if (ret < 0) {
	// 	LOG_ERR("Could not generate sps30 unique id");
	// 	return ret;
	// }

	while (!openthread_is_ready())
		k_sleep(K_MSEC(100));
	k_sleep(K_MSEC(100)); // Something else is not ready, not sure what

	mqtt_watchdog_init(wdt, mqtt_wdt_chan_id);

	ha_start();
	ha_register_sensor(&temperature_sensor);
	ha_register_sensor(&humidity_sensor);
	ha_register_sensor(&co2_sensor);

	// 1 measurement per second
	ret = temphum24_default_cfg(&temphum24);
	if (ret < 0) {
		LOG_ERR("Could not start hdc302x");
		return ret;
	}
	// 1 measurement every 5 seconds
	hvac_scd40_send_cmd(&hvac, HVAC_START_PERIODIC_MEASUREMENT);
	// New readings are available every second
	// hvac_sps30_start_measurement (&hvac);

	LOG_INF("💤 waiting for all sensors to be ready");
	k_sleep(K_SECONDS(SEDONDS_IN_BETWEEN_SENSOR_READING));

	// We set the device online after 10 seconds so HA gets time to
	// process the sensor registrations first.
	ha_set_online();

	LOG_INF("🎉 init done 🎉");

	float temperature, humidity;

	measuremen_data_t hvac_data;
	mass_and_num_cnt_data_t sps30_data;

	int number_of_readings = NUMBER_OF_READINGS_IN_AVERAGE;

	bool non_fatal_error = false;

	while (1) {
		ret = temphum24_read_temp_and_rh(&temphum24,
						 &temperature, &humidity);
		if (ret < 0) {
			LOG_WRN("Could not read temperture and humidity");
			non_fatal_error = true;
		}
		else {
			LOG_INF("HDC302x");
			LOG_INF("├── Temperature: %.2f°C", temperature);
			LOG_INF("└── Humidity: %.1f%%", humidity);

			ha_add_sensor_reading(&temperature_sensor, temperature);
			ha_add_sensor_reading(&humidity_sensor, humidity);
		}

		ret = hvac_scd40_read_measurement(&hvac, &hvac_data);
		if (ret < 0) {
			LOG_WRN("Could not read hvac module");
			non_fatal_error = true;
		}
		else {
			LOG_INF("SCD4x");
			LOG_INF("├── CO2 Concentration = %d ppm", hvac_data.co2_concent);
			LOG_INF("├── Temperature = %.2f°C", hvac_data.temperature);
			LOG_INF("└── R. Humidity = %.2f%%", hvac_data.r_humidity);

			ha_add_sensor_reading(&co2_sensor, hvac_data.co2_concent);
		}

		// hvac_sps30_read_measured_data(&hvac, &sps30_data);

		// LOG_INF("SPS30");
		// LOG_INF("├── Mass concentration");
		// LOG_INF("│   ├── PM 1.0 = %.2f μg/m³", sps30_data.mass_pm_1_0);
		// LOG_INF("│   ├── PM 2.5 = %.2f μg/m³", sps30_data.mass_pm_2_5);
		// LOG_INF("│   ├── PM 4.0 = %.2f μg/m³", sps30_data.mass_pm_4_0);
		// LOG_INF("│   └── PM 10  = %.2f μg/m³", sps30_data.mass_pm_10);

		// LOG_INF("└── Number Concentration");
		// LOG_INF("    ├── PM 0.5 = %.2f n/cm³", sps30_data.num_pm_0_5);
		// LOG_INF("    ├── PM 1.0 = %.2f n/cm³", sps30_data.num_pm_1_0);
		// LOG_INF("    ├── PM 2.5 = %.2f n/cm³", sps30_data.num_pm_2_5);
		// LOG_INF("    ├── PM 4.0 = %.2f n/cm³", sps30_data.num_pm_4_0);
		// LOG_INF("    └── PM 10  = %.2f n/cm³", sps30_data.num_pm_10);

		number_of_readings += 1;
		if (number_of_readings >= NUMBER_OF_READINGS_IN_AVERAGE) {
			ret = ha_send_sensor_value(&temperature_sensor);
			if (ret < 0) {
				LOG_WRN("Could not send temperature");
				non_fatal_error = true;
			}

			ret = ha_send_sensor_value(&humidity_sensor);
			if (ret < 0) {
				LOG_WRN("Could not send humidity");
				non_fatal_error = true;
			}

			ret = ha_send_sensor_value(&co2_sensor);
			if (ret < 0) {
				LOG_WRN("Could not send CO2");
				non_fatal_error = true;
			}

			number_of_readings = 0;
		}

		// It's non-fatal but the watchdog will take action if it
		// keeps happening.
		if (!non_fatal_error) {
			wdt_feed(wdt, main_wdt_chan_id);
		}
		non_fatal_error = false;

		LOG_INF("💤 end of main loop");
		k_sleep(K_SECONDS(SEDONDS_IN_BETWEEN_SENSOR_READING));
	}

	return 0;
}
