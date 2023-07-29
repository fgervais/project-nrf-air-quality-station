#include <zephyr/drivers/i2c.h>

#include "drv_i2c_master.h"


void i2c_master_configure_default(i2c_master_config_t *config)
{
	config->scl = -1;
	config->sda = -1;
}

err_t i2c_master_open(i2c_master_t *obj, i2c_master_config_t *config)
{
	return I2C_MASTER_SUCCESS;
}

err_t i2c_master_set_speed(i2c_master_t *obj, uint32_t speed)
{
	return I2C_MASTER_SUCCESS;
}

err_t i2c_master_set_slave_address(i2c_master_t *obj, uint8_t address)
{
	obj->config.addr = address;
	return I2C_MASTER_SUCCESS;
}

err_t i2c_master_write(i2c_master_t *obj,
		       uint8_t *write_data_buf,
		       size_t len_write_data)
{
	return i2c_write(obj->dev, write_data_buf,
			 len_write_data, obj->config.addr);
}

err_t i2c_master_write_then_read(i2c_master_t *obj,
				 uint8_t *write_data_buf,
				 size_t len_write_data,
				 uint8_t *read_data_buf,
				 size_t len_read_data)
{
	return i2c_write_read(obj->dev,
			      obj->config.addr,
			      write_data_buf,
			      len_write_data,
			      read_data_buf,
			      len_read_data);
}

err_t i2c_master_read(i2c_master_t *obj,
		      uint8_t *read_data_buf,
		      size_t len_read_data)
{
	return i2c_read(obj->dev,
			read_data_buf,
			len_read_data,
			obj->config.addr);
}