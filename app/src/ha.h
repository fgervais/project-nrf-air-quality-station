#ifndef HA_H_
#define HA_H_

#define HA_TOPIC_BUFFER_SIZE		128

struct ha_sensor {
	// Set by user
	const char *name;
	const char *unique_id;
	const char *device_class;
	const char *state_class;
	const char *unit_of_measurement;
	int suggested_display_precision;

	// Internal use
	char full_state_topic[HA_TOPIC_BUFFER_SIZE];
};

int ha_start();
int ha_register_sensor(struct ha_sensor *);
int ha_send_value(struct ha_sensor *, double value);
// int ha_send_current_temp(double current_temp);
// int ha_send_current_state(bool enabled);

#endif /* HA_H_ */