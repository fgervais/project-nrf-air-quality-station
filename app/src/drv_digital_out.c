#include <zephyr/drivers/gpio.h>

#include "drv_digital_out.h"


err_t digital_out_init ( digital_out_t *out, pin_name_t name )
{
	out->pin = name;
	return gpio_pin_configure(out->port, out->pin, GPIO_OUTPUT_LOW);
}

err_t digital_out_high ( digital_out_t *out )
{
	return gpio_pin_set(out->port, out->pin, 1);
}

err_t digital_out_low ( digital_out_t *out )
{
	return gpio_pin_set(out->port, out->pin, 0);
}
