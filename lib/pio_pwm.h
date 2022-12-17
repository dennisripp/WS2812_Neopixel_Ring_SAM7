#ifndef __PIO_PWM_H__
#define __PIO_PWM_H__
	
#include "aic.h"
	
typedef enum 
{
	PIO_CONFIG_INPUT=0,
	PIO_CONFIG_OUTPUT=1,
	PIO_CONFIG_CHANNELA=2,
	PIO_CONFIG_CHANNELB=3,
//---- Ergänzend zu konfiguieren -----
	PIO_CONFIG_OC      =0x010,
	PIO_CONFIG_PU      =0x020,
	PIO_CONFIG_DPU     =0x040,
	PIO_CONFIG_GLITCH  =0x080,
	PIO_CONFIG_IRQ     =0x100,
} PIO_CONFIG;


int  pwm_init (int channel, int freq,int alignment);
void pwm_set  (uint32_t value);

int pio_init  (uint32_t port,uint32_t channel, PIO_CONFIG config,
                                               IntVector  isr);
int pio_set   (uint32_t port,uint32_t channel,int         value );
int pio_get   (uint32_t port,uint32_t channel);
int pio_toggle(uint32_t port,uint32_t channel);
    
#endif
