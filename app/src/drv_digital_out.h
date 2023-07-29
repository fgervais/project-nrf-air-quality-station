#ifndef DRV_DIGITAL_OUT_H_
#define DRV_DIGITAL_OUT_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#include "drv_name.h"

#ifdef __cplusplus
extern "C" {
#endif

// https://github.com/MikroElektronika/mikrosdk_v2/blob/076ab1c6ce8d141ecd5aedfcba20d84338c3c59b/drv/lib/include/drv_digital_out.h#L61
typedef enum
{
	DIGITAL_OUT_SUCCESS = 0,	   /*!< Success. */
	DIGITAL_OUT_UNSUPPORTED_PIN = (-1) /*!< Error. */
} digital_out_err_t;

typedef struct
{
	const struct device *dev;
} digital_out_t;

err_t digital_out_init ( digital_out_t *out, pin_name_t name );
err_t digital_out_high ( digital_out_t *out );
err_t digital_out_low ( digital_out_t *out );

#ifdef __cplusplus
}
#endif

#endif /* DRV_DIGITAL_OUT_H_ */