/*
 * neopixel_config.h - copy this to the parent folder of libavr_neopixel_uart and configure
 *
 * Created: 22.03.2020
 * Author: echoromeo
 */
#ifndef NEOPIXEL_CONFIG_H_
#define NEOPIXEL_CONFIG_H_

/*
 * Needed for the instance defines
 */
#include <avr/io.h>

/*
 * Select one Neopixel type to configure the driver correctly
 */
#define NEOPIXEL_TYPE_RGB
//#define NEOPIXEL_TYPE_WWA
//#define NEOPIXEL_TYPE_RGBW

/*
 * Configure the USART instance and output pin to be used by the driver
 */
#define LED_USART			USART1
#define LED_USART_PORT	    PORTC
#define LED_DATA_PIN		PIN0_bm
#define LED_DATA_PINCTRL	PIN0CTRL

/*
 * Define one of these only if you want to move the USART output to an alternate pinout
 */
//#define LED_USART_PORT_ALT			PORTMUX_USART0_ALTERNATE_gc //tiny-style
//#define LED_USART_PORT_ALT1			PORTMUX_USART0_ALT1_gc //mega-style

#endif /* NEOPIXEL_CONFIG_H_ */
