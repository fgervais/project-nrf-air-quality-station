#ifndef HA_H_
#define HA_H_

#define HA_TOPIC_BUFFER_SIZE		128

struct ha_sensor {
	// Set by user
	const char *component;
	const char *name;
	const char *unique_id;
	const char *device_class;
	const char *state_class;
	const char *unit_of_measurement;
	int suggested_display_precision;

	double total_value;
	int number_of_values;

	// Internal use
	char full_state_topic[HA_TOPIC_BUFFER_SIZE];
};

int ha_start(const char *device_id);
int ha_set_online();
int ha_init_sensor(struct ha_sensor *);
int ha_add_sensor_reading(struct ha_sensor *, double value);
int ha_add_sensor_reading_bool(struct ha_sensor *, bool value);
int ha_register_sensor(struct ha_sensor *);
int ha_send_sensor_value(struct ha_sensor *);
int ha_send_sensor_value_bool(struct ha_sensor *);

#endif /* HA_H_ */