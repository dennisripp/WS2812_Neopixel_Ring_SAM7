#include "adc.h"
#include "../main.h"
#include "../AT91SAM7S64.h"
#include "systick.h"


static SYSTICK_VL adc_systick_vl;
void adc_channel_convert(void);

void adc_init(AD_TRIG trig,ADC_BIT bit)
{
#ifdef MODE_SIM
	//Im Trace32-Simulationsmode ist dieses Modul nicht enthalten
	//daher Workaround auf dieser Ebene notwendig
#endif	
	uint32_t mode_reg=0;
	
	//Reset ADC
	AT91C_BASE_ADC->ADC_CR=AT91C_ADC_SWRST;  
	
	//Sleep-Mode deaktivieren
	mode_reg= AT91C_ADC_SLEEP_NORMAL_MODE;
	
	switch(trig) {
		case ADC_TRIG_TC0:
			mode_reg|=AT91C_ADC_TRGEN_EN | AT91C_ADC_TRGSEL_TIOA0;
			//Timer muss im Waveform-Mode Konfiguriert werden
			break;
		case ADC_TRIG_TC1:
			mode_reg|=AT91C_ADC_TRGEN_EN | AT91C_ADC_TRGSEL_TIOA1;
			//Timer muss im Waveform-Mode Konfiguriert werden
			break;
		case ADC_TRIG_TC2:
			mode_reg|=AT91C_ADC_TRGEN_EN | AT91C_ADC_TRGSEL_TIOA2;
			//Timer muss im Waveform-Mode Konfiguriert werden
			break;
		case ADC_TRIG_ADTRG:
			mode_reg|=AT91C_ADC_TRGEN_EN | AT91C_ADC_TRGSEL_EXT;
			//PIO-Controller PA8 entsprechend setzen
			break;
		case ADC_TRIG_SYSTICK:
		default:
			mode_reg|=AT91C_ADC_TRGEN_DIS;
			break;
	}

	switch(bit) {
		case ADC_BIT_8:
			mode_reg|= ((ADC_PRESCALER_8   <<8 )&AT91DJ_ADC_PRESCAL);  //3f
			mode_reg|= ((ADC_STARTUP_8     <<16)&AT91DJ_ADC_STARTUP); //1f
			mode_reg|= ((ADC_SAMPLE_HOLD_8 <<24)&AT91DJ_ADC_SHTIM  ); //0f
			mode_reg|=AT91C_ADC_LOWRES_8_BIT;
			break;
		case ADC_BIT_10:
		default:
			mode_reg|= ((ADC_PRESCALER_10  <<8 )&AT91DJ_ADC_PRESCAL);  //3f
			mode_reg|= ((ADC_STARTUP_10    <<16)&AT91DJ_ADC_STARTUP); //1f
			mode_reg|= ((ADC_SAMPLE_HOLD_10<<24)&AT91DJ_ADC_SHTIM  ); //0f
			mode_reg|=AT91C_ADC_LOWRES_10_BIT;
			break;
	}
	AT91C_BASE_ADC->ADC_MR = mode_reg;
	
	//Optional: IRQ
	//	EOC0..7     End of Conversion
	//  OVRE0..7    Overrun
	//  DRDY        Data Ready
	//  GOVRE       General Overrun
	//  ENDRX       End of Receive Buffer
	//  RXBUFF      Receive Buffer Full
	
	//Optional: DMA Datentransfer
	
//	adc_channel_set(ADC_CHANNEL_USB);
//	adc_channel_set(ADC_CHANNEL_CURRENT);

//	adc_channel_set(ADC_CHANNEL_DIGIA1);
//	adc_channel_set(ADC_CHANNEL_DIGIB1);
//	adc_channel_set(ADC_CHANNEL_DIGIC1);
//	adc_channel_set(ADC_CHANNEL_DIGID1);
//	adc_channel_set(ADC_CHANNEL_VMBT);
	
	/* Callback Routine einhängen */
	if(trig==ADC_TRIG_SYSTICK)
		systick_callback(&adc_systick_vl,adc_channel_convert);
}

void adc_channel_convert(void)
{
	//Begins analog-to-digital conversion
	AT91C_BASE_ADC->ADC_CR=AT91C_ADC_START;
}


void adc_channel_set(uint32_t channel)
{
	//Defaultmäßig ist der zugehörige Prozessor-Pin auf Eingabe geschaltet und der AD-Eingang intern auf GND gelegt
	//Sobald der AD-Eingang als ADC per ADC_CHER gesetzt, wird die PIO-Konfiguration 'überschrieben' und der 
	//Prozessor-Pin mit dem AD-Eingang verbunden
	
	if(channel < 8)
		AT91C_BASE_ADC->ADC_CHER=1<<channel;
//	AT91C_BASE_ADC->ADC_CHDR   Disable
//	AT91C_BASE_ADC->ADC_CHSR   Status
}

uint32_t adc_channel_get(uint32_t channel)
{
	//36.5.3 By setting the bit LOWRES, the ADC switches in the lowest resolution
	//and the vonversion results can be read in the eight lowes significatn bits
	//of the data registers. The two highest bits of the DATA field in the 
	//corresponding ADC_CDR register and of the LDATA field in the ADC_LCDR
	//register read 0.
	return channel < 8 ? AT91C_ADC_CDR0[channel]:0;
}

uint32_t adc_status(void)
{
	return AT91C_BASE_ADC->ADC_SR;
	//Bit  0 -> EOC0  Channel is enabled and conversion is completed
	//Bit  7 -> EOC1  Channel is enabled and conversion is completed
	//Bit  8 -> OVRE0 There has been an overrun (no read operation) since the last read
	//Bit 15 -> OVRE7 There has been an overrun (no read operation) since the last read
	//Bit 16 -> DRDY  At least one data has been converted and is available in ADC_LCDR
	//Bit 17 -> GOVRE At least on general Overrun has occured since the last read of ADC_SR
	//Bit 18 -> ENDRX The receiver counter has reached 0 since the last write in ADC_RCR or ADC_RNCR
	//Bit 19 -> RXBUF Both ADC_RCR and ADC_RNCR have a value of 0
}
