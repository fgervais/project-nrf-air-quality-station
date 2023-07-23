#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(home_assistant, LOG_LEVEL_DBG);

#include <zephyr/data/json.h>
#include <zephyr/drivers/hwinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <app_version.h>

#include "mqtt.h"


#define DEVICE_ID_BYTE_SIZE		8
#define TOPIC_BUFFER_SIZE		128
#define JSON_CONFIG_BUFFER_SIZE		1024
#define UNIQUE_ID_BUFFER_SIZE		64

#define MQTT_BASE_PATH_FORMAT_STRING "home/room/kitchen/air_quality/%s"
#define LAST_WILL_TOPIC_FORMAT_STRING MQTT_BASE_PATH_FORMAT_STRING "/available"
#define DISCOVERY_TOPIC_FORMAT_STRING	"homeassistant/sensor/%s/config"

#define AIR_QUALITY_DEVICE {			\
	.identifiers = device_id_hex_string,	\
	.name = "Air Quality Monitor",		\
	.sw_version = APP_VERSION_FULL,		\
	.hw_version = "rev1",			\
	.model = "Gold",			\
	.manufacturer = "François Gervais",	\
}

struct device {
	const char *identifiers;
	const char *name;
	const char *sw_version;
	const char *hw_version;
	const char *model;
	const char *manufacturer;
};

struct config {
	const char *base_path;
	const char *name;
	const char *unique_id;
	const char *device_class;
	const char *state_class;
	const char *availability_topic;
	const char *state_topic;
	struct device dev;
};


static char device_id_hex_string[DEVICE_ID_BYTE_SIZE * 2 + 1];
static char mqtt_base_path[TOPIC_BUFFER_SIZE];
// static char *scd4x_sn;
// static char *sps30_sn;

static char unique_id_co2[UNIQUE_ID_BUFFER_SIZE];
static char unique_id_pm25[UNIQUE_ID_BUFFER_SIZE];

static char last_will_topic[TOPIC_BUFFER_SIZE];
static const char *last_will_message = "offline";


// static void callback_sub_set_mode(const char *payload);
// static void callback_sub_set_temperature(const char *payload);


static struct config co2_config = {
	.base_path = mqtt_base_path,
	.name = "Air Quality Monitor",
	.unique_id = unique_id_co2,
	.device_class = "carbon_dioxide",
	.state_class = "measurement",
	.availability_topic = "~/available",
	.state_topic = "~/sensor/co2/state",
	.dev = AIR_QUALITY_DEVICE,
};

// static struct config pm25_config = {
// 	.base_path = mqtt_base_path,
// 	.name = "Air Quality Monitor",
// 	.unique_id = unique_id_pm25,
// 	.device_class = "pm25",
// 	.state_class = "measurement",
// 	.availability_topic = "~/available",
// 	.state_topic = "~/sensor/pm25/state",
// 	.dev = AIR_QUALITY_DEVICE,
// };

// static const struct mqtt_subscription subs[] = {
// 	{
// 		.topic = "home/room/kitchen/air_quality/monitor/mode/set",
// 		.callback = callback_sub_set_mode,
// 	},
// 	{
// 		.topic = "home/room/kitchen/air_quality/monitor/temperature/set",
// 		.callback = callback_sub_set_temperature,
// 	},
// };

static const struct json_obj_descr device_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct device, identifiers,	 JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct device, name,	 JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct device, sw_version,	 JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct device, hw_version,	 JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct device, model,	 JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct device, manufacturer, JSON_TOK_STRING),
};

static const struct json_obj_descr config_descr[] = {
	JSON_OBJ_DESCR_PRIM_NAMED(struct config, "~", base_path,	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct config, name,			JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct config, unique_id,			JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct config, device_class,		JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct config, state_class,			JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct config, availability_topic,		JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct config, state_topic,			JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct config, dev, device_descr),
};

// static void (*mode_change_callback)(const char *mode) = NULL;
// static void (*temperature_setpoint_change_callback)(double setpoint) = NULL;

// static void callback_sub_set_mode(const char *payload)
// {
// 	LOG_INF("⚡ I've been called back: %s", payload);

// 	if (mode_change_callback) {
// 		mode_change_callback(payload);
// 	}
// }

// static void callback_sub_set_temperature(const char *payload)
// {
// 	double temperature;

// 	LOG_INF("⚡ I've been called back: %s", payload);

// 	if (temperature_setpoint_change_callback) {
// 		temperature = atof(payload);
// 		temperature_setpoint_change_callback(temperature);
// 	}
// }

static int get_device_id_string(char *id_string, size_t id_string_len)
{
	uint8_t dev_id[DEVICE_ID_BYTE_SIZE];
	ssize_t length;

	length = hwinfo_get_device_id(dev_id, sizeof(dev_id));

	if (length == -ENOTSUP) {
		LOG_ERR("Not supported by hardware");
		return -ENOTSUP;
	} else if (length < 0) {
		LOG_ERR("Error: %zd", length);
		return length;
	}

	bin2hex(dev_id, ARRAY_SIZE(dev_id), id_string, id_string_len);

	return 0;
}

// static int ha_subscribe_to_topics(void)
// {
// 	mqtt_subscribe_to_topic(subs, ARRAY_SIZE(subs));

// 	return 0;
// }

// <discovery_prefix>/<component>/[<node_id>/]<object_id>/config
//
// Best practice for entities with a unique_id is to set <object_id> to
// unique_id and omit the <node_id>.
// https://www.home-assistant.io/integrations/mqtt/#discovery-topic


static int ha_send_discovery(void)
{
	int ret;
	char json_config[JSON_CONFIG_BUFFER_SIZE];
	char discovery_topic[TOPIC_BUFFER_SIZE];


	// Add a function to add other sensors

	snprintf(discovery_topic, sizeof(discovery_topic),
		 DISCOVERY_TOPIC_FORMAT_STRING, co2_config.unique_id);

	LOG_DBG("discovery topic: %s", discovery_topic);

	ret = json_obj_encode_buf(config_descr, ARRAY_SIZE(config_descr),
				  &co2_config, json_config, sizeof(json_config));
	if (ret < 0) {
		LOG_ERR("Could not encode JSON (%d)", ret);
		return ret;
	}

	LOG_DBG("payload: %s", json_config);

	mqtt_publish_to_topic(discovery_topic, json_config, true);

	return 0;
}

// int ha_send_current_temp(double current_temp)
// {
// 	char topic[strlen(ac_config.base_path)
// 		   + strlen(ac_config.current_temperature_topic)];
// 	char temp_string[16];

// 	snprintf(topic, sizeof(topic),
// 		 "%s%s",
// 		 ac_config.base_path,
// 		 ac_config.current_temperature_topic + 1);

// 	snprintf(temp_string, sizeof(temp_string),
// 		 "%g",
// 		 current_temp);

// 	mqtt_publish_to_topic(topic, temp_string, false);

// 	return 0;
// }

// int ha_send_current_state(bool enabled)
// {
// 	char topic[strlen(ac_config.base_path)
// 		   + strlen(ac_config.availability_topic)];
	
// 	snprintf(topic, sizeof(topic),
// 		 "%s%s",
// 		 ac_config.base_path,
// 		 ac_config.availability_topic + 1);

// 	mqtt_publish_to_topic(topic, enabled ? "online" : "offline", true);

// 	return 0;
// }

int ha_start(char *scd4x_serial_number, char *sps30_serial_number)
{
	int ret;

	// mode_change_callback = mode_change_cb;
	// temperature_setpoint_change_callback = temperature_setpoint_change_cb;

	ret = get_device_id_string(
		device_id_hex_string,
		ARRAY_SIZE(device_id_hex_string));
	if (ret < 0) {
		LOG_ERR("Could not get device ID");
		return ret;
	}

	LOG_INF("Device ID: %s", co2_config.dev.identifiers);
	LOG_INF("Version: %s", co2_config.dev.sw_version);

	ret = snprintf(mqtt_base_path, sizeof(mqtt_base_path),
		 MQTT_BASE_PATH_FORMAT_STRING, device_id_hex_string);
	if (ret < 0 && ret >= sizeof(mqtt_base_path)) {
		LOG_ERR("Could not set mqtt_base_path");
		return -ENOMEM;
	}



	// Wrap this in a function?

	ret = snprintf(unique_id_co2, sizeof(unique_id_co2),
		 "scd4x_%s_co2", scd4x_serial_number);
	if (ret < 0 && ret >= sizeof(unique_id_co2)) {
		LOG_ERR("Could not set unique_id_co2");
		return -ENOMEM;
	}

	ret = snprintf(unique_id_pm25, sizeof(unique_id_pm25),
		 "sps30_%s_pm25", sps30_serial_number);
	if (ret < 0 && ret >= sizeof(unique_id_pm25)) {
		LOG_ERR("Could not set unique_id_pm25");
		return -ENOMEM;
	}

	// --------------------

	ret = snprintf(last_will_topic, sizeof(last_will_topic),
		 LAST_WILL_TOPIC_FORMAT_STRING, device_id_hex_string);
	if (ret < 0 && ret >= sizeof(last_will_topic)) {
		LOG_ERR("Could not set last_will_topic");
		return -ENOMEM;
	}

	ret = mqtt_init(device_id_hex_string, last_will_topic, last_will_message);
	if (ret < 0) {
		LOG_ERR("could initialize MQTT");
		return ret;
	}

	LOG_INF("✏️  send discovery");
	ha_send_discovery();
	// LOG_INF("✏️  subscribe to topics");
	// ha_subscribe_to_topics();

	return 0;
}
