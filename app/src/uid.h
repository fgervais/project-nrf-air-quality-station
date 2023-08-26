#ifndef UID_H_
#define UID_H_

#include "hvac.h"
#include "temphum24.h"

#define UID_UNIQUE_ID_STRING_SIZE	32

struct ha_sensor;

char * uid_get_device_id(void);
char * uid_get_hdc302x_serial(void);
char * uid_get_scd4x_serial(void);
char * uid_get_sps30_serial(void);

int uid_init(temphum24_t *temphum24, hvac_t *hvac);
int uid_fill_unique_ids(struct ha_sensor *wdt,
			struct ha_sensor *temp,
			struct ha_sensor *hum,
			struct ha_sensor *co2);

#endif /* UID_H_ */