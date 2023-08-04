#ifndef DRV_NAME_H_
#define DRV_NAME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>


typedef	int32_t	err_t;
typedef	gpio_pin_t pin_name_t;

static inline void Delay_100ms ( void ) {
	k_sleep( K_MSEC( 1 ) );
}

static inline void Delay_1sec ( void ) {
	k_sleep( K_SECONDS( 1 ) );
}

#ifdef __cplusplus
}
#endif

#endif /* DRV_NAME_H_ */