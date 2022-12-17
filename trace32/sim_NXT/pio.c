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
 PIO Parallel Input Output Controler
 Nicht implementiert:
 - keine Berücksichtigung des Power Managements (und damit der Taktrate)
 - keine Berücksichtigung des Glitch Filters
 - keine Berücksichtigung von PullUp 
 - keine Berücksichtigung von OpenCollector (wird eins zu eins durchgereicht)
 - keine Berücksichtigung von Peripheral X Output Enable
ToDo: 
 - LCD Pin berücksichitgen
**************************************************************************/

#include "simul.h"
#include "AT91SAM7S64.h"
#include "sim_NXT.h"
#include <stddef.h>

/**************************************************************************

	Local port structure
	
**************************************************************************/

typedef struct
{
	void       *nxt;
    void       *timerid;
	simulWord   debug;
	AT91S_PIO   at91s_pio;
	simulWord   peripheral_a;
	simulWord   peripheral_b;
} PIO;

/**************************************************************************
  Parallel Input Output Controler (PIO)
***************************************************************************
//PIO_PDSR;
//  0= The I/O-Line is at level 0
//  1= The I/O Line is at level 1
//PIO_ISR;     // Interrupt Status Register
//  0= No Input Change has been detected on the I/O-Line since PIO_iSR was last read or since reset
//  1= At least one Input Change has been detected on the I/O-Line since PIO_ISR was last read or since reset
//PIO_OWSR;     // Output Write Status Register
//  0= Writing PIO_ODSR does not affect the I/O-Line
//  1= Writing PIO_ODSR affects the I/O-Line
//PIO_ABSR;     // AB Select Status Register
//  0= The I/O-Line is assigned to the Peripheral A
//  1= The I/O-Line is assigned to the Peripheral B
//PIO_PSR;     // PIO Status Register
//  0= PIO is inactive on the corresponding I/O-Line (Peripheral is active)
//  1= PIO is   active on the corresponding I/O-Line (peripheral is inactive)
//PIO_ODSR;     // Output Data Status Register
//  0= The data to be driven on the I/O-Line is 0
//  1= The data to be driven on the I/O-Line is 1
//PIO_MDSR;     // Multi-driver Status Register
//  0= The Multi Drive is disabled on the I/O-Line. The pin is driven at high and low level
//  1= The Multi Drive is  enabled on the I/O-Line. the pin is drive at low level only
//PIO_OSR;     // Output Status Register
//  0= The I/O-Line is a pure input
//  1= The I/O-Line is enabled in output
//PIO_IMR;     // Interrupt Mask Register
//  0= Input Change Interrupt is disabled on the I/O-Line
//  1= Input Change Interrupt is enabled  on the I/O-Line
//PIO_IFSR;     // Input Filter Status Register
//  0= The input glitch filter is disabled on the I/O-Line
//  1= The input glitch filter is enabled  on the I/O-Line
//PIO_PPUSR;     // Pull-up Status Register
//  0= Pull Up resistor is  enabled on the I/O-Line
//  1= Pull Up resistor is disabled on the I/O-Line

***************************************************************************
                                                                    +3.3V
                                 |\                                   |
                  |\   PIO_OSR--O|1\                                 +-+
Peripheral A -----|0\            |  |----------------+   PIO_PPUSR---| |
Output Enable     |  |           |  |                |  +----+       | |
                  |  |-----------|0/             |\  +--|    |       +-+
Peripheral B -----|1/            |/|         0 --|0\    | OR |--+     |
Output Enable     |/|              |             |  |---|    |  |     |
                    |              |             |  |   +----+  |     |
  PIO_ABSR ---------+  PIO_PSR ----+  +----------|1/            |     |
                    |              |  |          |/|            |     |
                  |\|              |  |            |            |     |
Peripheral A -----|0\            |\|  | PIO_MDSR---+            |     |
Output            |	 |-----------|0\  |            |            |     |
                  |  |           |  | |          |\|          |\O     |
Peripheral B -----|1/            |  |-+----------|0\          | \     |   +-+
Output            |/  PIO_ODSR---|1/             |  |---------|  >----+---| |
                                 |/              |  |         | /     |   +-+
                                             0 --|1/          |/ /|   |
                                                 |/             / |   |
   +--------------------------------------------------+-------<   |---+
   |                                                  |         \ |
   |          |\    +--PIO_PDSR        +--PIO_ISR     |          \|
   +----------|0\   |                  |              +--Peripheral A Input
   |          |  |  | +-------------+  |  +----+      +--Peripheral B Input
   | +------+ |  |--+-|Edge Detector|--+--|    |      +----+
   +-|Glitch|-|1/     +-------------+     |AND |------|    |
     +------+ |/|               PIO_IMR---|    |      | OR |--- PIO Interrupt
                |                         +----+  ----|    |
    PIO_IFSR----+                                 ----|    |
	                                                  +----+
**************************************************************************/

static void interrupt_check(simulProcessor processor,simulPtr private)
{
	PIO        *pio = (PIO *) private;
	
	if(pio->at91s_pio.PIO_ISR & pio->at91s_pio.PIO_IMR)
	{
		simulWord data = 1;
		SIMUL_SetPort(processor, AIC_PORTIRQ_PIOA, 1, &data);
	}
	else
	{
		simulWord data = 0;
		SIMUL_SetPort(processor, AIC_PORTIRQ_PIOA, 1, &data);
	}
}

static void update(simulProcessor processor,simulPtr private)
{
    PIO      *pio = (PIO *) private;
	simulWord lauf;
	simulWord flag;
	simulWord data;
	
	simulWord per_ab = (pio->peripheral_a       & ~pio->at91s_pio.PIO_ABSR) |
	                   (pio->peripheral_b       &  pio->at91s_pio.PIO_ABSR);
	
	simulWord output = (per_ab                  & ~pio->at91s_pio.PIO_PSR ) |
	                   (pio->at91s_pio.PIO_ODSR &  pio->at91s_pio.PIO_PSR );

	for(lauf=0,flag=0x01;lauf<32;lauf++,flag<<=1)
	{
		data=output & flag ? 1 : 0;
		if(pio->at91s_pio.PIO_OSR & flag)
			SIMUL_SetPort(processor, PIO_PORTPIN_OFFSET+lauf, 1, &data);
	}
	
	pio_changed(processor,pio->nxt,
						  pio->at91s_pio.PIO_ABSR,pio->at91s_pio.PIO_PSR ,
	                      pio->at91s_pio.PIO_ODSR,pio->at91s_pio.PIO_OSR,
						  pio->at91s_pio.PIO_MDSR,pio->at91s_pio.PIO_PPUSR,
						  pio->at91s_pio.PIO_PDSR);
}

static int SIMULAPI PIO_PortPinChange(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIO      *pio = (PIO *) private;
	simulWord data;
	//Hinweis: x.port.olddata entspricht nicht meiner Interpreation von Olddata
	//         daher ein eigenes Old Data mitgezogen
	
	pio->at91s_pio.PIO_ISR  |= (cbs->x.port.newdata) & (~pio->at91s_pio.PIO_PDSR);
	pio->at91s_pio.PIO_PDSR  =  cbs->x.port.newdata;

	//IRQ0 durchreichen
	data = cbs->x.port.newdata & (1<<(PIO_PORT_IRQ0-PIO_PORTPIN_OFFSET)) ? 1 : 0;
	SIMUL_SetPort(processor, AIC_PORTIRQ_IRQ0, 1, &data);

	//IRQ1 durchreichen
	data = cbs->x.port.newdata & (1<<(PIO_PORT_IRQ1-PIO_PORTPIN_OFFSET)) ? 1 : 0;
	SIMUL_SetPort(processor, AIC_PORTIRQ_IRQ1, 1, &data);
	
	//FIQ durchreichen
	data = cbs->x.port.newdata & (1<<(PIO_PORT_FIQ-PIO_PORTPIN_OFFSET)) ? 1 : 0;
	SIMUL_SetPort(processor, AIC_PORTIRQ_FIQ, 1, &data);

	interrupt_check(processor,private);
	pio_changed(processor,pio->nxt,
						  pio->at91s_pio.PIO_ABSR,pio->at91s_pio.PIO_PSR ,
	                      pio->at91s_pio.PIO_ODSR,pio->at91s_pio.PIO_OSR,
						  pio->at91s_pio.PIO_MDSR,pio->at91s_pio.PIO_PPUSR,
						  pio->at91s_pio.PIO_PDSR);

	return SIMUL_PORT_OK;
}

static int SIMULAPI PIO_PortPAChange(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIO *pio = (PIO *) private;

	pio->peripheral_a = cbs->x.port.newdata;

	update(processor,private);
	
	return SIMUL_PORT_OK;
}

static int SIMULAPI PIO_PortPBChange(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIO *pio = (PIO *) private;
	
	pio->peripheral_b = cbs->x.port.newdata;

	update(processor,private);
	
	return SIMUL_PORT_OK;
}

/**************************************************************************/

static int SIMULAPI PIO_PortWrite(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	PIO *pio = (PIO *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_PER))
	{	//(PIOA) PIO Enable Register  (Offset 0x00)  Write-Only
		pio->at91s_pio.PIO_PER  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_PSR |=  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_PDR))
	{   //(PIOA) PIO Disable Register (Offset 0x04)  Write-Only
		pio->at91s_pio.PIO_PDR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_PSR &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_OER))
	{	//(PIOA) Output Enable Register  (Offset 0x10)  Write-Only
		pio->at91s_pio.PIO_OER  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_OSR |=  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_ODR))
	{	//(PIOA) Output Disable Register  (Offset 0x14)  Write-Only
		pio->at91s_pio.PIO_ODR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_OSR &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_IFER))
	{	//(PIOA) Input Filter Enable Register  (Offset 0x20)  Write-Only
		pio->at91s_pio.PIO_IFER  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_IFSR |=  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_IFDR))
	{	//(PIOA) Input Filter Disable Register  (Offset 0x24)  Write-Only
		pio->at91s_pio.PIO_IFDR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_IFSR &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_SODR))
	{	//(PIOA) Set Output Data Register  (Offset 0x30)  Write-Only
		pio->at91s_pio.PIO_SODR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_ODSR |=  (cbs->x.bus.data & ~pio->at91s_pio.PIO_OWSR);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_CODR))
	{	//(PIOA) Clear Output Data Register  (Offset 0x34)  Write-Only
		pio->at91s_pio.PIO_CODR  =   cbs->x.bus.data;
		pio->at91s_pio.PIO_ODSR &= ~(cbs->x.bus.data & ~pio->at91s_pio.PIO_OWSR);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_ODSR))
	{	//(PIOA) Output Data Status Register  (Offset 0x38)  Read-Write
		//Nur Leitungen aus PIO_OWSR dürfen direkt geschrieben werden
		pio->at91s_pio.PIO_ODSR &= ~pio->at91s_pio.PIO_OWSR;
		pio->at91s_pio.PIO_ODSR |= (cbs->x.bus.data & pio->at91s_pio.PIO_OWSR);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_IER))
	{	//(PIOA) Interrupt Enable Register  (Offset 0x40)  Write-Only
		pio->at91s_pio.PIO_IER  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_IMR |=  cbs->x.bus.data;
		interrupt_check(processor,private);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_IDR))
	{	//(PIOA) Interrupt Disable Register  (Offset 0x44)  Write-Only
		pio->at91s_pio.PIO_IDR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_IMR &= ~cbs->x.bus.data;
		interrupt_check(processor,private);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_MDER))
	{	//(PIOA) Multi-driver Enable Register  (Offset 0x50)  Write-Only
		pio->at91s_pio.PIO_MDER  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_MDSR |=  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_MDDR))
	{	//(PIOA) Multi-driver Disable Register  (Offset 0x54)  Write-Only
		pio->at91s_pio.PIO_MDDR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_MDSR &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_PPUDR))
	{	//(PIOA) Pull-up Disable Register  (Offset 0x60)  Write-Only
		pio->at91s_pio.PIO_PPUDR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_PPUSR |=  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_PPUER))
	{	//(PIOA) Pull-up Enable Register  (Offset 0x64)  Write-Only
		pio->at91s_pio.PIO_PPUER  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_PPUSR &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_ASR))
	{	//(PIOA) Select A Register  (Offset 0x70)  Write-Only
		pio->at91s_pio.PIO_ASR   =  cbs->x.bus.data;
		pio->at91s_pio.PIO_ABSR &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_BSR))
	{	//(PIOA) Select B Register  (Offset 0x74)  Write-Only
		pio->at91s_pio.PIO_BSR   =  cbs->x.bus.data;
		pio->at91s_pio.PIO_ABSR |=  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_OWER))
	{	//(PIOA) Output Write Enable Register  (Offset 0xA0)  Write-Only
		pio->at91s_pio.PIO_OWER  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_OWSR |=  cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_OWDR))
	{	//(PIOA) Output Write Disable Register  (Offset 0xA4)  Write-Only
		pio->at91s_pio.PIO_OWDR  =  cbs->x.bus.data;
		pio->at91s_pio.PIO_OWSR &= ~cbs->x.bus.data;
	}
	else
	{
SIMUL_Printf(processor,"PIO_PortWrite(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}

	update(processor,private);
	
	return SIMUL_MEMORY_OK;
}


static int SIMULAPI PIO_PortRead(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	PIO *pio = (PIO *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_PSR))
	{	//(PIOA) PIO Status Register  (Offset 0x08)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_PSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_OSR))
	{	//(PIOA) Output Status Register  (Offset 0x18)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_OSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_IFSR))
	{	//(PIOA) Input Filter Status Register  (Offset 0x28)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_IFSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_ODSR))
	{	//(PIOA) Output Data Status Register  (Offset 0x38)  Read/Write
		cbs->x.bus.data=pio->at91s_pio.PIO_ODSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_PDSR))
	{	//(PIOA) Pin Data Status Register  (Offset 0x3C)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_PDSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_IMR))
	{	//(PIOA) Interrupt Mask Register  (Offset 0x48)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_IMR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_ISR))
	{	//(PIOA) Interrupt Status Register  (Offset 0x4C)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_ISR;
		//Reset by Reading	
		if(pio->debug == 0)
			pio->at91s_pio.PIO_ISR = 0;
		interrupt_check(processor,private);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_MDSR))
	{	//(PIOA) Multi-driver Status Register  (Offset 0x58)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_MDSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_PPUSR))
	{	//(PIOA) Pull-up Status Register  (Offset 0x68)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_PPUSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_ABSR))
	{	//(PIOA) AB Select Status Register  (Offset 0x78)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_ABSR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PIOA_OWSR))
	{	//(PIOA) Output Write Status Register  (Offset 0xA8)  Read-Only
		cbs->x.bus.data=pio->at91s_pio.PIO_OWSR;
	}
	else
	{
SIMUL_Printf(processor,"PIO_PortRead(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}

	update(processor,private);
	
	return SIMUL_MEMORY_OK;
}

/**************************************************************************/

static int SIMULAPI PIO_Go(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIO *pio = (PIO *) private;
	pio->debug=0;
}

static int SIMULAPI PIO_Break(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIO *pio = (PIO *) private;
	pio->debug=1;
}

/**************************************************************************/

static int SIMULAPI PIO_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIO *pio = (PIO *) private;
	simulWord CS;

	pio->at91s_pio.PIO_PER   = 0;     // PIO Enable Register
	pio->at91s_pio.PIO_PDR   = 0;     // PIO Disable Register
	pio->at91s_pio.PIO_PSR   = 0;     // PIO Status Register
	pio->at91s_pio.PIO_OER   = 0;     // Output Enable Register
	pio->at91s_pio.PIO_ODR   = 0;     // Output Disable Registerr
	pio->at91s_pio.PIO_OSR   = 0;     // Output Status Register
	pio->at91s_pio.PIO_IFER  = 0;     // Input Filter Enable Register
	pio->at91s_pio.PIO_IFDR  = 0;     // Input Filter Disable Register
	pio->at91s_pio.PIO_IFSR  = 0;     // Input Filter Status Register
	pio->at91s_pio.PIO_SODR  = 0;     // Set Output Data Register
	pio->at91s_pio.PIO_CODR  = 0;     // Clear Output Data Register
	pio->at91s_pio.PIO_ODSR  = 0;     // Output Data Status Register
	pio->at91s_pio.PIO_PDSR  = 0xffffffff; // Pin Data Status Register
	pio->at91s_pio.PIO_IER   = 0;     // Interrupt Enable Register
	pio->at91s_pio.PIO_IDR   = 0;     // Interrupt Disable Register
	pio->at91s_pio.PIO_IMR   = 0;     // Interrupt Mask Register
	pio->at91s_pio.PIO_ISR   = 0;     // Interrupt Status Register
	pio->at91s_pio.PIO_MDER  = 0;     // Multi-driver Enable Register
	pio->at91s_pio.PIO_MDDR  = 0;     // Multi-driver Disable Register
	pio->at91s_pio.PIO_MDSR  = 0;     // Multi-driver Status Register
	pio->at91s_pio.PIO_PPUDR = 0;     // Pull-up Disable Register
	pio->at91s_pio.PIO_PPUER = 0;     // Pull-up Enable Register
	pio->at91s_pio.PIO_PPUSR = 0;     // Pull-up Status Register
	pio->at91s_pio.PIO_ASR   = 0;     // Select A Register
	pio->at91s_pio.PIO_BSR   = 0;     // Select B Register
	pio->at91s_pio.PIO_ABSR  = 0;     // AB Select Status Register
	pio->at91s_pio.PIO_OWER  = 0;     // Output Write Enable Register
	pio->at91s_pio.PIO_OWDR  = 0;     // Output Write Disable Register
	pio->at91s_pio.PIO_OWSR  = 0;     // Output Write Status Register
	
	pio->peripheral_a        = 0;
	pio->peripheral_b        = 0;
	pio->debug               = 1;

	//Leitungen sind zunächst auf Eingang schalten, daher Ports auf 1 setzen
	simulWord data = 0xffffffff;
	SIMUL_SetPort(processor, PIO_PORTPIN_OFFSET, PIO_PORTPIN_WIDTH, &data);
	
    return SIMUL_RESET_OK;
}

static int SIMULAPI PIO_TimerElapsed(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	//Manuell Aufrufen, zur Konfiguration der internen Daten
	PIO_PortReset(processor,NULL, private);
}

void PIO_PortInit(simulProcessor processor,void *nxt)
{
	PIO            *pio;
    simulWord       from, to;
	
	pio = (PIO *)SIMUL_Alloc(processor, sizeof(PIO));
	pio->nxt=nxt;

	
	SIMUL_Printf(processor,"-> Peripherie PIO loaded (ohne Gewähr, nicht vollständig getestet)\n");

    SIMUL_RegisterResetCallback(processor, PIO_PortReset, (simulPtr) pio);

    from = (simulWord) AT91C_PIOA_PER;  //DJ:AT91C_BASE_PIOA;  //430  434  438
    to   = (simulWord) AT91C_PIOA_PER   /*DJ:AT91C_BASE_PIOA*/ + sizeof(AT91S_PIO)-1;
    SIMUL_RegisterBusWriteCallback(processor, PIO_PortWrite, (simulPtr) pio, 0, &from, &to);
    SIMUL_RegisterBusReadCallback (processor, PIO_PortRead,  (simulPtr) pio, 0, &from, &to);

	//32-PIN (PA0..PA31) Quellen registrieren
	SIMUL_RegisterPortChangeCallback(processor, PIO_PortPinChange, (simulPtr) pio, PIO_PORTPIN_OFFSET,PIO_PORTPIN_WIDTH);

	//32-PA (Peripherals A0..A31) Quellen registrieren
	SIMUL_RegisterPortChangeCallback(processor, PIO_PortPAChange, (simulPtr) pio, PIO_PORTPA_OFFSET,PIO_PORTPA_WIDTH);

	//32-PB (Peripherals B0..B31) Quellen registrieren
	SIMUL_RegisterPortChangeCallback(processor, PIO_PortPBChange, (simulPtr) pio, PIO_PORTPB_OFFSET,PIO_PORTPB_WIDTH);

	SIMUL_RegisterGoCallback   (processor, PIO_Go,    (simulPtr) pio);
	SIMUL_RegisterBreakCallback(processor, PIO_Break, (simulPtr) pio);
	
	//Initialisierung erfolgt über TimerElaplsed, also in PIO_TimerElapsed
    pio->timerid = SIMUL_RegisterTimerCallback(processor, PIO_TimerElapsed, (simulPtr) pio);
	simulTime clocks = 1;
	SIMUL_StartTimer(processor, pio->timerid, 
								SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
								&clocks);
}

