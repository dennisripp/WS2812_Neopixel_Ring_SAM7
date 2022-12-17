#include <stdint.h>
#include "../AT91SAM7S64.h"
#include "../main.h"
#include "pio_pwm.h"

//31-28 27-24 23-20 19-16 15-12 11-8 7-4 3-0
//            1000   0100  0000 0000 0000 0000
//	    0x800000
//		0x040000

const uint32_t nxtport2pio[]=
{                                      //                           PerA/PerB       PERA/PERB
	AT91C_PIO_PA23,  AT91C_PIO_PA18,   //Sensor-1 DIGx0/DIGx1  PA23=SCK1/PWM0  PA18=RD  /PCK2/AD1
	AT91C_PIO_PA28,  AT91C_PIO_PA19,   //Sensor-2 DIGx0/DIGx1  PA28=DSR1/TCLK1 PA19=RK  /FIQ /AD2
	AT91C_PIO_PA29,  AT91C_PIO_PA20,   //Sensor-3 DIGx0/DIGx1  PA29=RI1 /TCLK2 PA20=RF  /IRQ0/AD3
	AT91C_PIO_PA30,  AT91C_PIO_PA2,    //Sensor-4 DIGx0/DIGx1  PA30=IRQ1/NPCS2 PA2 =PWM2/SCK0
	//               AD7                                                                      AD7
	AT91C_PIO_PA15,  AT91C_PIO_PA1,    //Motor-A  INTx0/DIRx   PA15=TF  /TIOA1 PA1 =PWM1/TIOB0
	AT91C_PIO_PA26,  AT91C_PIO_PA9,    //Motor-B  INTx0/DIRx   PA26=DCD1/TIOA2 PA9 =DRXD/NPCS1
	AT91C_PIO_PA0,   AT91C_PIO_PA8     //Motor-C  INTx0/DIRx   PA0 =PWM0/TIOA0 PA8 =CTS0/ADTRG
};

#define ISR_TABLE_MAX 10
typedef struct {
	uint32_t mask;
	IntVector isr;
} isr_table_t;
isr_table_t pio_pwm_isr_table[ISR_TABLE_MAX+1];
 
void pio_pwm_isr_entry(void)
{
//  uint32_t i_state = interrupts_get_and_disable();

	uint32_t pinChanges  = *AT91C_PIOA_ISR;	// Acknowledge change
	isr_table_t *ptr;
	for(ptr=&pio_pwm_isr_table[0];ptr->mask;ptr++)
		if(ptr->mask & pinChanges)
			ptr->isr();

//  if (i_state)
//    interrupts_enable();
}

int pio_init(uint32_t port,uint32_t channel,PIO_CONFIG config,IntVector isr)
{
	int lauf;
	if((port > 7 ) || (channel > 1))
		return -1;

	*AT91C_PMC_PCER = (1 << AT91C_ID_PIOA);	/* Power to the pins! */
	
	switch(config & 0x0f) {
		case PIO_CONFIG_INPUT:
			AT91C_BASE_PIOA->PIO_PER   = nxtport2pio[2*port+channel]; /* enable pin = Disable Peripheral */		
			AT91C_BASE_PIOA->PIO_ODR   = nxtport2pio[2*port+channel];          /* configure as input pin */
			AT91C_BASE_PIOA->PIO_PPUDR = nxtport2pio[2*port+channel];                  /* disable PullUp */
			break;
		case PIO_CONFIG_OUTPUT:
			AT91C_BASE_PIOA->PIO_PER = nxtport2pio[2*port+channel];   /* enable pin = Disable Peripheral */
			AT91C_BASE_PIOA->PIO_OER = nxtport2pio[2*port+channel];           /* configure as output pin */		
			break;
		case PIO_CONFIG_CHANNELA:
			AT91C_BASE_PIOA->PIO_PDR = nxtport2pio[2*port+channel];  /* Disable pin = Enable Peripheral */		
			AT91C_BASE_PIOA->PIO_ASR = nxtport2pio[2*port+channel];                 /* Set Peripheral A */		
			break;
		case PIO_CONFIG_CHANNELB:
			AT91C_BASE_PIOA->PIO_PDR = nxtport2pio[2*port+channel];  /* Disable pin = Enable Peripheral */		
			AT91C_BASE_PIOA->PIO_BSR = nxtport2pio[2*port+channel];                 /* Set Peripheral B */		
			break;
	}
	if(port==3) {
		AT91C_BASE_PIOA->PIO_PER  = AT91C_PIO_PA7; /* an Port3 hängt der RS485 Treiber*/
		AT91C_BASE_PIOA->PIO_OER  = AT91C_PIO_PA7;
		AT91C_BASE_PIOA->PIO_CODR = AT91C_PIO_PA7; /* PA7=0 zum deaktiveren des RS485 Treibers */
	}
	
	if(config &  PIO_CONFIG_OC)
		AT91C_BASE_PIOA->PIO_MDER  = nxtport2pio[2*port+channel];

	if(config & PIO_CONFIG_PU)
		AT91C_BASE_PIOA->PIO_PPUER = nxtport2pio[2*port+channel];

	if(config & PIO_CONFIG_DPU)
		AT91C_BASE_PIOA->PIO_PPUDR = nxtport2pio[2*port+channel];

	if(config & PIO_CONFIG_GLITCH)
		AT91C_BASE_PIOA->PIO_IFER  = nxtport2pio[2*port+channel];

	if(config & PIO_CONFIG_IRQ) {
		for(lauf=0;lauf<ISR_TABLE_MAX;lauf++)
			if(pio_pwm_isr_table[lauf].mask == 0) {
				pio_pwm_isr_table[lauf].mask=nxtport2pio[2*port+channel];
				pio_pwm_isr_table[lauf].isr =isr;
				break;
			}
		if(lauf==ISR_TABLE_MAX)
			return -1;
			
		aic_mask_off(AT91C_ID_PIOA);
		aic_set_vector(AT91C_ID_PIOA, AIC_INT_LEVEL_ABOVE_NORMAL,pio_pwm_isr_entry);
								 /*AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL */
		aic_mask_on(AT91C_ID_PIOA);
		
		uint32_t pinChanges  = *AT91C_PIOA_ISR;	// Acknowledge change
		(void) pinChanges;
		AT91C_BASE_PIOA->PIO_IER = nxtport2pio[2*port+channel]; 
	}
	
	return 0;
}

int pio_set(uint32_t port,uint32_t channel,int value)
{
	if((port > 7 ) || (channel > 1))
		return -1;
		
	if(value == 0)
		AT91C_BASE_PIOA->PIO_CODR = nxtport2pio[2*port+channel];
	else
		AT91C_BASE_PIOA->PIO_SODR = nxtport2pio[2*port+channel];

	return 0;
}

int pio_get(uint32_t port,uint32_t channel)
{
	if((port > 7 ) || (channel > 1))
		return -1;

	return(AT91C_BASE_PIOA->PIO_PDSR & nxtport2pio[2*port+channel] ? 1 : 0);
}

int pio_toggle(uint32_t port,uint32_t channel)
{
	if((port > 7 ) || (channel > 1))
		return -1;

	if(AT91C_BASE_PIOA->PIO_PDSR & nxtport2pio[2*port+channel])
		AT91C_BASE_PIOA->PIO_CODR = nxtport2pio[2*port+channel];
	else
		AT91C_BASE_PIOA->PIO_SODR = nxtport2pio[2*port+channel];
	
	return 0;
}

/*******************************************************************************/

//Initialisierung bedingt, dass der zugehörigen Prozessor-Pin zuvor 
//auf den zugehörigen Peripherie-Kanal gesetzt wurde 
int pwm_init(int channel, int freq,int alignment)			
{
	if((channel > 3) || (channel < 0))
		return -1;
	
	// POWER for PWM
	AT91C_BASE_PMC->PMC_PCER = (1<<AT91C_ID_PWMC);
	
	int cpre;
	int prdr;
	for(cpre=0;cpre<11;cpre++)
	{
		if(alignment == 0)
			prdr = MCK/(1<<cpre)/freq;
		else
			prdr = MCK/(1<<cpre)/freq/2;
		if(prdr <= 65535)
			break;
	}

	//Vorteiler konfigurieren (nicht notwendig)
	//AT91C_BASE_PWMC->PWMC_MR = ((PREB&0xf)<<24) | ((DIVB&0xff)<<16) | ((PREA&0x0f)<<8) | ((DIVA&0xff)<<0);
	
	//Configure PWM
	AT91C_BASE_PWMC->PWMC_CH[channel].PWMC_CMR   = ((cpre&0x0f)<<0)                   /* MCPX / x */
	                                             | alignment ? AT91C_PWMC_CALG : 0; /*Channel Alignment*/ 
                                                 //| AT91C_PWMC_CPOL /*Channel Polarity*/
                                                 //| AT91C_PWMC_CPD  /*Channel Update Period (0->CUPDR->CDTYR 1->CUPDR->CPRDR)*/

	AT91C_BASE_PWMC->PWMC_CH[channel].PWMC_CPRDR = prdr;     //Period
	AT91C_BASE_PWMC->PWMC_CH[channel].PWMC_CDTYR = prdr/2;   //Duty Cycle
	//Waveform are fixed at 0 when: (CPOL=0 && CDTY=CPRD) || (CPOL=1 && CDTY=0   )
	//Waveform are fixed at 1 when: (CPOL=0 && CDTY=0   ) || (DPOL=1 && CDTY=CPRD) 

	// PWM aktivieren
	AT91C_BASE_PWMC->PWMC_ENA = 1 << channel;
	
	return prdr;
}

void pwm_set(uint32_t value)
{
	// PWM Cycle ändern
	//AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CUPDR = ( value ) & 0xFFFF;
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CDTYR = ( value ) & 0xFFFF;

}
