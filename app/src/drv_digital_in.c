#include "drv_digital_in.h"


err_t digital_in_init( digital_in_t *in, pin_name_t name )
{
	in->pin = name;
	return gpio_pin_configure(in->port, in->pin, GPIO_INPUT);
}

uint8_t digital_in_read ( digital_in_t *in )
{
	return gpio_pin_get(in->port, in->pin);
}
