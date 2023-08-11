#ifndef UID_H_
#define UID_H_

#include "hvac.h"
#include "temphum24.h"

#define UID_UNIQUE_ID_STRING_SIZE	32

int uid_init(temphum24_t *temphum24, hvac_t *hvac);
char * uid_get_device_id(void);
char * uid_get_hdc302x_serial(void);
char * uid_get_scd4x_serial(void);
char * uid_get_sps30_serial(void);
int uid_generate_unique_id(char *uid_buf, size_t uid_buf_size,
			   const char *part_number,
			   const char *sensor_name,
			   const char *serial_number);

#endif /* UID_H_ */