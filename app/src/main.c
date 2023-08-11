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
#include "init.h"
#include "mqtt.h"
#include "openthread.h"
#include "reset.h"
#include "temphum24.h"
#include "uid.h"


#define SEDONDS_IN_BETWEEN_SENSOR_READING	10
#define NUMBER_OF_READINGS_IN_AVERAGE		6

#define RETRY_DELAY_SECONDS			10


static void register_sensor_retry(struct ha_sensor *sensor)
{
	int ret;

retry:
	ret = ha_register_sensor(sensor);
	if (ret < 0) {
		LOG_WRN("Could not register sensor, retrying");
		k_sleep(K_SECONDS(RETRY_DELAY_SECONDS));
		goto retry;
	}
}

static void send_bianry_sensor_retry(struct ha_sensor *sensor)
{
	int ret;

retry:
	ret = ha_send_binary_sensor_state(sensor);
	if (ret < 0) {
		LOG_WRN("Could not send binary sensor, retrying");
		k_sleep(K_SECONDS(RETRY_DELAY_SECONDS));
		goto retry;
	}
}

static void set_online_retry(void)
{
	int ret;

retry:
	ret = ha_set_online();
	if (ret < 0) {
		LOG_WRN("Could not set online, retrying");
		k_sleep(K_SECONDS(RETRY_DELAY_SECONDS));
		goto retry;
	}
}

static int send_sensor_values(void)
{
	int ret;
	bool non_fatal_error = false;

	ret = ha_send_sensor_value(&temperature_sensor);
	if (ret < 0) {
		LOG_WRN("⚠️ could not send temperature");
		non_fatal_error = true;
	}

	ret = ha_send_sensor_value(&humidity_sensor);
	if (ret < 0) {
		LOG_WRN("⚠️ could not send humidity");
		non_fatal_error = true;
	}

	ret = ha_send_sensor_value(&co2_sensor);
	if (ret < 0) {
		LOG_WRN("⚠️ could not send CO2");
		non_fatal_error = true;
	}

	if (non_fatal_error) {
		return -1;
	}

	return 0;
}

int main(void)
{
	int ret;
	int main_wdt_chan_id = -1, mqtt_wdt_chan_id = -1;
	uint32_t reset_cause;

	temphum24_t temphum24;
	hvac_t hvac;

	struct ha_sensor watchdog_triggered_sensor = {
		.name = "Watchdog",
		.device_class = "problem",
	};

	struct ha_sensor temperature_sensor = {
		.name = "Temperature",
		.device_class = "temperature",
		.state_class = "measurement",
		.unit_of_measurement = "°C",
		.suggested_display_precision = 2,
	};

	struct ha_sensor humidity_sensor = {
		.name = "Humidity",
		.device_class = "humidity",
		.state_class = "measurement",
		.unit_of_measurement = "%",
		.suggested_display_precision = 1,
	};

	struct ha_sensor co2_sensor = {
		.name = "CO₂",
		.device_class = "carbon_dioxide",
		.state_class = "measurement",
		.unit_of_measurement = "ppm",
		.suggested_display_precision = 0,
	};

	const struct device *wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));

	init_watchdog(wdt, &main_wdt_chan_id, &mqtt_wdt_chan_id);

	LOG_INF("\n\n🚀 MAIN START 🚀\n");

	reset_cause = show_reset_cause();
	clear_reset_cause();

	openthread_enable_ready_flag();
	ret = openthread_my_start();
	if (ret < 0) {
		LOG_ERR("Could not start openthread");
		return ret;
	}

	ret = init_temphum24_click(&temphum24);
	if (ret < 0) {
		LOG_ERR("Could not initialize temphum24 click");
		return ret;
	}

	ret = init_hvac_click(&hvac);
	if (ret < 0) {
		LOG_ERR("Could not initialize hvac click");
		return ret;
	}

	LOG_INF("Version: %s", APP_VERSION_FULL);

	ret = uid_init(&temphum24, &hvac);
	if (ret < 0) {
		LOG_ERR("Could not init uid module");
		return ret;
	}

	ret = uid_fill_unique_ids(&watchdog_triggered_sensor,
				  &temperature_sensor,
				  &humidity_sensor,
				  &co2_sensor);
	if (ret < 0) {
		LOG_ERR("Could fill unique ids");
		return ret;
	}

	LOG_INF("💤 waiting for openthread to be ready");
	openthread_wait_for_ready();

	mqtt_watchdog_init(wdt, mqtt_wdt_chan_id);
	ha_start(uid_get_device_id());

	ha_init_binary_sensor(&watchdog_triggered_sensor);
	ha_init_sensor(&temperature_sensor);
	ha_init_sensor(&humidity_sensor);
	ha_init_sensor(&co2_sensor);

	register_sensor_retry(&watchdog_triggered_sensor);
	register_sensor_retry(&temperature_sensor);
	register_sensor_retry(&humidity_sensor);
	register_sensor_retry(&co2_sensor);

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

	// We set the device online a little after sensor registrations
	// so HA gets time to process the sensor registrations first.
	set_online_retry();

	ha_set_binary_sensor_state(&watchdog_triggered_sensor,
				   is_reset_cause_watchdog(reset_cause));
	send_bianry_sensor_retry(&watchdog_triggered_sensor);

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
			non_fatal_error |= send_sensor_values();
			number_of_readings = 0;
		}

		// It's non-fatal but the watchdog will take action if it
		// keeps happening.
		if (!non_fatal_error) {
			LOG_INF("🦴 feed watchdog");
			wdt_feed(wdt, main_wdt_chan_id);
		}
		non_fatal_error = false;

		LOG_INF("💤 end of main loop");
		k_sleep(K_SECONDS(SEDONDS_IN_BETWEEN_SENSOR_READING));
	}

	return 0;
}
