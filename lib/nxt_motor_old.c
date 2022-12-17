#include <stddef.h>
#include "../AT91SAM7S64.h"                        /* AT91SAMT7S64 definitions */
#include "nxt_motor.h"
#include "pio_pwm.h"
#include "nxt_avr.h"

int motor_pos[3];

void nxt_motor_set_speed(uint32_t n, int speed_percent, NXT_MOTOR_BRAKE brake)
{
  if ((n < NXT_N_MOTORS) && (brake <=1)) {
    if (speed_percent > 100)
      speed_percent = 100;
    if (speed_percent < -100)
      speed_percent = -100;
    nxt_avr_set_motor(n, speed_percent, brake);
  }
}

int nxt_motor_get_pos(uint32_t n)
{
  if (n < NXT_N_MOTORS) {
	return(motor_pos[n]);
  }
  return(0);
}

/**********************************************************************************
 *  A              ----       ------       ------       ------       ---
 *                     |     |      |     |      |     |      |     |
 *                     -------      -------      -------      -------   
 *
 *  B               ------       -------------       ------       --------
 *                        |     |             |     |      |     |
 *                        -------             -------      -------
 *                        Linksrum <-  |  -> Rechtsrum
 
 * pinChanges A     00010000010000001000001000000100000100000010000010000
 * pinChanges B     00000010000010000000000000100000100000010000010000000
 * currentPins A    11100000011111110000001111111000000111111100000011111
 * currentPins B    11111100000011111111111111000000111111100000011111111
 * 
 **********************************************************************************/
#define MA0 15
#define MA1 1
#define MB0 26
#define MB1 9
#define MC0 0
#define MC1 8

#define DECODE(a,b) (((a ^ b)&0x01)?+1:-1)
void nxt_motor_isr_a(void)
{
	uint32_t currentPins = *AT91C_PIOA_PDSR;	// Read pins
	motor_pos[0]+=DECODE(((currentPins >> MA0)),((currentPins >> MA1)));
}

void nxt_motor_isr_b(void)
{
	uint32_t currentPins = *AT91C_PIOA_PDSR;	// Read pins
	motor_pos[1]+=DECODE(((currentPins >> MB0)),((currentPins >> MB1)));
}

void nxt_motor_isr_c(void)
{
	uint32_t currentPins = *AT91C_PIOA_PDSR;	// Read pins
	motor_pos[2]+=DECODE(((currentPins >> MC0)),((currentPins >> MC1)));
}

void nxt_motor_init(void)
{
	//PWM-Signale laufen über den AVR-Prozessor
	//Initialisierung nicht notwendig, da dieser zuvor 
	//bereits ininitialisiert wurde
	//nxt_avr_init()
    nxt_avr_set_motor(0,0,0);
	nxt_avr_set_motor(1,0,0);
	nxt_avr_set_motor(2,0,0);
	
	//Encoder-Signale gehen direkt auf den AT91SAM7
	motor_pos[0]=0;
	motor_pos[1]=0;
	motor_pos[2]=0;
	pio_init(4,0,PIO_CONFIG_INPUT | PIO_CONFIG_GLITCH ,PIO_CONFIG_IRQ,nxt_motor_isr_a);
	pio_init(4,1,PIO_CONFIG_INPUT | PIO_CONFIG_GLITCH ,NULL);
	pio_init(5,0,PIO_CONFIG_INPUT | PIO_CONFIG_GLITCH ,PIO_CONFIG_IRQ,nxt_motor_isr_b);
	pio_init(5,1,PIO_CONFIG_INPUT | PIO_CONFIG_GLITCH ,NULL);
	pio_init(6,0,PIO_CONFIG_INPUT | PIO_CONFIG_GLITCH ,PIO_CONFIG_IRQ,nxt_motor_isr_c);
	pio_init(6,1,PIO_CONFIG_INPUT | PIO_CONFIG_GLITCH ,NULL);
}

