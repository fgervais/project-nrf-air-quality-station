#ifndef HA_H_
#define HA_H_

struct ha_sensor {
	const char *name;
	const char *unique_id;
	const char *device_class;
	const char *state_class;
};

int ha_start(char *scd4x_serial_number, char *sps30_serial_number);
// int ha_send_current_temp(double current_temp);
// int ha_send_current_state(bool enabled);

#endif /* HA_H_ */