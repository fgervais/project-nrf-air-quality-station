#ifndef DRV_DIGITAL_IN_H_
#define DRV_DIGITAL_IN_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

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
	const struct device *dev;
} digital_in_t;

#ifdef __cplusplus
}
#endif

#endif /* DRV_DIGITAL_IN_H_ */