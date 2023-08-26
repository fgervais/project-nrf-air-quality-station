#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
// There is an include missing in thread_analyzer.h
// Workaround by including it lower.
#include <zephyr/debug/thread_analyzer.h>

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
#define NUMBER_OF_READINGS_IN_AVERAGE		(60 / SEDONDS_IN_BETWEEN_SENSOR_READING)

#define NUMBER_OF_LOOP_RUN_ANALYSIS		(5 * 60 / SEDONDS_IN_BETWEEN_SENSOR_READING)

#define RETRY_DELAY_SECONDS			10


static struct ha_sensor watchdog_triggered_sensor = {
	.type = HA_BINARY_SENSOR_TYPE,
	.name = "Watchdog",
	.device_class = "problem",
};

static struct ha_sensor temperature_sensor = {
	.type = HA_SENSOR_TYPE,
	.name = "Temperature",
	.device_class = "temperature",
	.state_class = "measurement",
	.unit_of_measurement = "°C",
	.suggested_display_precision = 2,
};

static struct ha_sensor humidity_sensor = {
	.type = HA_SENSOR_TYPE,
	.name = "Humidity",
	.device_class = "humidity",
	.state_class = "measurement",
	.unit_of_measurement = "%",
	.suggested_display_precision = 1,
};

static struct ha_sensor co2_sensor = {
	.type = HA_SENSOR_TYPE,
	.name = "CO₂",
	.device_class = "carbon_dioxide",
	.state_class = "measurement",
	.unit_of_measurement = "ppm",
	.suggested_display_precision = 0,
};


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

static int start_sensor_measurements(temphum24_t *temphum24, hvac_t *hvac)
{
	int ret;

	// 1 measurement per second
	ret = temphum24_default_cfg(temphum24);
	if (ret < 0) {
		LOG_ERR("Could not start hdc302x");
		return ret;
	}
	// 1 measurement every 5 seconds
	hvac_scd40_send_cmd(hvac, HVAC_START_PERIODIC_MEASUREMENT);
	// New readings are available every second
	// hvac_sps30_start_measurement (&hvac);

	return 0;
}

int main(void)
{
	int ret;
	int main_wdt_chan_id = -1, mqtt_wdt_chan_id = -1;
	uint32_t reset_cause;

	const struct device *wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));

	temphum24_t temphum24;
	hvac_t hvac;

	float temperature, humidity;
	measuremen_data_t hvac_data;
	mass_and_num_cnt_data_t sps30_data;

	uint32_t main_loop_counter = 0;
	bool non_fatal_error = false;


	init_watchdog(wdt, &main_wdt_chan_id, &mqtt_wdt_chan_id);

	LOG_INF("\n\n🚀 MAIN START (%s) 🚀\n", APP_VERSION_FULL);

	reset_cause = show_reset_cause();
	clear_reset_cause();

	if (is_reset_cause_watchdog(reset_cause)
	    || is_reset_cause_button(reset_cause)) {
		ret = openthread_erase_persistent_info();
		if (ret < 0) {
			LOG_WRN("Could not erase openthread info");
		}
	}

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

	register_sensor_retry(&watchdog_triggered_sensor);
	register_sensor_retry(&temperature_sensor);
	register_sensor_retry(&humidity_sensor);
	register_sensor_retry(&co2_sensor);

	ret = start_sensor_measurements(&temphum24, &hvac);
	if (ret < 0) {
		LOG_ERR("Could not start sensor measurements");
		return ret;
	}

	LOG_INF("💤 waiting for all sensors to be ready");
	k_sleep(K_SECONDS(SEDONDS_IN_BETWEEN_SENSOR_READING));

	// We set the device online a little after sensor registrations
	// so HA gets time to process the sensor registrations first.
	set_online_retry();

	ha_set_binary_sensor_state(&watchdog_triggered_sensor,
				   is_reset_cause_watchdog(reset_cause));
	send_bianry_sensor_retry(&watchdog_triggered_sensor);

	LOG_INF("🎉 init done 🎉");

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

		if (main_loop_counter % NUMBER_OF_READINGS_IN_AVERAGE == 0) {
			non_fatal_error |= send_sensor_values();
		}

		if (main_loop_counter % NUMBER_OF_LOOP_RUN_ANALYSIS == 0) {
			thread_analyzer_print();
		}

		// Epilogue

		main_loop_counter += 1;

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
