#ifndef DRV_DIGITAL_IN_H_
#define DRV_DIGITAL_IN_H_

#include <zephyr/device.h>

#include "drv_name.h"

#ifdef __cplusplus
extern "C" {
#endif

// https://github.com/MikroElektronika/mikrosdk_v2/blob/076ab1c6ce8d141ecd5aedfcba20d84338c3c59b/drv/lib/include/drv_digital_in.h#L61
typedef enum
{
	DIGITAL_IN_SUCCESS = 0,	   /*!< Success. */
	DIGITAL_IN_UNSUPPORTED_PIN = (-1) /*!< Error. */
} digital_in_err_t;

typedef struct
{
	const struct device *port;
	gpio_pin_t pin;
} digital_in_t;

err_t digital_in_init ( digital_in_t *in, pin_name_t name );
uint8_t digital_in_read ( digital_in_t *in );

#ifdef __cplusplus
}
#endif

#endif /* DRV_DIGITAL_IN_H_ */