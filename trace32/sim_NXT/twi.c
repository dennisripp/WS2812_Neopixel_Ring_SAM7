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
 TWI Two-wire Interface (für SAM7S512/256/128/64/321/32)
 Nicht implementiert:
 - keine Berücksichtiugng der PIO Signale
 - keine Berücksichtigung von ACK/NACK im Datentransfer
 - keine Berücksichtigung des Power Managements (und damit der Taktrate)
ToDo: 

Fragen:
 - Der ResetZustand von TWI_SR = 0x8, wobei es an dieser Stelle kein Bit gibt
**************************************************************************/

#include "simul.h"
#include "AT91SAM7S64.h"
#include "sim_NXT.h"
#include <stddef.h>

/**************************************************************************

	Local port structure
	
**************************************************************************/

typedef enum
{
	SERIALIZER_RESET,
	SERIALIZER_OFF,
	//--- Reihenfolge nicht ändern
	SERIALIZER_MS_WRIADR3,
	SERIALIZER_MS_WRIADR2,
	SERIALIZER_MS_WRIADR1,
	SERIALIZER_MS_WRDATA,
	//--- Reihenfolge nicht ändern
	SERIALIZER_MS_RDDATA,
	SERIALIZER_MS_RDIADR3,
	SERIALIZER_MS_RDIADR2,
	SERIALIZER_MS_RDIADR1
} SERIALIZER_MODE;

typedef struct
{
    void           *timerid;
	SERIALIZER_MODE mode;
	simulWord       thr;
	simulWord       rhr;
	simulWord       dadr;
	simulWord       iadr;
	simulWord       byte;
} SERIALIZER;

typedef struct
{
	void        *nxt;
	AT91S_TWI   at91s_twi;
	SERIALIZER  serializer;
	simulWord   debug;
	simulWord   MSEN;
} TWI;

#define     CLOCK_CALCULATE(CWGR)	( (((((CWGR & AT91C_TWI_CLDIV) >> 0) * (1 << ((CWGR & AT91C_TWI_CKDIV) >> 16))) + 3) + \
                                       ((((CWGR & AT91C_TWI_CHDIV) >> 8) * (1 << ((CWGR & AT91C_TWI_CKDIV) >> 16))) + 3))*9)

static int SIMULAPI TWI_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private);

/**************************************************************************
Two-Wire Interface (TWI)
***************************************************************************

TWI_CR:   TWI Control Register               Offset 0x00  Write-Only ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| SWRST  |        |        |        | MSDIS  | MSEN   | STOP   | START  |
+--------+--------+--------+--------+--------+--------+--------+--------+
Start: Send a START Condition
- 0 = No effect
- 1 = A frame beginning with a START bit is transmitted according to the
      features defined in the mode register
	  This action is necessary when the TWI peripheral wants to read data
	  from a slave. When configured in Master Mode with a write operation,
	  a frame is sent as soon as the user writes a character in the TWI_THR
STOP: Send a STOP Condition
- 0 = No effect
- 1 = STOP Condition is sent juster after completing the current byte
      transmission in master read mode
	  - In single data byte master read, the START and STOP must both be sent
	  - In multiple data bytes master read, the STOP must be set after
	    the last data received but one.
      - In master read mode, if a NACK bit is received, the STOP is 
	    automatically performed
	  - In multiple data write operation, when both THR and shift register are
	    empty, a STOP condition is automatically sent
MSEN: TWI Master Transfer Enabled
- 0 = No effect
- 1 = If MSDIS = 1, the master data transfer is enabled
MSDIS: TWI Master Transfer Disabled
- 0 = No effect
- 1 = The master data transfer is disabled, all pending data is transmitted. The
      shifter and holding characters (if they contain data) are transmitted in
	  case of write operation. In read operation, the character bein transfered
	  must be completely received before disabling
SWRST: Software Reset
- 0 = No effect
- 1 = Equivalent to a system reset

TWI_MMR:  TWI Master Mode Register           Offset 0x04  Read/Write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |                           DADR                               |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        | MREAD  |        |        |   IADRSZ        |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
IADRSZ: Internal Device Address Size
- IADRSZ 9:8
         0 0  No internal device address (Byte command protocol)
		 0 1  One-byte internal device address
		 1 0  Two-byte internal device address
		 1 1  Three-byte internal device address
MREAD: Master Read Direction
- 0 = Master write direction
- 1 = Master read direction
DADR: Device Address
- the device address is used to access slave devices in read or write mode.


TWI_IADR: TWI Internal Address Register      Offset 0x0C  Read/write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  IADR                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  IADR                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  IADR                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
IADr: Internal Address
0,1,2 or 3 bytes depending on IADRSZ: 
  - Low significant byte address in 10-bit mode addresses
  

TWI_CWGR: TWI Clock Waveform Generator Reg   Offset 0x10  Read/Write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |          CKDIV           |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  CHDIV                                |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 CLDIV                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
CLDIV: Clock Low Divider
- The SCL low period is defined as follows: 
  Tlow = ((CLDIV * 2^CKDIV)+3)*TMCK
  
CHDIV: Clock High Divider
- The SCL high period is defined as follows:
  Thigh = ((CHDIV * 2 CKDIV)+3)*TMCK
  
CKDIV: Clock Divider
- The CKDIV is used to increase both SCL high and low periods
  

TWI_SR:   TWI Status Register                Offset 0x20  Read-Only  0x0000_0008
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |  NACK  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        | TXRDY  | RXRDY  |TXCOMP  |
+--------+--------+--------+--------+--------+--------+--------+--------+
TXCOMP: Transmission Completed 
- 0 = During the length of the current frame
- 1 = When both holding and shift registers are empty and STOP condition has
      been sent, or when MSEN is set (enable TWI)

RXRDY: Receive Holding Register Ready
- 0 = No character has been received since the last TWI_RHR read operation
- 1 = A byte has been received in the TWI_RHR since the last read

TXRDY: Transmit Holding Register Ready
- 0 The transmit holding register has not been transfered into the shift register.
    Set to 0 when writing into TWI_THR register
- 1 As soon as data byte is transferred from TWI_THR to internal shifter or if a
    NACK error is detected, TXRDY is set at the same time as TXCOMP and NACK.
	TXRDY is also set when MSEN is set (enable TWI)

NACK: Not Acknowledged
- 0 = Each data byte has been correctly received by the far-end side TWI slave component
- 1 = A data byte has not been acknowledged by the slave component. Set at the
      same time as TXCOMP. Reset after read

TWI_IER:  TWI Interrupt Enable Register      Offset 0x24  Write-Only ---
TWI_IDR:  TWI Interrupt Disable Register     Offset 0x28  Write-Only ---
TWI_IMR:  TWI Interrupt Mask Register        Offset 0x2C  Read-Only  0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |  NACK  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        | TXRDY  | RXRDY  |TXCOMP  |
+--------+--------+--------+--------+--------+--------+--------+--------+
TXCOMP: Transmission    Completed       Enable / Disable / Mode
RXRDY:  Receive Holding Register Ready  Enable / Disable / Mode
TXRDY:  Transmit Holding Register Ready Enable / Disable / Mode
NACK:   Not Acknowledge                 Enable / Disable / Mode



TWI_RHR:  TWI Receive Holding Register       Offset 0x30  Read-Only  0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                RXDATA                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
RXDATA: Receive Holding Data

TWI_THR:  TWI Transmit Holding Register      Offset 0x34  Read/Write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                TXDATA                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
TXDATA: Transmit Holding Data
	  
**************************************************************************/

static void interrupt_check(simulProcessor processor,simulPtr private)
{
	TWI *twi = (TWI *) private;
	
	if(twi->at91s_twi.TWI_SR & twi->at91s_twi.TWI_IMR & (AT91C_TWI_TXCOMP | AT91C_TWI_RXRDY |
		                                                 AT91C_TWI_TXRDY  | AT91C_TWI_OVRE  |
													     AT91C_TWI_UNRE   | AT91C_TWI_NACK))
	{
		simulWord data = 1;
		SIMUL_SetPort(processor, AIC_PORTIRQ_TWI, 1, &data);
	}
	else
	{
		simulWord data = 0;
		SIMUL_SetPort(processor, AIC_PORTIRQ_TWI, 1, &data);
	}
}


/**************************************************************************/
static void serializer_idle(simulProcessor processor, simulPtr private)
{
    TWI      *twi = (TWI *)private;
	simulTime clocks;
	
	if(!twi->serializer.timerid)
	{
		SIMUL_Warning(processor,"Error-TWI: twi->serializer.timerid == 0\n");
		return;
	}

	if((twi->serializer.mode == SERIALIZER_OFF) ||
	   (twi->serializer.mode == SERIALIZER_RESET))
	{
		if(twi->MSEN)  
		{	//Master Mode Enabled
			if(((twi->at91s_twi.TWI_SR  & AT91C_TWI_TXRDY) == 0) &&
			   ((twi->at91s_twi.TWI_MMR & AT91C_TWI_MREAD) == 0) )
			{	//New Byte in THR available -> Start Write
				twi->at91s_twi.TWI_SR  &= ~AT91C_TWI_TXCOMP;
				
				twi->serializer.dadr = (twi->at91s_twi.TWI_MMR & AT91C_TWI_DADR) >> 16;
				twi->serializer.byte = 0;
				
				switch(twi->at91s_twi.TWI_MMR & AT91C_TWI_IADRSZ)
				{
					case AT91C_TWI_IADRSZ_NO:
						twi->serializer.mode=SERIALIZER_MS_WRDATA;
						twi->serializer.iadr = 0;
						break;
					case AT91C_TWI_IADRSZ_1_BYTE:
						twi->serializer.mode=SERIALIZER_MS_WRIADR1;
						twi->serializer.iadr = twi->at91s_twi.TWI_IADR & 0xFF;
						break;
					case AT91C_TWI_IADRSZ_2_BYTE:
						twi->serializer.mode=SERIALIZER_MS_WRIADR2;
						twi->serializer.iadr = twi->at91s_twi.TWI_IADR & 0xFFFF;
						break;
					case AT91C_TWI_IADRSZ_3_BYTE:
						twi->serializer.mode=SERIALIZER_MS_WRIADR3;
						twi->serializer.iadr = twi->at91s_twi.TWI_IADR & 0xFFFFFF;
						break;
				}

				clocks= CLOCK_CALCULATE(twi->at91s_twi.TWI_CWGR);
				SIMUL_StartTimer(processor, twi->serializer.timerid, 
										SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
										&clocks);
			}
			if((twi->at91s_twi.TWI_MMR & AT91C_TWI_MREAD) &&
			   (twi->at91s_twi.TWI_CR  & AT91C_TWI_START))
			{	//Start Read
				twi->at91s_twi.TWI_CR  &= ~AT91C_TWI_START;
				twi->at91s_twi.TWI_SR  &= ~AT91C_TWI_TXCOMP;

				twi->serializer.dadr = (twi->at91s_twi.TWI_MMR & AT91C_TWI_DADR) >> 16;
				twi->serializer.byte = 0;
				
				switch(twi->at91s_twi.TWI_MMR & AT91C_TWI_IADRSZ)
				{
					case AT91C_TWI_IADRSZ_NO:
						twi->serializer.mode=SERIALIZER_MS_RDDATA;
						twi->serializer.iadr = 0;
						break;
					case AT91C_TWI_IADRSZ_1_BYTE:
						twi->serializer.mode=SERIALIZER_MS_RDIADR1;
						twi->serializer.iadr = twi->at91s_twi.TWI_IADR & 0xFF;
						break;
					case AT91C_TWI_IADRSZ_2_BYTE:
						twi->serializer.mode=SERIALIZER_MS_RDIADR2;
						twi->serializer.iadr = twi->at91s_twi.TWI_IADR & 0xFFFF;
						break;
					case AT91C_TWI_IADRSZ_3_BYTE:
						twi->serializer.mode=SERIALIZER_MS_RDIADR3;
						twi->serializer.iadr = twi->at91s_twi.TWI_IADR & 0xFFFFFF;
						break;
				}

				clocks= CLOCK_CALCULATE(twi->at91s_twi.TWI_CWGR);
				SIMUL_StartTimer(processor, twi->serializer.timerid, 
										SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
										&clocks);
			}
		}	
	}
}

static int SIMULAPI TWI_TimerElapsed(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    TWI      *twi = (TWI *)private;
	simulTime clocks;
	
	//28.6.3.2 Flow Master Mode Flow Diagram
	switch(twi->serializer.mode)
	{
		case SERIALIZER_RESET:  //OFF durch SoftwareAnforderung (AT91C_TWI_SWRST)
			twi->serializer.mode = SERIALIZER_OFF;
			SIMUL_StopTimer(processor, twi->serializer.timerid);
			break;
		case SERIALIZER_OFF:	//OFF Selbstständig nach Übertragung des letzten Bytes
							    //In diesem Zustand sollte somit nie auftreten können
			break;
		case SERIALIZER_MS_WRIADR1:
		case SERIALIZER_MS_WRIADR2:
		case SERIALIZER_MS_WRIADR3:
			//Acknowlege des vorherigen Datentransfers
			if(/*NACk*/0)
			{
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_NACK;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXCOMP;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXRDY;
				//Ende des Datentransfers ebenfalls noch einleiten
			}
			else
				twi->at91s_twi.TWI_SR &= ~AT91C_TWI_NACK;

			twi->serializer.mode++;
			
			clocks= CLOCK_CALCULATE(twi->at91s_twi.TWI_CWGR);
			SIMUL_StartTimer(processor, twi->serializer.timerid, 
									SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
									&clocks);
			
			break;
		case SERIALIZER_MS_WRDATA:
			//Acknowlege des vorherigen Datentransfers
			if(/*NACk*/0)
			{
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_NACK;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXCOMP;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXRDY;
				//Ende des Datentransfers ebenfalls noch einleiten
			}
			else
				twi->at91s_twi.TWI_SR &= ~AT91C_TWI_NACK;

			//Gesendetes Byte kommt erst am Ende der Übertragung beim Empfänger an
			if(twi->serializer.byte != 0)
			{
				avr_toavr(processor, twi->nxt, twi->serializer.dadr, twi->serializer.iadr, 
					         				   twi->serializer.byte, twi->serializer.thr);
			}
			
			if(twi->at91s_twi.TWI_SR & AT91C_TWI_TXRDY)
			{	//Keine neuen Daten zu senden
				twi->at91s_twi.TWI_SR |= AT91C_TWI_TXCOMP;
				twi->serializer.mode=SERIALIZER_OFF;
				SIMUL_StopTimer(processor, twi->serializer.timerid);
			}
			else
			{	//Neue Daten zum senden vorhanden
				twi->serializer.byte++;
				twi->serializer.thr = twi->at91s_twi.TWI_THR;
				twi->at91s_twi.TWI_SR |= AT91C_TWI_TXRDY;
				
				clocks= CLOCK_CALCULATE(twi->at91s_twi.TWI_CWGR);
				SIMUL_StartTimer(processor, twi->serializer.timerid, 
										SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
										&clocks);
			}							
			break;
		case SERIALIZER_MS_RDIADR1:
		case SERIALIZER_MS_RDIADR2:
		case SERIALIZER_MS_RDIADR3:
			//Acknowlege des vorherigen Datentransfers
			if(/*NACk*/0)
			{
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_NACK;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXCOMP;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXRDY;
				//Ende des Datentransfers ebenfalls noch einleiten
			}
			else
				twi->at91s_twi.TWI_SR &= ~AT91C_TWI_NACK;

			twi->serializer.mode++;
			
			clocks= CLOCK_CALCULATE(twi->at91s_twi.TWI_CWGR);
			SIMUL_StartTimer(processor, twi->serializer.timerid, 
									SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
									&clocks);
			
			break;
		case SERIALIZER_MS_RDDATA:
			//Acknowlege des vorherigen Datentransfers
			if(/*NACk*/0)
			{
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_NACK;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXCOMP;
				twi->at91s_twi.TWI_SR |=  AT91C_TWI_TXRDY;
				//Ende des Datentransfers ebenfalls noch einleiten
			}
			else
				twi->at91s_twi.TWI_SR &= ~AT91C_TWI_NACK;

			//Empfagenes Byte kommt erst am Ende der Übertragung beim vom Sender an
			if(twi->serializer.byte != 0)
			{
				avr_fromavr(processor, twi->nxt, twi->serializer.dadr, twi->serializer.iadr, 
							     			     twi->serializer.byte, &twi->serializer.rhr);
				twi->at91s_twi.TWI_RHR = twi->serializer.rhr;
				twi->at91s_twi.TWI_SR |= AT91C_TWI_RXRDY;
				
				if(twi->at91s_twi.TWI_CR & AT91C_TWI_STOP)
				{	//Keine neuen Daten zu senden
					twi->at91s_twi.TWI_CR &= ~AT91C_TWI_STOP;
					twi->at91s_twi.TWI_SR |= AT91C_TWI_TXCOMP;
					twi->serializer.mode=SERIALIZER_OFF;
					SIMUL_StopTimer(processor, twi->serializer.timerid);
					break;
				}
			}
		
			twi->serializer.byte++;
				
			clocks= CLOCK_CALCULATE(twi->at91s_twi.TWI_CWGR);
			SIMUL_StartTimer(processor, twi->serializer.timerid, 
									SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
									&clocks);
			break;
	}
	
	interrupt_check(processor,private);
    return SIMUL_TIMER_OK;
}
	
/**************************************************************************/

static int SIMULAPI TWI_PortWrite(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	TWI *twi = (TWI *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_CR))
	{   //(TWI) Control Register (Offset: 0x00) Write-Only
		twi->at91s_twi.TWI_CR = cbs->x.bus.data;
		
		if((twi->at91s_twi.TWI_CR & (AT91C_TWI_MSEN | AT91C_TWI_MSDIS)) == AT91C_TWI_MSEN)
		{
			twi->MSEN = 1;
			twi->at91s_twi.TWI_SR |= AT91C_TWI_TXRDY;
			twi->at91s_twi.TWI_SR |= AT91C_TWI_TXCOMP;
		}
			
		if((twi->at91s_twi.TWI_CR & (                 AT91C_TWI_MSDIS)) == AT91C_TWI_MSDIS)
			twi->MSEN = 0;
			
		if(twi->at91s_twi.TWI_CR & AT91C_TWI_SWRST)
		{
			TWI_PortReset(processor, (simulCallbackStruct *) 1,private);
		}
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_MMR))
	{   //(TWI) Master Mode Register (Offset: 0x04) Read/Write
		twi->at91s_twi.TWI_MMR = cbs->x.bus.data & (AT91C_TWI_IADRSZ | 
		                                            AT91C_TWI_MREAD  | 
		                                            AT91C_TWI_DADR   );
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_IADR))
	{   //(TWI) Internal Address Register (Offset: 0x0C) Read/Write
		twi->at91s_twi.TWI_IADR  =  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_CWGR))
	{   //(TWI) Clock Waveform Generator Register (Offset: 0x10) Read/Write
		twi->at91s_twi.TWI_CWGR  = cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_IER))
	{   //(TWI) Interrupt Enable Register (Offset: 0x24) Write-Only
		twi->at91s_twi.TWI_IER  = cbs->x.bus.data;
		twi->at91s_twi.TWI_IMR |= cbs->x.bus.data & (AT91C_TWI_TXCOMP | AT91C_TWI_RXRDY |
		                                             AT91C_TWI_TXRDY  | AT91C_TWI_OVRE  |
													 AT91C_TWI_UNRE   | AT91C_TWI_NACK  );
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_IDR))
	{   //(TWI) Interrupt Disable Register (Offset: 0x28) Write-Only
		twi->at91s_twi.TWI_IDR  =  cbs->x.bus.data;
		twi->at91s_twi.TWI_IMR &= ~cbs->x.bus.data & (AT91C_TWI_TXCOMP | AT91C_TWI_RXRDY |
		                                              AT91C_TWI_TXRDY  | AT91C_TWI_OVRE  |
										  			  AT91C_TWI_UNRE   | AT91C_TWI_NACK  );
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_THR))
	{   //(TWI) Transmit Holding Register (Offset: 0x34) Read/Write
		twi->at91s_twi.TWI_THR=cbs->x.bus.data;

		twi->at91s_twi.TWI_SR &= ~AT91C_TWI_TXRDY;
	}
	else
	{
SIMUL_Printf(processor,"TWI_PortWrite(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}

	serializer_idle(processor,private);
	interrupt_check(processor,private);
	
	return SIMUL_MEMORY_OK;
}


static int SIMULAPI TWI_PortRead(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	TWI *twi = (TWI *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_MMR))
	{   //(TWI) Master Mode Register (Offset: 0x04) Read/Write
		cbs->x.bus.data=twi->at91s_twi.TWI_MMR & (AT91C_TWI_IADRSZ | 
		                                          AT91C_TWI_MREAD  |
										 		  AT91C_TWI_DADR   );
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_IADR))
	{   //(TWI) Internal Address Register (Offset: 0x0C) Read/Write
		cbs->x.bus.data=twi->at91s_twi.TWI_IADR;

//		if(twi->debug == 0)
//			twi->at91s_twi.TWI_SR &= ~AT91C_TWI_RDRF;

	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_CWGR))
	{   //(TWI) Clock Waveform Generator Register (Offset: 0x10) Read/Write
		cbs->x.bus.data=twi->at91s_twi.TWI_CWGR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_SR))
	{   //(TWI) Status Register (Offset: 0x20) Read-Only
		cbs->x.bus.data=twi->at91s_twi.TWI_SR & (AT91C_TWI_TXCOMP | AT91C_TWI_RXRDY |
		                                         AT91C_TWI_TXRDY  | AT91C_TWI_OVRE  |
												 AT91C_TWI_UNRE   | AT91C_TWI_NACK  );

		if((twi->at91s_twi.TWI_SR & AT91C_TWI_NACK) && (twi->debug == 0))
			twi->at91s_twi.TWI_SR &= ~AT91C_TWI_NACK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_IMR))
	{   //(TWI) Interrupt Mask Register (Offset: 0x2c) Read-Only
		cbs->x.bus.data=twi->at91s_twi.TWI_IMR & (AT91C_TWI_TXCOMP | AT91C_TWI_RXRDY |
		                                          AT91C_TWI_TXRDY  | AT91C_TWI_OVRE	 |
												  AT91C_TWI_UNRE   | AT91C_TWI_NACK  );
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_RHR))
	{   //(TWI) Receive Holding Register (Offset: 0x30) Read-Only
		cbs->x.bus.data=twi->at91s_twi.TWI_RHR;
		twi->at91s_twi.TWI_SR &= ~AT91C_TWI_RXRDY;
		
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_TWI_THR))
	{   //(TWI) Transmit Holding Register (Offset: 0x34) Read/Write
		cbs->x.bus.data=twi->at91s_twi.TWI_THR;
	}
	else
	{
SIMUL_Printf(processor,"TWI_PortRead(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}
	
	serializer_idle(processor,private);
	interrupt_check(processor,private);
	
	return SIMUL_MEMORY_OK;
}

/**************************************************************************/

static int SIMULAPI TWI_Go(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    TWI *twi = (TWI *) private;
	twi->debug=0;
}

static int SIMULAPI TWI_Break(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    TWI *twi = (TWI *) private;
	twi->debug=1;
}

/**************************************************************************/

static int SIMULAPI TWI_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    TWI *twi = (TWI *) private;
	
	twi->debug              = 0x1;
	twi->MSEN               = 0x0;

	twi->at91s_twi.TWI_CR    = 0x0;
	twi->at91s_twi.TWI_MMR   = 0x0;
	twi->at91s_twi.TWI_IADR  = 0x0;
	twi->at91s_twi.TWI_CWGR  = 0x0;
	twi->at91s_twi.TWI_SR    = AT91C_TWI_TXRDY;   //Lt. Datenblatt 0x08;  0x04 macht aber mehr Sinn
	twi->at91s_twi.TWI_IER   = 0x0;
	twi->at91s_twi.TWI_IDR   = 0x0;
	twi->at91s_twi.TWI_IMR   = 0x0;
	twi->at91s_twi.TWI_RHR   = 0x0;
	twi->at91s_twi.TWI_THR   = 0x0;
	
	twi->serializer.mode     = SERIALIZER_RESET;
	twi->serializer.thr      = 0x0;
	twi->serializer.rhr      = 0x0;
	twi->serializer.dadr     = 0x0;
	twi->serializer.iadr     = 0x0;
	twi->serializer.byte     = 0x0;

    return SIMUL_RESET_OK;
}


void TWI_PortInit(simulProcessor processor,void *nxt)
{
	TWI            *twi;
    simulWord       from, to;
	
	twi = (TWI *)SIMUL_Alloc(processor, sizeof(TWI));
	twi->nxt=nxt;
	
	SIMUL_Printf(processor,"-> Peripherie TWI loaded (ohne Gewähr, nicht vollständig getestet)\n");

	//Manuell Aufrufen, zur Konfiguration der internen Daten
	TWI_PortReset(processor,NULL, (simulPtr) twi);
	
    SIMUL_RegisterResetCallback(processor, TWI_PortReset, (simulPtr) twi);

    from = (simulWord) AT91C_TWI_CR;  //DJ:AT91C_BASE_TWI;
    to   = (simulWord) AT91C_TWI_CR   /*DJ:AT91C_BASE_TWI*/+sizeof(AT91S_TWI)-1;
    SIMUL_RegisterBusWriteCallback(processor, TWI_PortWrite, (simulPtr) twi, 0, &from, &to);
    SIMUL_RegisterBusReadCallback (processor, TWI_PortRead,  (simulPtr) twi, 0, &from, &to);

    twi->serializer.timerid = SIMUL_RegisterTimerCallback(processor, TWI_TimerElapsed, (simulPtr) twi);

	SIMUL_RegisterGoCallback   (processor, TWI_Go,    (simulPtr) twi);
	SIMUL_RegisterBreakCallback(processor, TWI_Break, (simulPtr) twi);
}
