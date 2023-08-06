#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(home_assistant, LOG_LEVEL_DBG);

#include <zephyr/data/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <app_version.h>

#include "ha.h"
#include "mqtt.h"


#define JSON_CONFIG_BUFFER_SIZE		1024
#define UNIQUE_ID_BUFFER_SIZE		64

#define MQTT_BASE_PATH_FORMAT_STRING "home/room/kitchen/air_quality/%s"
#define LAST_WILL_TOPIC_FORMAT_STRING MQTT_BASE_PATH_FORMAT_STRING "/available"
#define DISCOVERY_TOPIC_FORMAT_STRING	"homeassistant/sensor/%s/config"
// #define DISCOVERY_TOPIC_FORMAT_STRING	"test/sensor/%s/config"

#define AIR_QUALITY_DEVICE {			\
	.identifiers = device_id_hex_string,	\
	.name = "Air Quality Monitor",		\
	.sw_version = APP_VERSION_FULL,		\
	.hw_version = "rev1",			\
	.model = "Gold",			\
	.manufacturer = "François Gervais",	\
}

struct ha_device {
	const char *identifiers;
	const char *name;
	const char *sw_version;
	const char *hw_version;
	const char *model;
	const char *manufacturer;
};

struct ha_sensor_config {
	const char *base_path;
	const char *name;
	const char *unique_id;
	const char *object_id;
	const char *device_class;
	const char *state_class;
	const char *unit_of_measurement;
	int suggested_display_precision;
	const char *availability_topic;
	const char *state_topic;
	struct ha_device dev;
};


static const char *device_id_hex_string;
static char mqtt_base_path[HA_TOPIC_BUFFER_SIZE];

static char last_will_topic[HA_TOPIC_BUFFER_SIZE];
static const char *last_will_message = "offline";


// static void callback_sub_set_mode(const char *payload);
// static void callback_sub_set_temperature(const char *payload);


// static struct config co2_config = {
// 	.base_path = mqtt_base_path,
// 	.name = "Air Quality Monitor - CO2",
// 	.unique_id = unique_id_co2,
// 	.device_class = "carbon_dioxide",
// 	.state_class = "measurement",
// 	.availability_topic = "~/available",
// 	.state_topic = "~/sensor/co2/state",
// 	.dev = AIR_QUALITY_DEVICE,
// };

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
	JSON_OBJ_DESCR_PRIM(struct ha_device, identifiers,	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_device, name,	 	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_device, sw_version,	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_device, hw_version,	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_device, model,	 	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_device, manufacturer, 	JSON_TOK_STRING),
};

static const struct json_obj_descr config_descr[] = {
	JSON_OBJ_DESCR_PRIM_NAMED(struct ha_sensor_config, "~", base_path,	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, name,			JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, unique_id,			JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, object_id,			JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, device_class,		JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, state_class,		JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, unit_of_measurement,	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, suggested_display_precision, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, availability_topic,	JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct ha_sensor_config, state_topic,		JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct ha_sensor_config, dev, device_descr),
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


static int ha_send_discovery(struct ha_sensor_config *conf)
{
	int ret;
	char json_config[JSON_CONFIG_BUFFER_SIZE];
	char discovery_topic[HA_TOPIC_BUFFER_SIZE];

	snprintf(discovery_topic, sizeof(discovery_topic),
		 DISCOVERY_TOPIC_FORMAT_STRING, conf->unique_id);

	LOG_DBG("discovery topic: %s", discovery_topic);

	ret = json_obj_encode_buf(config_descr, ARRAY_SIZE(config_descr),
				  conf, json_config, sizeof(json_config));
	if (ret < 0) {
		LOG_ERR("Could not encode JSON (%d)", ret);
		return ret;
	}

	LOG_DBG("payload: %s", json_config);

	ret = mqtt_publish_to_topic(discovery_topic, json_config, true);
	if (ret < 0) {
		LOG_ERR("Count not publish to topic");
		return ret;
	}

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

int ha_start(const char *device_id)
{
	int ret;

	device_id_hex_string = device_id;

	ret = snprintf(mqtt_base_path, sizeof(mqtt_base_path),
		 MQTT_BASE_PATH_FORMAT_STRING, device_id_hex_string);
	if (ret < 0 && ret >= sizeof(mqtt_base_path)) {
		LOG_ERR("Could not set mqtt_base_path");
		return -ENOMEM;
	}



	// Wrap this in a function?

	// ret = snprintf(unique_id_co2, sizeof(unique_id_co2),
	// 	 "scd4x_%s_co2", scd4x_serial_number);
	// if (ret < 0 && ret >= sizeof(unique_id_co2)) {
	// 	LOG_ERR("Could not set unique_id_co2");
	// 	return -ENOMEM;
	// }

	// ret = snprintf(unique_id_pm25, sizeof(unique_id_pm25),
	// 	 "sps30_%s_pm25", sps30_serial_number);
	// if (ret < 0 && ret >= sizeof(unique_id_pm25)) {
	// 	LOG_ERR("Could not set unique_id_pm25");
	// 	return -ENOMEM;
	// }

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

	// ha_send_discovery();
	// LOG_INF("✏️  subscribe to topics");
	// ha_subscribe_to_topics();

	return 0;
}

int ha_set_online()
{
	int ret;

	ret = mqtt_publish_to_topic(last_will_topic, "online", false);
	if (ret < 0) {
		LOG_ERR("Count not publish to topic");
		return ret;
	}

	return 0;
}

int ha_init_sensor(struct ha_sensor *sensor)
{
	sensor->total_value = 0;
	sensor->number_of_values = 0;

	return 0;
}

int ha_add_sensor_reading(struct ha_sensor *sensor, double value)
{
	sensor->total_value += value;
	sensor->number_of_values += 1;

	return 0;
}

// `object_id` = `unique id`
//
// `object_id` is set to `unique id` in order to maintain full `name` flexibility
// as by default, the `entity_id` is generated from the `name` if defined and
// `entity_id` has strict character requirements.
//
// Setting the `object_id` allows HA to use it instead of the name to
// generate the `entity_id` thus allowing name to use characters unallowed
// in an `entity_id`.
//
// https://github.com/home-assistant/core/issues/4628#:~:text=Description%20of%20problem%3A%20In%20case%20device%20has%20no%20English%20characters%20in%20the%20name%20HA%20will%20generate%20an%20empty%20entity_id%20and%20it%20will%20not%20be%20possible%20to%20access%20the%20device%20from%20HA.
// 
// object_id: Used instead of name for automatic generation of entity_id
//   https://www.home-assistant.io/integrations/sensor.mqtt/#object_id
//
// Best practice for entities with a unique_id is to set <object_id> to unique_id
//   https://www.home-assistant.io/integrations/mqtt/#discovery-messages
//
// MQTT sensor example:
// https://community.home-assistant.io/t/unique-id-with-mqtt-sensor-not-working/564315/8
//
// Other usefull links:
// https://community.home-assistant.io/t/unique-id-and-object-id-are-being-ignored-in-my-mqtt-sensor/397368/14
// https://community.home-assistant.io/t/wth-are-there-unique-id-and-entity-id/467623/9
int ha_register_sensor(struct ha_sensor *sensor)
{
	int ret;
	char brief_state_topic[HA_TOPIC_BUFFER_SIZE];
	struct ha_sensor_config ha_sensor_config = {
		.base_path = mqtt_base_path,
		.name = sensor->name,
		.unique_id = sensor->unique_id,
		.object_id = sensor->unique_id,
		.device_class = sensor->device_class,
		.state_class = sensor->state_class,
		.unit_of_measurement = sensor->unit_of_measurement,
		.suggested_display_precision = sensor->suggested_display_precision,
		.availability_topic = "~/available",
		.state_topic = brief_state_topic,
		.dev = AIR_QUALITY_DEVICE,
	};

	LOG_INF("📝 registering sensor: %s", sensor->unique_id);

	ret = snprintf(brief_state_topic, sizeof(brief_state_topic),
		       "~/sensor/%s/state", sensor->unique_id);
	if (ret < 0 && ret >= sizeof(brief_state_topic)) {
		LOG_ERR("Could not set brief_state_topic");
		return -ENOMEM;
	}

	ret = snprintf(sensor->full_state_topic, sizeof(sensor->full_state_topic),
		 "%s%s",
		 mqtt_base_path,
		 brief_state_topic + 1);
	if (ret < 0 && ret >= sizeof(brief_state_topic)) {
		LOG_ERR("Could not set full_state_topic");
		return -ENOMEM;
	}

	LOG_INF("📖 send discovery");
	ret = ha_send_discovery(&ha_sensor_config);
	if (ret < 0) {
		LOG_ERR("Could not send discovery");
		return ret;
	}

	return 0;
}

int ha_send_sensor_value(struct ha_sensor *sensor)
{
	int ret;
	char value_string[16];

	if (sensor->number_of_values == 0) {
		goto out;
	}

	ret = snprintf(value_string, sizeof(value_string),
		       "%g",
		       sensor->total_value / sensor->number_of_values);
	if (ret < 0 && ret >= sizeof(value_string)) {
		LOG_ERR("Could not set value_string");
		return -ENOMEM;
	}

	ret = mqtt_publish_to_topic(sensor->full_state_topic, value_string, false);
	if (ret < 0) {
		LOG_ERR("Count not publish to topic");
		return ret;
	}

	sensor->total_value = 0;
	sensor->number_of_values = 0;

out:
	return 0;
}
