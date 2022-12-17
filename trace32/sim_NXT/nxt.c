/**************************************************************************
* Product: AT91SAM7S256 & Lego Mindstorm Port Emulator for Lauterbach Trace32
* Last Updated for Version: 1.0.00
* Date of the Last Update:  6.12.2013
*
*                    Ostfalia Hochschule für angewandte Wissenschaften
*                    ---------------------------
*                    Prof. Justen / Fakultät Informatik
*
* Copyright (C) 2013-2019 D. Justen, LLC. All rights reserved.
*
* This software may be distributed and modified under the terms of the GNU
* General Public License version 2 (GPL) as published by the Free Software
* Foundation and appearing in the file GPL.TXT included in the packaging of
* this file. Please note that GPL Section 2[b] requires that all works based
* on this software must also be made publicly available under the terms of
* the GPL ("Copyleft").
*
* Contact information:
* e-mail:                  d.justen@ostfalia.de
***************************************************************************
 NXT
 Implementiert:
 - LCD über SPI
 - AVR über TWI
 - Digitalen Ein- und Ausgänge der Sensor / Aktor Ports
 - Dazugehöriges PER File
 Nicht  Implementiert
 - der ganze Rest
**************************************************************************/
#include "simul.h"
#include "AT91SAM7S64.h"
#include "sim_NXT.h"
#include <stdarg.h>
#include <stdio.h>

#define NXT_WIDTH 150
#define NXT_HIGH  250

#define LCD_X      25
#define LCD_Y      50
#define LCD_WIDTH 100
#define LCD_HIGH   64

#define KEY_X     (NXT_WIDTH/2)
#define KEY_Y     (LCD_Y+LCD_HIGH+30)
#define KEY_WIDTH 20
#define KEY_HIGH  20

#define NXT_MEMORY_SIZE 0xC0
#define NXT_AVR_ADC_1           (NXT_MEMORY_OFFSET + 0x00)
#define NXT_AVR_ADC_2           (NXT_MEMORY_OFFSET + 0x04)
#define NXT_AVR_ADC_3           (NXT_MEMORY_OFFSET + 0x08)
#define NXT_AVR_ADC_4           (NXT_MEMORY_OFFSET + 0x0C)
#define NXT_AVR_BATTERY         (NXT_MEMORY_OFFSET + 0x10)
#define NXT_AVR_BUTTON          (NXT_MEMORY_OFFSET + 0x14)
#define NXT_AVR_BUTTON_STATE    (NXT_MEMORY_OFFSET + 0x18)
#define NXT_AVR_POWER           (NXT_MEMORY_OFFSET + 0x20)
#define NXT_AVR_PWM_FREQUENCY   (NXT_MEMORY_OFFSET + 0x24)
#define NXT_AVR_OUTPUT_PERCENTA (NXT_MEMORY_OFFSET + 0x28)
#define NXT_AVR_OUTPUT_PERCENTB (NXT_MEMORY_OFFSET + 0x2C)
#define NXT_AVR_OUTPUT_PERCENTC (NXT_MEMORY_OFFSET + 0x30)
#define NXT_AVR_OUTPUT_PERCENTD (NXT_MEMORY_OFFSET + 0x34)
#define NXT_AVR_OUTPUT_MODE     (NXT_MEMORY_OFFSET + 0x38)
#define NXT_AVR_INPUT_POWER     (NXT_MEMORY_OFFSET + 0x3C)
#define NXT_PIO_PA0             (NXT_MEMORY_OFFSET + 0x40)
#define NXT_PIO_PA1             (NXT_MEMORY_OFFSET + 0x44)
#define NXT_PIO_PA2             (NXT_MEMORY_OFFSET + 0x48)
#define NXT_PIO_PA3             (NXT_MEMORY_OFFSET + 0x4C)
#define NXT_PIO_PA4             (NXT_MEMORY_OFFSET + 0x50)
#define NXT_PIO_PA5             (NXT_MEMORY_OFFSET + 0x54)
#define NXT_PIO_PA6             (NXT_MEMORY_OFFSET + 0x58)
#define NXT_PIO_PA7             (NXT_MEMORY_OFFSET + 0x5C)
#define NXT_PIO_PA8             (NXT_MEMORY_OFFSET + 0x60)
#define NXT_PIO_PA9             (NXT_MEMORY_OFFSET + 0x64)
#define NXT_PIO_PA10            (NXT_MEMORY_OFFSET + 0x68)
#define NXT_PIO_PA11            (NXT_MEMORY_OFFSET + 0x6C)
#define NXT_PIO_PA12            (NXT_MEMORY_OFFSET + 0x70)
#define NXT_PIO_PA13            (NXT_MEMORY_OFFSET + 0x74)
#define NXT_PIO_PA14            (NXT_MEMORY_OFFSET + 0x78)
#define NXT_PIO_PA15            (NXT_MEMORY_OFFSET + 0x7C)
#define NXT_PIO_PA16            (NXT_MEMORY_OFFSET + 0x80)
#define NXT_PIO_PA17            (NXT_MEMORY_OFFSET + 0x84)
#define NXT_PIO_PA18            (NXT_MEMORY_OFFSET + 0x88)
#define NXT_PIO_PA19            (NXT_MEMORY_OFFSET + 0x8C)
#define NXT_PIO_PA20            (NXT_MEMORY_OFFSET + 0x90)
#define NXT_PIO_PA21            (NXT_MEMORY_OFFSET + 0x94)
#define NXT_PIO_PA22            (NXT_MEMORY_OFFSET + 0x98)
#define NXT_PIO_PA23            (NXT_MEMORY_OFFSET + 0x9C)
#define NXT_PIO_PA24            (NXT_MEMORY_OFFSET + 0xA0)
#define NXT_PIO_PA25            (NXT_MEMORY_OFFSET + 0xA4)
#define NXT_PIO_PA26            (NXT_MEMORY_OFFSET + 0xA8)
#define NXT_PIO_PA27            (NXT_MEMORY_OFFSET + 0xAC)
#define NXT_PIO_PA28            (NXT_MEMORY_OFFSET + 0xB0)
#define NXT_PIO_PA29            (NXT_MEMORY_OFFSET + 0xB4)
#define NXT_PIO_PA30            (NXT_MEMORY_OFFSET + 0xB8)
#define NXT_PIO_PA31            (NXT_MEMORY_OFFSET + 0xBC)

#define BUTTON_MITTE          0x000000ff  // button_state=1
#define BUTTON_UNTEN          0x0000ff00  // button_state=8
#define BUTTON_LEFT           0x00ff0000  // button_state=2
#define BUTTON_RIGHT          0xff000000  // button_state=4
#define BUTTON_VAL(x)        (x&BUTTON_MITTE ? 2047 : \
                              x&BUTTON_UNTEN ?  750 : \
							  x&BUTTON_RIGHT ?  300 : \
							  x&BUTTON_LEFT  ?  100 : 0)

typedef unsigned char  U8;
typedef   signed char  S8;
typedef unsigned short U16;
typedef   signed short S16;
typedef unsigned int   U32;
typedef   signed int   S32;

#define NXT_AVR_ADDRESS 1
#define NXT_AVR_N_OUTPUTS 4
#define NXT_AVR_N_INPUTS  4

// This string is used to establish communictions with the AVR
const char avr_brainwash_string[] =
  "\xCC" "Let's samba nxt arm in arm, (c)LEGO System A/S";

// The following Raw values are read/written directly to the AVR. so byte
// order, packing etc. must match
typedef struct{
  // Raw values
  U8  power;
  U8  pwm_frequency;
  S8  output_percent[NXT_AVR_N_OUTPUTS];
  U8  output_mode;
  U8  input_power;
  U8  checksum;
} __attribute__((packed)) TO_AVR;

typedef struct {
  // Raw values
  U16 adc_value[NXT_AVR_N_INPUTS];
  U16 buttonsVal;
  U16 extra;
  U8 csum;
} __attribute__((packed)) FROM_AVR;


typedef enum
{
	AVR_INIT,
	AVR_CONNECTING,
	AVR_WORKING
} AVR_MODE;

typedef struct
{
	AVR_MODE mode;
	TO_AVR   to_avr;
	FROM_AVR from_avr;
} AVR;

/**************************************************************************/

typedef enum 
{
	LCD_IDLE,
	LCD_POT
} LCD_MODE;

typedef struct
{
	LCD_MODE      mode;
	unsigned char col;
	unsigned char mr;	//Multiplex Rate
	unsigned char tc;
	unsigned char hi;
	unsigned char pc;
	unsigned char sl;
	unsigned char page;
	unsigned char ac;
	unsigned char fr;
	unsigned char on;
	unsigned char inverse;
	unsigned char enable;
	unsigned char map_control;
	unsigned char reset;
	unsigned char ratio;
	unsigned char cursor_upate;
	unsigned char black;
	unsigned char white;
} LCD;

/**************************************************************************/
typedef enum
{
	PIO_UNDEF,
	PIO_INPUT,
	PIO_OUTPUT_ODSR,
	PIO_OUTPUT_A,
	PIO_OUTPUT_B,
	PIO_OUTPUT_OC_ODSR,
	PIO_OUTPUT_OC_A,
	PIO_OUTPUT_OC_B
}PIO_MODE;
const char PIO_MODE_CHAR[]={'X','I','O','A','B','o','a','b'};

/**************************************************************************/
typedef struct 
{
	U32    nxt_avr_adc_1;
	U32    nxt_avr_adc_2;
	U32    nxt_avr_adc_3;
	U32    nxt_avr_adc_4;
	U32    nxt_avr_battery;
	U32    nxt_avr_button;
	//----
	U32	   nxt_avr_power;				//Wird nicht wirklich genutzt. Ansonsten mit PWM_FREQUENCY zur erweiterten Sterung
	U32    nxt_avr_pwm_frequency;		//Wird nur einmal gesetzt. Ansonsten mit POWER zur Erweiterten Steuerung
	S32    nxt_avr_output_percentA;
	S32    nxt_avr_output_percentB;
	S32    nxt_avr_output_percentC;
	S32    nxt_avr_output_percentD;
	U32    nxt_avr_output_mode;         //Bit3: Brake MotorD ... Bit0: Brake MotorA
	U32    nxt_avr_input_power;			//Bit4 & 0->Sensor 1  01->9V Pulsed 10->9V Always 00->9V Off
	                                    //Bit5 & 1->Sensor 2  01->9V Pulsed 10->9V Always 00->9V Off
										//Bit6 & 2->Sensor 3  01->9V Pulsed 10->9V Always 00->9V Off
										//Bit7 & 3->Sensor 4  01->9V Pulsed 10->9V Always 00->9V Off
	//----
	PIO_MODE  pio_mode[32];
	simulWord pio_state[32];
} DATA;


/**************************************************************************/
typedef struct
{
    void  *timerid;
	AVR    avr;
	LCD    lcd;
	DATA   akt;
	DATA   old;
} NXT;

/**************************************************************************/

static void	update(simulProcessor processor, simulPtr private, int mode);

/**************************************************************************/

/* Font table for a 5x8 font. 1 pixel spacing between chars */
#define N_CHARS 128
#define FONT_WIDTH 5

static const U8 font[N_CHARS][FONT_WIDTH] = {
/* 0x00 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x01 */ {0x3E, 0x55, 0x61, 0x55, 0x3E},
/* 0x02 */ {0x3E, 0x6B, 0x5F, 0x6B, 0x3E},
/* 0x03 */ {0x0C, 0x1E, 0x3C, 0x1E, 0x0C},
/* 0x04 */ {0x08, 0x1C, 0x3E, 0x1C, 0x08},
/* 0x05 */ {0x18, 0x5E, 0x7E, 0x5E, 0x18},
/* 0x06 */ {0x18, 0x5C, 0x7E, 0x5C, 0x18},
/* 0x07 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x08 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x09 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x0A */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x0B */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x0C */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x0D */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x0E */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x0F */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x10 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x11 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x12 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x13 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x14 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x15 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x16 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x17 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x18 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x19 */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x1A */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x1B */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x1C */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x1D */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x1E */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x1F */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
/* 0x20 */ {0x00, 0x00, 0x00, 0x00, 0x00},
/* 0x21 */ {0x00, 0x00, 0x5F, 0x00, 0x00},
/* 0x22 */ {0x00, 0x07, 0x00, 0x07, 0x00},
/* 0x23 */ {0x14, 0x3E, 0x14, 0x3E, 0x14},
/* 0x24 */ {0x04, 0x2A, 0x7F, 0x2A, 0x10},
/* 0x25 */ {0x26, 0x16, 0x08, 0x34, 0x32},
/* 0x26 */ {0x36, 0x49, 0x59, 0x26, 0x50},
/* 0x27 */ {0x00, 0x00, 0x07, 0x00, 0x00},
/* 0x28 */ {0x00, 0x1C, 0x22, 0x41, 0x00},
/* 0x29 */ {0x00, 0x41, 0x22, 0x1C, 0x00},
/* 0x2A */ {0x2A, 0x1C, 0x7F, 0x1C, 0x2A},
/* 0x2B */ {0x08, 0x08, 0x3E, 0x08, 0x08},
/* 0x2C */ {0x00, 0x50, 0x30, 0x00, 0x00},
/* 0x2D */ {0x08, 0x08, 0x08, 0x08, 0x08},
/* 0x2E */ {0x00, 0x60, 0x60, 0x00, 0x00},
/* 0x2F */ {0x20, 0x10, 0x08, 0x04, 0x02},
/* 0x30 */ {0x3E, 0x51, 0x49, 0x45, 0x3E},
/* 0x31 */ {0x00, 0x42, 0x7F, 0x40, 0x00},
/* 0x32 */ {0x42, 0x61, 0x51, 0x49, 0x46},
/* 0x33 */ {0x21, 0x41, 0x45, 0x4B, 0x31},
/* 0x34 */ {0x18, 0x14, 0x12, 0x7F, 0x10},
/* 0x35 */ {0x27, 0x45, 0x45, 0x45, 0x39},
/* 0x36 */ {0x3C, 0x4A, 0x49, 0x49, 0x30},
/* 0x37 */ {0x01, 0x01, 0x79, 0x05, 0x03},
/* 0x38 */ {0x36, 0x49, 0x49, 0x49, 0x36},
/* 0x39 */ {0x06, 0x49, 0x49, 0x29, 0x1E},
/* 0x3A */ {0x00, 0x36, 0x36, 0x00, 0x00},
/* 0x3B */ {0x00, 0x56, 0x36, 0x00, 0x00},
/* 0x3C */ {0x08, 0x14, 0x22, 0x41, 0x00},
/* 0x3D */ {0x14, 0x14, 0x14, 0x14, 0x14},
/* 0x3E */ {0x41, 0x22, 0x14, 0x08, 0x00},
/* 0x3F */ {0x02, 0x01, 0x59, 0x05, 0x02},
/* 0x40 */ {0x1C, 0x2A, 0x36, 0x3E, 0x0C},
/* 0x41 */ {0x7E, 0x09, 0x09, 0x09, 0x7E},
/* 0x42 */ {0x7F, 0x49, 0x49, 0x49, 0x3E},
/* 0x43 */ {0x3E, 0x41, 0x41, 0x41, 0x22},
/* 0x44 */ {0x7F, 0x41, 0x41, 0x22, 0x1C},
/* 0x45 */ {0x7F, 0x49, 0x49, 0x49, 0x41},
/* 0x46 */ {0x7F, 0x09, 0x09, 0x09, 0x01},
/* 0x47 */ {0x3E, 0x41, 0x41, 0x49, 0x3A},
/* 0x48 */ {0x7F, 0x08, 0x08, 0x08, 0x7F},
/* 0x49 */ {0x00, 0x41, 0x7F, 0x41, 0x00},
/* 0x4A */ {0x20, 0x40, 0x41, 0x3F, 0x01},
/* 0x4B */ {0x7F, 0x08, 0x14, 0x22, 0x41},
/* 0x4C */ {0x7F, 0x40, 0x40, 0x40, 0x40},
/* 0x4D */ {0x7F, 0x02, 0x04, 0x02, 0x7F},
/* 0x4E */ {0x7F, 0x04, 0x08, 0x10, 0x7F},
/* 0x4F */ {0x3E, 0x41, 0x41, 0x41, 0x3E},
/* 0x50 */ {0x7F, 0x09, 0x09, 0x09, 0x06},
/* 0x51 */ {0x3E, 0x41, 0x51, 0x21, 0x5E},
/* 0x52 */ {0x7F, 0x09, 0x19, 0x29, 0x46},
/* 0x53 */ {0x26, 0x49, 0x49, 0x49, 0x32},
/* 0x54 */ {0x01, 0x01, 0x7F, 0x01, 0x01},
/* 0x55 */ {0x3F, 0x40, 0x40, 0x40, 0x3F},
/* 0x56 */ {0x1F, 0x20, 0x40, 0x20, 0x1F},
/* 0x57 */ {0x7F, 0x20, 0x18, 0x20, 0x7F},
/* 0x58 */ {0x63, 0x14, 0x08, 0x14, 0x63},
/* 0x59 */ {0x03, 0x04, 0x78, 0x04, 0x03},
/* 0x5A */ {0x61, 0x51, 0x49, 0x45, 0x43},
/* 0x5B */ {0x00, 0x7F, 0x41, 0x41, 0x00},
/* 0x5C */ {0x02, 0x04, 0x08, 0x10, 0x20},
/* 0x5D */ {0x00, 0x41, 0x41, 0x7F, 0x00},
/* 0x5E */ {0x04, 0x02, 0x01, 0x02, 0x04},
/* 0x5F */ {0x40, 0x40, 0x40, 0x40, 0x40},
/* 0x60 */ {0x00, 0x00, 0x07, 0x00, 0x00},
/* 0x61 */ {0x20, 0x54, 0x54, 0x54, 0x78},
/* 0x62 */ {0x7f, 0x48, 0x44, 0x44, 0x38},
/* 0x63 */ {0x30, 0x48, 0x48, 0x48, 0x20},
/* 0x64 */ {0x38, 0x44, 0x44, 0x48, 0x7f},
/* 0x65 */ {0x38, 0x54, 0x54, 0x54, 0x18},
/* 0x66 */ {0x08, 0x7e, 0x09, 0x09, 0x02},
/* 0x67 */ {0x0c, 0x52, 0x52, 0x52, 0x3e},
/* 0x68 */ {0x7f, 0x08, 0x04, 0x04, 0x78},
/* 0x69 */ {0x00, 0x44, 0x7d, 0x40, 0x00},
/* 0x6A */ {0x20, 0x40, 0x40, 0x3d, 0x00},
/* 0x6B */ {0x7f, 0x10, 0x28, 0x44, 0x00},
/* 0x6C */ {0x00, 0x41, 0x7f, 0x40, 0x00},
/* 0x6D */ {0x7c, 0x04, 0x18, 0x04, 0x78},
/* 0x6E */ {0x7c, 0x08, 0x04, 0x04, 0x78},
/* 0x6F */ {0x38, 0x44, 0x44, 0x44, 0x38},
/* 0x70 */ {0xfc, 0x14, 0x14, 0x14, 0x08},
/* 0x71 */ {0x08, 0x14, 0x14, 0x18, 0x7c},
/* 0x72 */ {0x7c, 0x08, 0x04, 0x04, 0x08},
/* 0x73 */ {0x48, 0x54, 0x54, 0x54, 0x20},
/* 0x74 */ {0x04, 0x3f, 0x44, 0x40, 0x20},
/* 0x75 */ {0x3c, 0x40, 0x40, 0x20, 0x7c},
/* 0x76 */ {0x1c, 0x20, 0x40, 0x20, 0x1c},
/* 0x77 */ {0x3c, 0x40, 0x38, 0x40, 0x3c},
/* 0x78 */ {0x44, 0x28, 0x10, 0x28, 0x44},
/* 0x79 */ {0x0c, 0x50, 0x50, 0x50, 0x3c},
/* 0x7A */ {0x44, 0x64, 0x54, 0x4c, 0x44},
/* 0x7B */ {0x00, 0x08, 0x36, 0x41, 0x00},
/* 0x7C */ {0x00, 0x00, 0x7F, 0x00, 0x00},
/* 0x7D */ {0x00, 0x41, 0x36, 0x08, 0x00},
/* 0x7E */ {0x00, 0x07, 0x00, 0x07, 0x00},
/* 0x7F */ {0x3E, 0x36, 0x2A, 0x36, 0x3E},
};

static void display_char(simulProcessor processor,char zei,int links, int unten, int bc, int fc)
{
	simulWord data;
	simulWord address;
	simulWord x;
	simulWord y;
	simulWord flag;
	
	for(flag=0x80,y=unten;y>unten-8;y--,flag>>=1)
	{
		address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+links;
		for(x=0; x<FONT_WIDTH; x++)
		{
			data=font[zei][x] & flag ? fc : bc;
			if(data != -1)
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			address++;
		}
	}
}

static void display_printf(simulProcessor processor,int links, int unten, int bc, int fc,const char *format, ...)
{
	va_list vargzeiger;
	char string[255];
	int  lauf;
	
	va_start(vargzeiger,format);
	vsnprintf(string,255,format,vargzeiger);
	va_end(vargzeiger);

	for(lauf=0; string[lauf]!=0; lauf++)
		display_char(processor,string[lauf],links+5*lauf,unten,bc,fc);
}

/**************************************************************************/

static int avr_init(simulProcessor processor,simulPtr private)
{
	AVR *avr = &((NXT *)private)->avr;
	
	avr->mode = AVR_INIT;

}

void	avr_fromavr(simulProcessor processor, simulPtr private, 
                        simulWord dadr, simulWord iadr, simulWord byte, simulWord *rhr)
{
	NXT *nxt = (NXT *)private;
	
	if(dadr != NXT_AVR_ADDRESS)
	{
		SIMUL_Printf(processor,"NXT Nicht unterstützte TWI-Receive-Adresse SOLL=1 IST=%d",dadr);
		return;
	}
		
	switch(nxt->avr.mode)
	{
		case AVR_INIT:
		case AVR_CONNECTING:
			break;			
		case AVR_WORKING:
			if(byte == 1)
			{
				update(processor,private,1);
				
				nxt->avr.from_avr.adc_value[0]=nxt->akt.nxt_avr_adc_1;
				nxt->avr.from_avr.adc_value[1]=nxt->akt.nxt_avr_adc_2;
				nxt->avr.from_avr.adc_value[2]=nxt->akt.nxt_avr_adc_3;
				nxt->avr.from_avr.adc_value[3]=nxt->akt.nxt_avr_adc_4;
				nxt->avr.from_avr.buttonsVal  =BUTTON_VAL(nxt->akt.nxt_avr_button);
				nxt->avr.from_avr.extra       =nxt->akt.nxt_avr_battery;
				
				unsigned char checksum=0;
				for(byte=0;byte<(sizeof(FROM_AVR)-1);byte++)
					checksum+=((unsigned char *)&nxt->avr.from_avr)[byte];
				nxt->avr.from_avr.csum = ~checksum;
				byte=1;
			}
			if(byte <= sizeof(FROM_AVR))
			{
				*rhr=((unsigned char *)&nxt->avr.from_avr)[byte-1];
			}
			break;
	}
}

void	avr_toavr(simulProcessor processor, simulPtr private, 
                     simulWord dadr, simulWord iadr, simulWord byte, simulWord thr)
{
	NXT *nxt = (NXT *)private;

	if(dadr != NXT_AVR_ADDRESS)
	{
		SIMUL_Printf(processor,"NXT Nicht unterstützte TWI-Transmit-Adresse SOLL=1 IST=%d",dadr);
		return;
	}

	switch(nxt->avr.mode)
	{
		case AVR_INIT:
			if(((char)thr == avr_brainwash_string[0]) && (byte==1))
				nxt->avr.mode=AVR_CONNECTING;
			break;
		case AVR_CONNECTING:
			if((char)thr != avr_brainwash_string[byte-1])
				nxt->avr.mode=AVR_INIT;
			if(byte == (sizeof(avr_brainwash_string)-1))
			{
				nxt->avr.mode=AVR_WORKING;
			}
			break;
		case AVR_WORKING:
			if(byte <= sizeof(TO_AVR))
			{
				((unsigned char *)&nxt->avr.to_avr)[byte-1]=(unsigned char)thr;
				if(byte == sizeof(TO_AVR))
				{
					unsigned char checksum=0;
					for(byte=0;byte<sizeof(TO_AVR);byte++)
						checksum+=((unsigned char *)&nxt->avr.to_avr)[byte];
					if(checksum == 0xff)
					{
						nxt->akt.nxt_avr_power           = nxt->avr.to_avr.power;
						nxt->akt.nxt_avr_pwm_frequency   = nxt->avr.to_avr.pwm_frequency;
						nxt->akt.nxt_avr_output_percentA = nxt->avr.to_avr.output_percent[0];
						nxt->akt.nxt_avr_output_percentB = nxt->avr.to_avr.output_percent[1];
						nxt->akt.nxt_avr_output_percentC = nxt->avr.to_avr.output_percent[2];
						nxt->akt.nxt_avr_output_percentD = nxt->avr.to_avr.output_percent[3];
						nxt->akt.nxt_avr_output_mode     = nxt->avr.to_avr.output_mode;
						nxt->akt.nxt_avr_input_power     = nxt->avr.to_avr.input_power;

						if((nxt->akt.nxt_avr_power == 0x5a) && (nxt->akt.nxt_avr_pwm_frequency == 0x00))
						{	//Power_Down
							SIMUL_Warning(processor,"Power Down wird nicht unterstützt");
						}
						if((nxt->akt.nxt_avr_power == 0xA5) && (nxt->akt.nxt_avr_pwm_frequency == 0x5A))
						{	//Firmware_Update (Tell the AVR to enter SAMBA mode)
							SIMUL_Warning(processor,"Firmware Upate wird nicht unterstützt");
						}
						update(processor,private,0);
					}
				}
			}
			break;
	}
}

/**************************************************************************/

static int lcd_init(simulProcessor processor,simulPtr private)
{
	LCD       *lcd = &((NXT *)private)->lcd;

	lcd->col=0;
	lcd->page=0;
	lcd->mode = LCD_IDLE;
	lcd->black=0x00;
	lcd->white=200;
}

void	lcd_sendreceive(simulProcessor processor, simulPtr private,
							simulWord tdr, simulWord *rdr)
{
	LCD        *lcd = &((NXT *)private)->lcd;
	simulWord   data;
	simulWord	address;
	simulWord	flag;
	simulWord	pos;

	//Receive = Send
	*rdr=tdr;
	
	if(((NXT *)private)->akt.pio_state[PIO_PORT_LCD_CD-PIO_PORTPIN_OFFSET])
	{	//Data
		if(lcd->col < LCD_WIDTH)
		{
			address = LCD_MEMORY_OFFSET + LCD_X + lcd->col + (LCD_Y+lcd->page*8) * NXT_WIDTH;
			
			for(pos=0,flag=0x01;pos<8;pos++,flag<<=1)
			{
				data = tdr & flag ? lcd->black : lcd->white;
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
				address += NXT_WIDTH;
			}
		}
		lcd->col ++;
		if(lcd->col >= 132)
		{
			lcd->col = 0;
			lcd->page = (lcd->page + 1) & 0x07;
		}
	}
	else
	{	//Control
//SIMUL_Printf(processor,"%04x   ",tdr&0xFFFF);
		switch(lcd->mode)
		{
			case LCD_IDLE:
				if((tdr & 0xF0) == 0x00)
				{	//nxt_lcd_set_col()
					lcd->col= (lcd->col&0xF0) | (tdr&0x0F);
				}
				else if((tdr & 0xF0) == 0x10)
				{	//nxt_lcd_set_col()
					lcd->col= (lcd->col&0x0F) | ((tdr<<4)&0xF0);
//SIMUL_Printf(processor,"LCD-Set-col %d (Soll=0)\n",lcd->col);				
				}
				else if((tdr & 0xF0) == 0x20)
				{	//nxt_lcd_set_multiplex_rate
					lcd->mr = tdr & 0x03;
//SIMUL_Printf(processor,"LCD-Set-Multiplex-Rate %d (Soll=3)\n",lcd->mr);				
				}
				else if((tdr & 0xFC) == 0x24)
				{	//nxt_lcd_set_temp_comp
					lcd->tc = tdr & 0x03;
				}
				else if((tdr & 0xFC) == 0x28)
				{	//nxt_lcd_set_panel_loading(U32 hi)
					lcd->hi = tdr & 0x01;
				}
				else if((tdr & 0xFC) == 0x2C)
				{	//nxt_lcd_set_pump_control(U32 pc)
					lcd->pc = tdr & 0x03;
				}
				else if((tdr & 0xC0) == 0x40)
				{	//nxt_lcd_set_scroll_line(U32 sl)
					lcd->sl = tdr & 0x3F;
				}
				else if((tdr & 0xF0) == 0xB0)
				{	//nxt_lcd_set_page_address(U32 pa)
					lcd->page = tdr & 0x0f;
				}
				else if((tdr & 0xF8) == 0x88)
				{ 	//nxt_lcd_set_ram_address_control(U32 ac)
					lcd->ac = tdr & 0x07;
//SIMUL_Printf(processor,"LCD-Set-RAM-Address-Control %d (Soll=1)\n",lcd->ac);				
					//1=AutoWrap
				}
				else if((tdr & 0xFF) == 0x81)
				{
					lcd->mode = LCD_POT;
				}
				else if((tdr & 0xFE) == 0xA0)
				{	//nxt_lcd_set_frame_rate(U32 fr)
					lcd->fr = tdr & 1;
				}
				else if((tdr & 0xFE) == 0xA4)
				{	//nxt_lcd_set_all_pixels_on(U32 on)
					lcd->on = tdr & 1;
				}
				else if((tdr & 0xFE) == 0xA6)
				{	//nxt_lcd_inverse_display(U32 on)
					lcd->inverse = tdr & 1;
				}
				else if((tdr & 0xFE) == 0xAE)
				{	//nxt_lcd_enable(U32 on)
					lcd->enable = tdr & 1;
//SIMUL_Printf(processor,"LCD-Enable %d (Soll=1->Enable)\n",lcd->enable);				
				}
				else if((tdr & 0xF0) == 0xC0)
				{	//nxt_lcd_set_map_control(U32 map_control)
					lcd->map_control = (tdr>>1) & 0x3;
					if(lcd->map_control != 2)
						SIMUL_Warning(processor,"LCD Map_Control != 2 wird nicht unterstützt");
//SIMUL_Printf(processor,"LCD-Set-Map-Control %d (Soll=2->Mirror in Y)\n",lcd->map_control);
				}
				else if((tdr & 0xFE) == 0xE2) 
				{	//nxt_lcd_reset(void)
					lcd->reset = 1;
//SIMUL_Printf(processor,"LCD-Reset\n");				
				}
				else if((tdr & 0xFC) == 0xE8)
				{	//nxt_lcd_set_bias_ratio(U32 ratio)
					lcd->ratio = tdr & 0x03;
//SIMUL_Printf(processor,"LCD-Set-Bias Ratio %d (Soll=3)\n",lcd->ratio);				
				}
				else if((tdr & 0xFE) == 0xEE)
				{	//nxt_lcd_set_cursor_update(U32 on)
					lcd->cursor_upate = tdr & 1;
				}
				break;
			case LCD_POT:
				lcd->black = tdr & 0xFF;
				lcd->mode  = LCD_IDLE;
//SIMUL_Printf(processor,"LCD-Pot %x (Soll=0x60 Helligkeit)\n",lcd->pot);				
				break;
		}
	}
}

/**************************************************************************/
void	pio_changed(simulProcessor processor, simulPtr private,
						  simulWord PIO_ABSR, simulWord PIO_PSR ,
	                      simulWord PIO_ODSR, simulWord PIO_OSR,
						  simulWord PIO_MDSR, simulWord PIO_PPUSR,
						  simulWord PIO_PDSR)
{
	NXT *nxt = (NXT *)private;
	simulWord flag;
	simulWord pos;
	
	for(pos=0,flag=0x01;pos<32;pos++,flag<<=1)
	{
		PIO_MODE mode=PIO_UNDEF;
		
		if(~PIO_OSR & flag)
		{
			mode=PIO_INPUT;
		}
		else if(PIO_MDSR & flag)
		{	//OpenCollector Mode
			if(PIO_PSR & flag)
				mode=PIO_OUTPUT_OC_ODSR;
			else
			{
				if(PIO_ABSR & flag)
					mode=PIO_OUTPUT_OC_B;
				else
					mode=PIO_OUTPUT_OC_A;
			}
		}
		else
		{	//PushPull Mode
			if(PIO_PSR & flag)
				mode=PIO_OUTPUT_ODSR;
			else
			{
				if(PIO_ABSR & flag)
					mode=PIO_OUTPUT_B;
				else
					mode=PIO_OUTPUT_A;
			}
		}
		nxt->akt.pio_mode[pos]  = mode;
		nxt->akt.pio_state[pos] = PIO_PDSR & flag ? 1 : 0;
	}
	
	update(processor,private,0);
}						  

/**************************************************************************/

static int SIMULAPI NXT_TimerElapsed(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    NXT      *nxt = (NXT *)private;
	
	simulWord x;
	simulWord y;
	simulWord data;
	simulWord address;

	
	for(address=0;address<32;address++)
	{
		nxt->akt.pio_mode[address]  = PIO_UNDEF;
		nxt->akt.pio_state[address] = 0;
		nxt->old.pio_mode[address]  = 0xff;
		nxt->old.pio_state[address] = 0;
	}
	
	nxt->akt.nxt_avr_adc_1   = 0x0;
	nxt->akt.nxt_avr_adc_2   = 0x0;
	nxt->akt.nxt_avr_adc_3   = 0x0;
	nxt->akt.nxt_avr_adc_4   = 0x0;
	nxt->akt.nxt_avr_battery = 9.80/0.01384765625;
	nxt->akt.nxt_avr_button  = 0;
	nxt->akt.nxt_avr_power            = 0x0;		//Wird nicht wirklich genutzt
	nxt->akt.nxt_avr_pwm_frequency    = 0x0;		//Wird nur einmal gesetzt
	nxt->akt.nxt_avr_output_percentA  = 0x0;
	nxt->akt.nxt_avr_output_percentB  = 0x0;
	nxt->akt.nxt_avr_output_percentC  = 0x0;
	nxt->akt.nxt_avr_output_percentD  = 0x0;
	nxt->akt.nxt_avr_output_mode      = 0x0;
	nxt->akt.nxt_avr_input_power      = 0x0;

	nxt->old.nxt_avr_adc_1   = 0xFFFFFFFF;
	nxt->old.nxt_avr_adc_2   = 0xFFFFFFFF;
	nxt->old.nxt_avr_adc_3   = 0xFFFFFFFF;
	nxt->old.nxt_avr_adc_4   = 0xFFFFFFFF;
	nxt->old.nxt_avr_battery = 0xFFFFFFFF;
	nxt->old.nxt_avr_button  = 0xFFFFFFFF;
	nxt->old.nxt_avr_power            = 0xFFFFFFFF;
	nxt->old.nxt_avr_pwm_frequency    = 0xFFFFFFFF;
	nxt->old.nxt_avr_output_percentA  = 0xFFFFFFFF;
	nxt->old.nxt_avr_output_percentB  = 0xFFFFFFFF;
	nxt->old.nxt_avr_output_percentC  = 0xFFFFFFFF;
	nxt->old.nxt_avr_output_percentD  = 0xFFFFFFFF;
	nxt->old.nxt_avr_output_mode      = 0xFFFFFFFF;
	nxt->old.nxt_avr_input_power      = 0xFFFFFFFF;
		
	//Bildschirm zeichnen
	address=LCD_MEMORY_OFFSET;
	for(y=0;y<NXT_HIGH;y++)
	{
		for(x=0;x<NXT_WIDTH;x++)
		{
			if((y==0) || (y==11) || (y==NXT_HIGH-12) || (y==NXT_HIGH-1))
			{	//Oberste / Unterste Linie Scharz zeichnen
				data=0x00;  //Black
			}
			else if((y<11) || (y>NXT_HIGH-12))
			{
				//Seitliche Abrundungen der oberen Grauen Balken zeichnen
				if((x == 0) || (x > NXT_WIDTH-1))
					data=  0;    //LOG(1)=0
				else if((x == 1) || (x > NXT_WIDTH-2))
					data= 38;  //LOG(2)=0,301
				else if((x == 2) || (x > NXT_WIDTH-3))
					data=60;   //LOG(3)=0,477
				else if((x == 3) || (x > NXT_WIDTH-4))
					data=77;   //LOG(4)=0,602
				else if((x == 4) || (x > NXT_WIDTH-5))
					data=79;   //LOG(5)=0,698
				else if((x == 5) || (x > NXT_WIDTH-6))
					data=100;   //LOG(6) 0,7785
				else if((x == 6) || (x > NXT_WIDTH-7))
					data=108;   //LOG(7) 0,845
				else if((x == 7) || (x > NXT_WIDTH-8))
					data=115;   //LOG(8)=0,903
				else if((x == 8) || (x > NXT_WIDTH-9))
					data=122;   //LOG(9)=0,954
				else if((x == 9) || (x > NXT_WIDTH-10))
					data=128;   //LOG(10)=1
				else	
					data=128;  //Gray
			}
			else if(( y<NXT_HIGH*1/5                     ) ||
					((y>NXT_HIGH*2/5) && (y<NXT_HIGH*3/5)) ||
					(                     y>NXT_HIGH*4/5))
			{	//Seitliche Abrundungen zeichnen
				if((x == 0) || (x > NXT_WIDTH-1))
					data=0x00;   //LOG(1)=0
				else if((x == 1) || (x > NXT_WIDTH-2))
					data= 76;  //LOG(2)=0,301
				else if((x == 2) || (x > NXT_WIDTH-3))
					data=121;   //LOG(3)=0,477
				else if((x == 3) || (x > NXT_WIDTH-4))
					data=154;   //LOG(4)=0,602
				else if((x == 4) || (x > NXT_WIDTH-5))
					data=178;   //LOG(5)=0,698
				else if((x == 5) || (x > NXT_WIDTH-6))
					data=199;   //LOG(6) 0,7785
				else if((x == 6) || (x > NXT_WIDTH-7))
					data=216;   //LOG(7) 0,845
				else if((x == 7) || (x > NXT_WIDTH-8))
					data=231;   //LOG(8)=0,903
				else if((x == 8) || (x > NXT_WIDTH-9))
					data=244;   //LOG(9)=0,954
				else if((x == 9) || (x > NXT_WIDTH-10))
					data=255;   //LOG(10)=1
			}
			else if((x<9) || (x>NXT_WIDTH-10))
			{	//Abrundungen der Befestigungslöscher zeichnen
				if((y==NXT_HIGH*1/5+0) || (y==NXT_HIGH*2/5-0) || (y==NXT_HIGH*3/5+0) || (y==NXT_HIGH*4/5-0))
					data=0x00;   //LOG(1)=0
				else if((y==NXT_HIGH*1/5+1) || (y==NXT_HIGH*2/5-1) || (y==NXT_HIGH*3/5+1) || (y==NXT_HIGH*4/5-1))
					data= 76;  //LOG(2)=0,301
				else if((y==NXT_HIGH*1/5+2) || (y==NXT_HIGH*2/5-2) || (y==NXT_HIGH*3/5+2) || (y==NXT_HIGH*4/5-2))
					data=121;   //LOG(3)=0,477
				else if((y==NXT_HIGH*1/5+3) || (y==NXT_HIGH*2/5-3) || (y==NXT_HIGH*3/5+3) || (y==NXT_HIGH*4/5-3))
					data=154;   //LOG(4)=0,602
				else if((y==NXT_HIGH*1/5+4) || (y==NXT_HIGH*2/5-4) || (y==NXT_HIGH*3/5+4) || (y==NXT_HIGH*4/5-4))
					data=178;   //LOG(5)=0,698
				else if((y==NXT_HIGH*1/5+5) || (y==NXT_HIGH*2/5-5) || (y==NXT_HIGH*3/5+5) || (y==NXT_HIGH*4/5-5))
					data=199;   //LOG(6) 0,7785
				else if((y==NXT_HIGH*1/5+6) || (y==NXT_HIGH*2/5-6) || (y==NXT_HIGH*3/5+6) || (y==NXT_HIGH*4/5-6))
					data=216;   //LOG(7) 0,845
				else if((y==NXT_HIGH*1/5+7) || (y==NXT_HIGH*2/5-7) || (y==NXT_HIGH*3/5+7) || (y==NXT_HIGH*4/5-7))
					data=231;   //LOG(8)=0,903
				else if((y==NXT_HIGH*1/5+8) || (y==NXT_HIGH*2/5-8) || (y==NXT_HIGH*3/5+8) || (y==NXT_HIGH*4/5-8))
					data=244;   //LOG(9)=0,954
				else if((y==NXT_HIGH*1/5+9) || (y==NXT_HIGH*2/5-9) || (y==NXT_HIGH*3/5+9) || (y==NXT_HIGH*4/5-9))
					data=255;   //LOG(10)=1
			}
			else
				data=255;
		  
			SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			address++;
		}	
	}
	
	//LCD Rahmen zeichnen
	data=0x80;
	for(x=LCD_X-1;x<LCD_X+LCD_WIDTH+1;x++)
	{
		address=LCD_MEMORY_OFFSET+(LCD_Y-1)*NXT_WIDTH+x;
		SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
		address=LCD_MEMORY_OFFSET+(LCD_Y+LCD_HIGH)*NXT_WIDTH+x;
		SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
	}
	
	for(y=LCD_Y-1;y<LCD_Y+LCD_HIGH+1;y++)
	{
		address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+LCD_X-1;
		SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
		address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+LCD_X+LCD_WIDTH;
		SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
	}
		
	display_char(processor,'A',NXT_WIDTH*1/5,         9,-1,0xC0);
	display_char(processor,'B',NXT_WIDTH*2/5,         9,-1,0xC0);
	display_char(processor,'C',NXT_WIDTH*3/5,         9,-1,0xC0);
	display_char(processor,'U',NXT_WIDTH*4/5-6,       9,-1,0xC0);
	display_char(processor,'S',NXT_WIDTH*4/5+0,       9,-1,0xC0);
	display_char(processor,'B',NXT_WIDTH*4/5+6,       9,-1,0xC0);
	display_char(processor,'1',NXT_WIDTH*1/5,NXT_HIGH-3,-1,0xC0);
	display_char(processor,'2',NXT_WIDTH*2/5,NXT_HIGH-3,-1,0xC0);
	display_char(processor,'3',NXT_WIDTH*3/5,NXT_HIGH-3,-1,0xC0);
	display_char(processor,'4',NXT_WIDTH*4/5,NXT_HIGH-3,-1,0xC0);

	update(processor,private,0);

	SIMUL_StopTimer(processor, nxt->timerid);
}

static void	update(simulProcessor processor, simulPtr private, int mode)
{
	NXT      *nxt = (NXT *) private;
	simulWord x;
	simulWord y;
	simulWord data;
	simulWord address;	

	if(mode == 1)
	{
		if(nxt->akt.nxt_avr_button & BUTTON_MITTE)
			nxt->akt.nxt_avr_button -= (0x01010101 & BUTTON_MITTE);
		if(nxt->akt.nxt_avr_button & BUTTON_UNTEN)
			nxt->akt.nxt_avr_button -= (0x01010101 & BUTTON_UNTEN);
		if(nxt->akt.nxt_avr_button & BUTTON_LEFT)
			nxt->akt.nxt_avr_button -= (0x01010101 & BUTTON_LEFT);
		if(nxt->akt.nxt_avr_button & BUTTON_RIGHT)
			nxt->akt.nxt_avr_button -= (0x01010101 & BUTTON_RIGHT);
	}

	if(nxt->old.nxt_avr_output_percentA != nxt->akt.nxt_avr_output_percentA)
	{
		display_printf(processor,NXT_WIDTH*1/5-10,20,255, 0,"%4d%c",nxt->akt.nxt_avr_output_percentA,nxt->akt.nxt_avr_output_mode&0x01?'B':' ');
		nxt->old.nxt_avr_output_percentA = nxt->akt.nxt_avr_output_percentA;
	}
	if(nxt->old.nxt_avr_output_percentB != nxt->akt.nxt_avr_output_percentB)
	{
		display_printf(processor,NXT_WIDTH*2/5-10,20,255, 0,"%4d%c",nxt->akt.nxt_avr_output_percentB,nxt->akt.nxt_avr_output_mode&0x02?'B':' ');
		nxt->old.nxt_avr_output_percentB = nxt->akt.nxt_avr_output_percentB;
	}
	if(nxt->old.nxt_avr_output_percentC != nxt->akt.nxt_avr_output_percentC)
	{
		display_printf(processor,NXT_WIDTH*3/5-10,20,255, 0,"%4d%c",nxt->akt.nxt_avr_output_percentC,nxt->akt.nxt_avr_output_mode&0x04?'B':' ');
		nxt->old.nxt_avr_output_percentC = nxt->akt.nxt_avr_output_percentC;
	}

	if(nxt->old.nxt_avr_input_power != nxt->akt.nxt_avr_input_power)
	{
		display_printf(processor,NXT_WIDTH*1/5-5,NXT_HIGH-33,255, 0,"%s",
		               (nxt->akt.nxt_avr_input_power & 0x11) == 0x00 ? " 0V ":
					   (nxt->akt.nxt_avr_input_power & 0x11) == 0x01 ? "9VPu":
					   (nxt->akt.nxt_avr_input_power & 0x11) == 0x10 ? " 9V ":"----");
		display_printf(processor,NXT_WIDTH*2/5-5,NXT_HIGH-33,255, 0,"%s",
		               (nxt->akt.nxt_avr_input_power & 0x22) == 0x00 ? " 0V ":
					   (nxt->akt.nxt_avr_input_power & 0x22) == 0x02 ? "9VPu":
					   (nxt->akt.nxt_avr_input_power & 0x22) == 0x20 ? " 9V ":"----");
		display_printf(processor,NXT_WIDTH*3/5-5,NXT_HIGH-33,255, 0,"%s",
		               (nxt->akt.nxt_avr_input_power & 0x44) == 0x00 ? " 0V ":
					   (nxt->akt.nxt_avr_input_power & 0x44) == 0x04 ? "9VPu":
					   (nxt->akt.nxt_avr_input_power & 0x44) == 0x40 ? " 9V ":"----");
		display_printf(processor,NXT_WIDTH*4/5-5,NXT_HIGH-33,255, 0,"%s",
		               (nxt->akt.nxt_avr_input_power & 0x88) == 0x00 ? " 0V ":
					   (nxt->akt.nxt_avr_input_power & 0x88) == 0x08 ? "9VPu":
					   (nxt->akt.nxt_avr_input_power & 0x88) == 0x80 ? " 9V ":"----");
		nxt->old.nxt_avr_input_power = nxt->akt.nxt_avr_input_power;
	}

	if(nxt->old.nxt_avr_adc_1 != nxt->akt.nxt_avr_adc_1)
	{
		display_printf(processor,NXT_WIDTH*1/5-5,NXT_HIGH-23,255, 0,"%03x",nxt->akt.nxt_avr_adc_1);
		nxt->old.nxt_avr_adc_1=nxt->akt.nxt_avr_adc_1;
	}
	if(nxt->old.nxt_avr_adc_2 != nxt->akt.nxt_avr_adc_2)
	{
		display_printf(processor,NXT_WIDTH*2/5-5,NXT_HIGH-23,255, 0,"%03x",nxt->akt.nxt_avr_adc_2);
		nxt->old.nxt_avr_adc_2=nxt->akt.nxt_avr_adc_2;
	}
	if(nxt->old.nxt_avr_adc_3 != nxt->akt.nxt_avr_adc_3)
	{
		display_printf(processor,NXT_WIDTH*3/5-5,NXT_HIGH-23,255, 0,"%03x",nxt->akt.nxt_avr_adc_3);
		nxt->old.nxt_avr_adc_3 = nxt->akt.nxt_avr_adc_3;
	}
	if(nxt->old.nxt_avr_adc_4 != nxt->akt.nxt_avr_adc_4)
	{
		display_printf(processor,NXT_WIDTH*4/5-5,NXT_HIGH-23,255, 0,"%03x",nxt->akt.nxt_avr_adc_4);
		nxt->old.nxt_avr_adc_4 = nxt->akt.nxt_avr_adc_4;
	}

	if((nxt->old.pio_mode [PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_mode [PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET]))
	   {
		display_printf(processor,NXT_WIDTH*1/5-10,NXT_HIGH-13,255, 0,"%c%c %c%c",
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET]? '1':'0',
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET]? '1':'0');
		nxt->old.pio_mode [PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGIA0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_mode [PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGIA1-PIO_PORTPIN_OFFSET];
					   
	   }
	if((nxt->old.pio_mode [PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_mode [PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET]))
	   {
		display_printf(processor,NXT_WIDTH*2/5-10,NXT_HIGH-13,255, 0,"%c%c %c%c",
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET]? '1':'0',
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET]? '1':'0');
		nxt->old.pio_mode [PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGIB0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_mode [PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGIB1-PIO_PORTPIN_OFFSET];
					   
	   }
	if((nxt->old.pio_mode [PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_mode [PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET]))
	   {
		display_printf(processor,NXT_WIDTH*3/5-10,NXT_HIGH-13,255, 0,"%c%c %c%c",
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET]? '1':'0',
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET]? '1':'0');
		nxt->old.pio_mode [PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGIC0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_mode [PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGIC1-PIO_PORTPIN_OFFSET];
	   }
	if((nxt->old.pio_mode [PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_mode [PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_mode [PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET]) ||
	   (nxt->old.pio_state[PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET] != nxt->akt.pio_state[PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET]))
	   {
		display_printf(processor,NXT_WIDTH*4/5-10,NXT_HIGH-13,255, 0,"%c%c %c%c",
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET]? '1':'0',
					   PIO_MODE_CHAR[nxt->akt.pio_mode [PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET]],
					                 nxt->akt.pio_state[PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET]? '1':'0');
		nxt->old.pio_mode [PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGID0-PIO_PORTPIN_OFFSET];
		nxt->old.pio_mode [PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_mode [PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET];
		nxt->old.pio_state[PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET] = nxt->akt.pio_state[PIO_PORT_DIGID1-PIO_PORTPIN_OFFSET];
	   }
	
//	if(nxt->old.nxt_avr_battery != nxt->akt.nxt_avr_battery)
//	{
//		display_printf(processor,20,NXT_HIGH-50,255, 0,"Bat: %2.1fV %03x",nxt->akt.nxt_avr_battery*0.01384765625,nxt->akt.nxt_avr_battery);
//		nxt->old.nxt_avr_battery = nxt->akt.nxt_avr_battery;
//	}

	if((nxt->old.nxt_avr_button & BUTTON_MITTE) != (nxt->akt.nxt_avr_button & BUTTON_MITTE))
	{	//ENTER Button Zeichnen (Mitte)
		for(y=KEY_Y-KEY_HIGH/2;y<=KEY_Y+KEY_HIGH/2;y++)
			for(x=KEY_X-KEY_WIDTH/2;x<=KEY_X+KEY_WIDTH/2;x++)
			{
				if((y==KEY_Y-KEY_HIGH/2)  || (y==KEY_Y+KEY_HIGH/2) ||
				   (x==KEY_X-KEY_WIDTH/2) || (x==KEY_X+KEY_WIDTH/2))
					data=0x20;
				else
					data=nxt->akt.nxt_avr_button & BUTTON_MITTE ? 100 : 200;
				address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+x;
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			}
		nxt->old.nxt_avr_button = (nxt->old.nxt_avr_button & ~BUTTON_MITTE) | 
		                          (nxt->akt.nxt_avr_button &  BUTTON_MITTE);
	}		
		
	if((nxt->old.nxt_avr_button & BUTTON_UNTEN) != (nxt->akt.nxt_avr_button & BUTTON_UNTEN))
	{	//ESC button Zeichnen (Unten)
		for(y=KEY_Y+KEY_HIGH;y<=KEY_Y+KEY_HIGH*7/4;y++)
			for(x=KEY_X-KEY_WIDTH/2;x<=KEY_X+KEY_WIDTH/2;x++)
			{
				if((y==KEY_Y+KEY_HIGH)  || (y==KEY_Y+KEY_HIGH*7/4) ||
				   (x==KEY_X-KEY_WIDTH/2) || (x==KEY_X+KEY_WIDTH/2))
					data=0x20;
				else
					data=nxt->akt.nxt_avr_button & BUTTON_UNTEN ? 40 : 80;
				address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+x;
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			}
		nxt->old.nxt_avr_button = (nxt->old.nxt_avr_button & ~BUTTON_UNTEN) | 
		                          (nxt->akt.nxt_avr_button &  BUTTON_UNTEN);
	}
	
	if((nxt->old.nxt_avr_button & BUTTON_RIGHT) != (nxt->akt.nxt_avr_button & BUTTON_RIGHT))
	{	//RIGHT Button Zeichnen (Rechts)
		data=nxt->akt.nxt_avr_button & BUTTON_RIGHT ? 100 : 200;
		for(y=KEY_Y-KEY_HIGH/2;y<=KEY_Y;y++)
			for(x=KEY_X+KEY_WIDTH;x<=KEY_X+KEY_WIDTH+(y-(KEY_Y-KEY_HIGH/2))*1.5;x++)
			{
				address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+x;
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			}
		for(y=KEY_Y+KEY_HIGH/2;y>KEY_Y;y--)
			for(x=KEY_X+KEY_WIDTH;x<=KEY_X+KEY_WIDTH+((KEY_Y+KEY_HIGH/2)-y)*1.5;x++)
			{
				address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+x;
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			}
		nxt->old.nxt_avr_button = (nxt->old.nxt_avr_button & ~BUTTON_RIGHT) | 
		                          (nxt->akt.nxt_avr_button &  BUTTON_RIGHT);
	}
	
	if((nxt->old.nxt_avr_button & BUTTON_LEFT) != (nxt->akt.nxt_avr_button & BUTTON_LEFT))
	{	//LEFT Button Zeichnen (Links)
		data=nxt->akt.nxt_avr_button & BUTTON_LEFT ? 100 : 200;
		for(y=KEY_Y-KEY_HIGH/2;y<=KEY_Y;y++)
			for(x=KEY_X-KEY_WIDTH;x>=KEY_X-KEY_WIDTH-(y-(KEY_Y-KEY_HIGH/2))*1.5;x--)
			{
				address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+x;
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			}
		for(y=KEY_Y+KEY_HIGH/2;y>KEY_Y;y--)
			for(x=KEY_X-KEY_WIDTH;x>=KEY_X-KEY_WIDTH-((KEY_Y+KEY_HIGH/2)-y)*1.5;x--)
			{
				address=LCD_MEMORY_OFFSET+y*NXT_WIDTH+x;
				SIMUL_WriteMemory(processor, 0, &address, 1*8, SIMUL_MEMORY_HIDDEN, &data);
			}
		nxt->old.nxt_avr_button = (nxt->old.nxt_avr_button & ~BUTTON_LEFT) | 
		                          (nxt->akt.nxt_avr_button &  BUTTON_LEFT);
	}
}

/**************************************************************************/

static int SIMULAPI NXT_PortWrite(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	NXT *nxt = (NXT *) private;

//SIMUL_Printf(processor,"NXT_PortWrite() %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
	
	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_1))
	{   
		nxt->akt.nxt_avr_adc_1 = cbs->x.bus.data & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_2))
	{   
		nxt->akt.nxt_avr_adc_2 = cbs->x.bus.data & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_3))
	{   
		nxt->akt.nxt_avr_adc_3 = cbs->x.bus.data & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_4))
	{   
		nxt->akt.nxt_avr_adc_4 = cbs->x.bus.data & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_BATTERY))
	{   
		nxt->akt.nxt_avr_battery = cbs->x.bus.data & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_BUTTON))
	{   
		nxt->akt.nxt_avr_button = cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_BUTTON_STATE))
	{   
		if(cbs->x.bus.data & BUTTON_MITTE)
			nxt->akt.nxt_avr_button |= (BUTTON_MITTE & 0xFFFFFFFF);
		if(cbs->x.bus.data & BUTTON_UNTEN)
			nxt->akt.nxt_avr_button |= (BUTTON_UNTEN   & 0xFFFFFFFF);
		if(cbs->x.bus.data & BUTTON_LEFT)
			nxt->akt.nxt_avr_button |= (BUTTON_LEFT  & 0xFFFFFFFF);
		if(cbs->x.bus.data & BUTTON_RIGHT)
			nxt->akt.nxt_avr_button |= (BUTTON_RIGHT & 0xFFFFFFFF);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_POWER))
	{   
		nxt->akt.nxt_avr_power = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_PWM_FREQUENCY))
	{   
		nxt->akt.nxt_avr_pwm_frequency = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTA))
	{   
		nxt->akt.nxt_avr_output_percentA = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTB))
	{   
		nxt->akt.nxt_avr_output_percentB = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTC))
	{   
		nxt->akt.nxt_avr_output_percentC = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTD))
	{   
		nxt->akt.nxt_avr_output_percentD = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_MODE))
	{   
		nxt->akt.nxt_avr_output_mode = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_INPUT_POWER))
	{   
		nxt->akt.nxt_avr_input_power = cbs->x.bus.data & 0xff;
	}
	else if((cbs->x.bus.width == 4*8)            && 
	       ((cbs->x.bus.address & 0x03) == 0)    &&
	        (cbs->x.bus.address >= NXT_PIO_PA0 ) &&
	        (cbs->x.bus.address <= NXT_PIO_PA31))
	{
		simulWord PAx=(cbs->x.bus.address-NXT_PIO_PA0)/4;
		simulWord data;
		simulWord address;
		
		switch(nxt->akt.pio_mode[PAx])
		{	
			case PIO_INPUT: 
				data = cbs->x.bus.data & 0x10 ? 1 : 0;
				SIMUL_SetPort(processor, PIO_PORTPIN_OFFSET + PAx, 1, &data);
				break;
			case PIO_OUTPUT_ODSR:
			case PIO_OUTPUT_OC_ODSR:
				address=(simulWord)(cbs->x.bus.data & 0x10 ? AT91C_PIOA_SODR : AT91C_PIOA_CODR);
				data   =1<<PAx;
				SIMUL_WriteMemory(processor, 0, &address, 4*8, SIMUL_MEMORY_HIDDEN, &data);
				break;
			case PIO_OUTPUT_A:
			case PIO_OUTPUT_OC_A:
				data = cbs->x.bus.data & 0x10 ? 1 : 0;
				SIMUL_SetPort(processor, PIO_PORTPA_OFFSET + PAx, 1, &data);
				break;
			case PIO_OUTPUT_B:
			case PIO_OUTPUT_OC_B:
				data = cbs->x.bus.data & 0x10 ? 1 : 0;
				SIMUL_SetPort(processor, PIO_PORTPB_OFFSET + PAx, 1, &data);
				break;
		}
	}
	else
	{
SIMUL_Printf(processor,"PIO_PortWrite(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}

	update(processor,private,0);
	return SIMUL_MEMORY_OK;
}

static int SIMULAPI NXT_PortRead(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	NXT *nxt = (NXT *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_1))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_adc_1 & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_2))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_adc_2 & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_3))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_adc_3 & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_ADC_4))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_adc_4 & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_BATTERY))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_battery & 0x3ff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_BUTTON))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_button;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_BUTTON_STATE))
	{   
		cbs->x.bus.data=0;
		if(nxt->akt.nxt_avr_button & BUTTON_MITTE)
			cbs->x.bus.data |= BUTTON_MITTE;
		if(nxt->akt.nxt_avr_button & BUTTON_UNTEN)
			cbs->x.bus.data |= BUTTON_UNTEN;
		if(nxt->akt.nxt_avr_button & BUTTON_LEFT)
			cbs->x.bus.data |= BUTTON_LEFT;
		if(nxt->akt.nxt_avr_button & BUTTON_RIGHT)
			cbs->x.bus.data |= BUTTON_RIGHT;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_POWER))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_power & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_PWM_FREQUENCY))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_pwm_frequency & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTA))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_output_percentA & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTB))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_output_percentB & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTC))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_output_percentC & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_PERCENTD))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_output_percentD & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_OUTPUT_MODE))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_output_mode & 0xff;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) NXT_AVR_INPUT_POWER))
	{   
		cbs->x.bus.data=nxt->akt.nxt_avr_input_power & 0xff;
	}
	else if((cbs->x.bus.width == 4*8)            && 
	       ((cbs->x.bus.address & 0x03) == 0)    &&
	        (cbs->x.bus.address >= NXT_PIO_PA0 ) &&
	        (cbs->x.bus.address <= NXT_PIO_PA31))
	{
		simulWord PAx=(cbs->x.bus.address-NXT_PIO_PA0)/4;
		cbs->x.bus.data = (nxt->akt.pio_mode[PAx] & 0x0f) | ((nxt->akt.pio_state[PAx]<<4)&0x10);
	}
	else
	{
SIMUL_Printf(processor,"NXT_PortRead(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}
	
	return SIMUL_MEMORY_OK;
}

static int SIMULAPI NXT_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    NXT *nxt = (NXT *) private;
	
	simulTime clocks = 1;
	SIMUL_StartTimer(processor, nxt->timerid, 
								SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
								&clocks);
	
}

/**************************************************************************/

void *NXT_Init    (simulProcessor processor)
{
	NXT		       *nxt;
    simulWord       from, to;
	
	nxt = (NXT *)SIMUL_Alloc(processor, sizeof(NXT));
	
	SIMUL_Printf(processor,"-> Peripherie NXT loaded (ohne Gewähr, nicht vollständig getestet)\n");

    SIMUL_RegisterResetCallback(processor, NXT_PortReset, (simulPtr) nxt);
	
    from = (simulWord) NXT_MEMORY_OFFSET;
    to   = (simulWord) NXT_MEMORY_OFFSET+NXT_MEMORY_SIZE-1;
    SIMUL_RegisterBusWriteCallback(processor, NXT_PortWrite, (simulPtr) nxt, 0, &from, &to);
    SIMUL_RegisterBusReadCallback (processor, NXT_PortRead,  (simulPtr) nxt, 0, &from, &to);

	avr_init(processor,nxt);
	lcd_init(processor,nxt);

	//Initialisierung erfolgt über TimerElaplsed, also in NXT_TimerElapsed
    nxt->timerid = SIMUL_RegisterTimerCallback(processor, NXT_TimerElapsed, (simulPtr) nxt);
	simulTime clocks = 1;
	SIMUL_StartTimer(processor, nxt->timerid, 
								SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
								&clocks);
								
	return(nxt);
}
