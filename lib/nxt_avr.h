#ifndef __NXT_AVR_H__
#define __NXT_AVR_H__

#include <stdint.h>

/* Main user interface */
void nxt_avr_init(void);

void nxt_avr_set_motor(uint32_t n, int power_percent, int brake);

void nxt_avr_power_down(void);

void nxt_avr_test_loop(void);

void nxt_avr_update(void);

uint32_t buttons_get(void);

uint32_t battery_voltage(void);

uint32_t sensor_adc(uint32_t n);

void nxt_avr_set_input_power(uint32_t n, uint32_t power_type);

#endif
