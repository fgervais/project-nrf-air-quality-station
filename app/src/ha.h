#ifndef HA_H_
#define HA_H_

struct ha_sensor {
	const char *name;
	const char *unique_id;
	const char *device_class;
	const char *state_class;

	char brief_state_topic[128];
	char full_state_topic[128];
};

int ha_start();
int ha_register_sensor(struct ha_sensor *);
// int ha_send_current_temp(double current_temp);
// int ha_send_current_state(bool enabled);

#endif /* HA_H_ */