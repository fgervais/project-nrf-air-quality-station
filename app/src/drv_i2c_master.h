#ifndef DRV_I2C_MASTER_H_
#define DRV_I2C_MASTER_H_

#include <zephyr/device.h>

#include "drv_name.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_PIN_NC			-1
#define I2C_MASTER_SPEED_STANDARD	0

typedef struct {
	uint8_t addr;
	pin_name_t sda;
	pin_name_t scl;
	uint32_t speed;
	uint16_t timeout_pass_count;
} i2c_master_config_t;

typedef	struct {
	const struct device *dev;
	i2c_master_config_t config;
} i2c_master_t;

// https://github.com/MikroElektronika/mikrosdk_v2/blob/076ab1c6ce8d141ecd5aedfcba20d84338c3c59b/drv/lib/include/drv_i2c_master.h#L59
typedef enum {
	I2C_MASTER_SUCCESS = 0,  /*!< Success. */
	I2C_MASTER_ERROR = (-1)  /*!< Error. */
} i2c_master_err_t;


void i2c_master_configure_default(i2c_master_config_t *config);
err_t i2c_master_open(i2c_master_t *obj, i2c_master_config_t *config);
err_t i2c_master_set_speed(i2c_master_t *obj, uint32_t speed);
err_t i2c_master_set_slave_address(i2c_master_t *obj, uint8_t address);
err_t i2c_master_write(i2c_master_t *obj,
		       uint8_t *write_data_buf,
		       size_t len_write_data);
err_t i2c_master_write_then_read(i2c_master_t *obj,
				 uint8_t *write_data_buf,
				 size_t len_write_data,
				 uint8_t *read_data_buf,
				 size_t len_read_data);
err_t i2c_master_read(i2c_master_t *obj,
		      uint8_t *read_data_buf,
		      size_t len_read_data);

#ifdef __cplusplus
}
#endif

#endif /* DRV_I2C_MASTER_H_ */