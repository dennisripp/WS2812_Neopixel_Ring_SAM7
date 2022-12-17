#ifndef __ADC_H__
#define __ADC_H__

//ADVREF=3,3V
//AD0/PGMD5/PA17  = SOUND_ARMA -> Ausgang, zur Freigabe des Sound-Ausganges
//AD1/PGMD6/PA18  = DIGIA1   (Optional weiterer Analogeingang für Buchse A)
//AD2/PGMD7/PA19  = DIGIB1   (Optional weiterer Analogeingang für Buchse B)
//AD3/PGMD8/PA20  = DIGIC1   (Optional weiterer Analogeingang für Buchse C)
//AD4             = USB_ADC   
//AD5             = ADC_I     (Shunts + OP nicht bestückt)
//AD6             = VMBT_STATE  (Bluetooth)
//AD7             = DIGID1   (Optional weiterer Analogeingang für Buchse D)
#define ADC_CHANNEL_DIGIA1  1
#define ADC_CHANNEL_DIGIB1  2
#define ADC_CHANNEL_DIGIC1  3
#define ADC_CHANNEL_DIGID1  7
#define ADC_CHANNEL_USB     4
#define ADC_CHANNEL_CURRENT 5
#define ADC_CHANNEL_VMBT    6

//ADC_I
//- 8 Channel
//- 10-Bit 384 ksample/sec or 8-Bit 583 ksample/sec 
//- successive approximation
//- +-2LSB Integral Non Linearity; 
//- +-1LSB Differential Non Linearity
//- Integrated 8-to-1 Multiplexer
//- Multiple Trigger Sources
//  - Hardware or Software Trigger
//  - External Trigger Pin (PA8)
//  - Timer Counter 0 to 2
//- Sleep Mode
//- Conversion Sequencer
//- PDC-Mode (DMA)
//	  -10-Bit Mode bedingt 16-Bit Datentransfer
//	  - 8-Bit Mode bedingt  8-Bit Datentransfer

//ADC-Clock Frequency (10-Bit)		max 5MHz
//ADC-Clock Frequency ( 8-Bit)      max 8MHz
//Startup Time (Return from Idle)   max 20µs
//Track and Hold Acquisition Time   min 600ns
//Conversion Time (ADC-Clock=5MHz)  max 2µs  
//Conversion Time (ADC-Clock=8MHz)  max 1,25µ
//Throughput Rate (ADC-Clock=5MHz)  max 384kSPS = (3 Track/Hold + 10Conversion) AD-Cycles 
//Throughput Rate (ADC-Clock=8MHz)  max 533kSPS = (5 Track/Hold + 10Conversion) AD-Cycles

//ADC-Clock       =MCK       / ((adc_prescaler+1)*2)
//Startup-Time    =(startup+1)*8/ADC-Clock
//Sample/Hold-time=(sa_ho    )  /ADC-Clock
#define ADC_PRESCALER_8      ((uint32_t)(MCK/2000000/2))
#define ADC_STARTUP_8        ((uint32_t)(0.00002*8000000))
#define ADC_SAMPLE_HOLD_8    2
#define ADC_PRESCALER_10     ((uint32_t)(MCK/1000000/2))
#define ADC_STARTUP_10       ((uint32_t)(0.00002*5000000))
#define ADC_SAMPLE_HOLD_10   2

//'Falscher' Wertebereich in AT91SAM7S64
#define AT91DJ_ADC_PRESCAL     ((uint32_t) 0xFF <<  8) // (ADC) Prescaler rate selection
#define AT91DJ_ADC_STARTUP     ((uint32_t) 0x7F << 16) // (ADC) Startup Time
#define AT91DJ_ADC_SHTIM       ((uint32_t) 0x0F << 24) // (ADC) Sample & Hold Time

typedef enum {ADC_BIT_8,ADC_BIT_10}      ADC_BIT;
typedef enum {ADC_TRIG_SYSTICK,ADC_TRIG_TC0,ADC_TRIG_TC1,ADC_TRIG_TC2,ADC_TRIG_ADTRG} AD_TRIG;

#include "systick.h"

void     adc_init           (AD_TRIG trig,ADC_BIT bit);
void     adc_channel_convert(void);
void     adc_channel_set    (uint32_t channel);
uint32_t adc_channel_get    (uint32_t channel);
uint32_t adc_status         (void);

#endif