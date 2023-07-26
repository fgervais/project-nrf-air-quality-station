#ifndef HA_H_
#define HA_H_

struct ha_sensor {
	// Set by user
	const char *name;
	const char *unique_id;
	const char *device_class;
	const char *state_class;

	// Set internally
	const char *object_id;

	// Internal use
	char brief_state_topic[128];
	char full_state_topic[128];
};

int ha_start();
int ha_register_sensor(struct ha_sensor *);
int ha_send_value(struct ha_sensor *, double value);
// int ha_send_current_temp(double current_temp);
// int ha_send_current_state(bool enabled);

#endif /* HA_H_ */