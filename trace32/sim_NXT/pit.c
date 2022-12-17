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
 PIT Periodic Interval Timer
 Nicht implementiert:
 - keine Berücksichtigung des Power Managements (und damit der Taktrate)
**************************************************************************/

#include "simul.h"
#include "AT91SAM7S64.h"
#include "sim_NXT.h"

/**************************************************************************

	Local port structure
	
**************************************************************************/
typedef struct
{
	AT91S_PITC  at91s_pitc;
	simulTime   timerstartclock;
    void       *timerid;
	simulWord   timerrun;
} PIT;

/**************************************************************************
Periodic Interval Timer (PIT)
***************************************************************************

                                PIV (PIT_MR)
          +------------------+   |
          |                 --------
          |                 \  ==  /
          |                  ------
          |                     +-------------------------------+ set   PITIEN (PIT_MR)
          |                     |                             +------+   /
          |                     |           0    +-------+    | PITS |--  -- PIT_IRQ
          |                     |         ----------     |    +------+
          |                     |          \ 0  1 /----- | ------+ reset
          |                     |           ------       |       |
          +------+  0           |              |         |       |
          |   ----------        |      +--------------+  |  read PIT_PIVR
          |    \ 0  1 /----------------| 12-bit Adder |  |
          |     ------                 +--------------+  |
          |        |                           |         |
          |  +------------+                    |         |
          |  | 20-bit CTR |                    |         |
          |  +------------+                    |         |
  MCK     |        |                           |         |
   |      |  +-----------------------------------------+ |
 +-----+  |  | +------+                     +-------+  | |
 | /16 |---->| | CPIV |       PIT_PIVR      | PICNT |  | |
 +-----+  |  | +------+                     +-------+  | |
          |  +-----------------------------------------+ |
          |       |                             |        |
          +-------+		                        +--------+
			      |                             |
             +-----------------------------------------+
             | +------+                     +-------+  |
			 | | CPIV |     PIT-PIIR        | PICNT |  |
             | +------+                     +-------+  |
             +-----------------------------------------+
The Periodic Interval Timer aims at providing periodic Interrupts for use by operating systems.
The PIT provides a programmable overflow counter an a reset-on-read feature. It is built
around two counters: a 20-bit CPIV counter and a 12-bit PICNT counter. Both counters work at
Master Clock / 16
The first 20-bit CPIV counter increments form 0 up to a programmable overflow value set in the
field PIV of the Mode Register (PIT_MR). When the counter CPIV reaches theis value, it resets to
0 and increments the Periodic Intervall Counter, PICNT. The status bit PiTS in the Status Register
(PIT_SR) rises and triggers an interrupt, provided  the interrupt is enabled (PITIEN in PIT_MR)
Writing a new PIV value in PIT_MR does not reset/restart the counters
When CPIV and PICNT values are obtained by reading the Periodic Interval Value Regiter 
(PIT_PIVR), the overflow counter (PICNT) is reset and the PITS is cleared, thus acknowledging
the interrupt. The value of PICNT gives the number of periodic intervals elapsed since the last
read of PIT_PIVR.
When CPIV and PICNT values are obtained by reading the Periodic Interval Image Register
(PIT_PIIR), there is no effect on the counters CPIV and PICNT, nor on the bit PITS. For exam-
ple, a profiler can read PIT_PIIR withouth clearing any pending inerrupt, whereas a timer
interrupt clears ther interrupt by reading PIT_PIVR.
The PIT may be enabled/disabled using the PITEN bit in the PIT_MR register (disabled on
reset). The PITEN bit only becomes effective when the CPIV value is 0. The Figure illustrates
the PIT counting. After the PIT Enabled bit is reset (PITEN=0), the CPIV goes on counting until
the PIV vlaue is reached, and is then reset. PIT restarts countering, only if the PITEN is set again
The PIT is stopped when the core enters debug state

PIT_MR:   Mode Register                     Offset 0x00 Read/Write 0x000F_FFFF
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        | PITIEN | PITEN  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |                PIV                |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  PIV                                  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  PIV                                  |
+--------+--------+--------+--------+--------+--------+--------+--------+
PIV: Periodic Interval Value
-Defines the value compared wiht the primary 20-bit counter of the Periodic Inteval Timer (CPIV)
 The period is equal to (PIV+1)
PITEN: Period Interval Timer Enabled
- 0 = The Periodic Interval Timer is disabled when the PIV vlaue is reached
- 1 = The Periodic Interval Timer is enbabled
PITIEN: Periodic Inteval Timer Interrupt Enbabled
- 0 = The bit PITS in PIT_SR has no effect on interrupt
- 1 = The bit PiTS in PIT_SR asserts interrupt

PIT_SR:   Status Register                   Offset 0x04 Read       0x0000_0000
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
|        |        |        |        |        |        |        |   PITS |
+--------+--------+--------+--------+--------+--------+--------+--------+
PITS: Periodic Interval Timer Status
- 0 = The Periodic Interval timer has not reached PIV since the last read of PIt_PIVR
- 1 = The Periodic Interval timer has reached PIV since the last read of PIT_PIVR

PIT_PIVR: Periodic Interval Value Register: Offset 0x08 Read       0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 PICNT                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|               PICNT               |               CPIV                |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 CPIV                                  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 CPIV                                  |
+--------+--------+--------+--------+--------+--------+--------+--------+
Reading this registers clears PITS in PIT_SR
CPIV: Current Periodic Interval Value
- Returns the currrent value of the periodic interval timer
PICNT: Periodic Interval Counter
- Returns the number of occurrences of periodic intervals since the last reas of PIT_PIVR


PIT_PIIR: Periodic Interval Image Register: Offset 0x0C Read       0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 PICNT                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|               PICNT               |               CPIV                |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 CPIV                                  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 CPIV                                  |
+--------+--------+--------+--------+--------+--------+--------+--------+
CPIV: Current Periodic Interval Value
- Returns the currrent value of the periodic interval timer
PICNT: Periodic Interval Counter
- Returns the number of occurrences of periodic intervals since the last reas of PIT_PIVR
**************************************************************************/

static int SIMULAPI PIT_PortWrite(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIT *pit = (PIT *) private;

//SIMUL_Printf(processor,"PIT_PortWrite() %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
	
	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PITC_PIMR))
	{   //Period Interval Mode Register
	
		//If Timer Enabled then Start Timer
		if(((pit->at91s_pitc.PITC_PIMR & AT91C_PITC_PITEN) == 0x00000000      ) &&
		   ((cbs->x.bus.data           & AT91C_PITC_PITEN) == AT91C_PITC_PITEN))
		{
			if(!pit->timerid)
			{
				SIMUL_Printf(processor,"Error-PIT: pit->timerid == 0\n");
				return SIMUL_MEMORY_FAIL;
			}

			//Aktuelle Zeit holen
			SIMUL_GetClock(processor, 0, &pit->timerstartclock);
				
			//Start Timer
			simulTime clocks = (16 * (cbs->x.bus.data & AT91C_PITC_PIV))-1;
			SIMUL_StartTimer(processor, pit->timerid, 
			                            SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
			                           &clocks);
									   
			pit->timerrun=1;
		}

		pit->at91s_pitc.PITC_PIMR = cbs->x.bus.data;
		
		//If Timer Interrupt Enabled and PITS==1 then Feuere Interrupt
		if(((pit->at91s_pitc.PITC_PIMR & AT91C_PITC_PITIEN) == AT91C_PITC_PITIEN) &&
		   ((pit->at91s_pitc.PITC_PISR & AT91C_PITC_PITS  ) == AT91C_PITC_PITS  ))
		{
			simulWord data = 1;
			SIMUL_SetPort(processor, AIC_PORTIRQSYS_PIT, 1, &data);
		}
		else
		{
			simulWord data = 0;
			SIMUL_SetPort(processor, AIC_PORTIRQSYS_PIT, 1, &data);
		}
		
		return SIMUL_MEMORY_OK;
	}
	else
		return SIMUL_MEMORY_FAIL;
	
}


static int SIMULAPI PIT_PortRead(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIT *pit = (PIT *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PITC_PIMR))
	{   //Period Interval Mode Register
		cbs->x.bus.data = pit->at91s_pitc.PITC_PIMR;
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PITC_PISR))
	{   //Period Interval Status Register
		cbs->x.bus.data = pit->at91s_pitc.PITC_PISR;
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PITC_PIVR))
	{   //Period Interval Value Register
		simulTime clocks;
	
		if(pit->timerrun == 1)
		{
			SIMUL_GetClock(processor, 0, &clocks);
			clocks = (clocks-pit->timerstartclock)/16;
		}
		else
			clocks = 0;
	
		cbs->x.bus.data = (pit->at91s_pitc.PITC_PIVR & AT91C_PITC_PICNT) + 
		                  ((simulWord)(clocks            & AT91C_PITC_CPIV));
	
		//PITS löschen
		pit->at91s_pitc.PITC_PISR = pit->at91s_pitc.PITC_PISR & ~AT91C_PITC_PITS;
		simulWord data = 0;
		SIMUL_SetPort(processor, AIC_PORTIRQSYS_PIT, 1, &data);
		
		//Counter Rücksetzen	
		pit->at91s_pitc.PITC_PIVR = pit->at91s_pitc.PITC_PIVR & ~AT91C_PITC_PICNT;
		
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_PITC_PIIR))
	{   //Period Interval Image Register
		simulTime clocks;
		
		if(pit->timerrun == 1)
		{
			SIMUL_GetClock(processor, 0, &clocks);
			clocks = (clocks-pit->timerstartclock)/16;
		}
		else
			clocks = 0;

		cbs->x.bus.data = (pit->at91s_pitc.PITC_PIVR & AT91C_PITC_PICNT) + 
		                  ((simulWord)(clocks            & AT91C_PITC_CPIV));
		
		return SIMUL_MEMORY_OK;
	}
	else
	{
SIMUL_Warning(processor,"PIT_PortRead(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}
	
}

static int SIMULAPI PIT_TimerElapsed(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIT *pit = (PIT *) private;

//SIMUL_Printf(processor,"PIT_TimerElapsed()\n");

	pit->at91s_pitc.PITC_PISR |= AT91C_PITC_PITS;
	pit->at91s_pitc.PITC_PIVR = (pit->at91s_pitc.PITC_PIVR + (0x01 << 20)) & AT91C_PITC_PICNT;

	if((pit->at91s_pitc.PITC_PIMR & AT91C_PITC_PITIEN) == AT91C_PITC_PITIEN)
	{
		simulWord data = 1;
		SIMUL_SetPort(processor, AIC_PORTIRQSYS_PIT, 1, &data);
	}
	
	if((pit->at91s_pitc.PITC_PIMR & AT91C_PITC_PITEN) == AT91C_PITC_PITEN)
	{
		//Aktuelle Zeit holen
		SIMUL_GetClock(processor, 0, &pit->timerstartclock);
				
		//Start Timer
		simulTime clocks = (16 * (pit->at91s_pitc.PITC_PIMR & AT91C_PITC_PIV))-1;
		SIMUL_StartTimer(processor, pit->timerid, 
		                            SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
		                           &clocks);
		pit->timerrun=1;
	}
	else
	{
		//Stop-Timer
		SIMUL_StopTimer(processor, pit->timerid);
		pit->timerrun=0;
	}
	
    return SIMUL_TIMER_OK;
}


static int SIMULAPI PIT_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    PIT *pit = (PIT *) private;

//SIMUL_Printf(processor,"PIT_PortReset()\n");

    SIMUL_StopTimer(processor, pit->timerid);
	
	pit->at91s_pitc.PITC_PIMR = 0x000fffff;
	pit->at91s_pitc.PITC_PISR = 0x0;
	pit->at91s_pitc.PITC_PIVR = 0x0;
	pit->at91s_pitc.PITC_PIIR = 0x0;

	pit->timerrun             = 0;
	pit->timerstartclock      = 0;
	
    return SIMUL_RESET_OK;
}


void PIT_PortInit(simulProcessor processor)
{
	PIT            *pit;
    simulWord       from, to;
	
    pit = (PIT *) SIMUL_Alloc(processor, sizeof(PIT));

	SIMUL_Printf(processor,"-> Peripherie PIT loaded (ohne Gewähr, nicht vollständig getestet)\n");
	
	pit->at91s_pitc.PITC_PIMR = 0x000fffff;
	pit->at91s_pitc.PITC_PISR = 0x0;
	pit->at91s_pitc.PITC_PIVR = 0x0;
	pit->at91s_pitc.PITC_PIIR = 0x0;

	pit->timerrun             = 0;
	pit->timerstartclock      = 0;
	
    SIMUL_RegisterResetCallback(processor, PIT_PortReset, (simulPtr) pit);

    from = (simulWord) AT91C_PITC_PIMR;   //DJ:AT91C_BASE_PITC;
    to   = (simulWord) AT91C_WDTC_WDCR-1; //DJ:AT91C_BASE_WDTC-1;
    SIMUL_RegisterBusWriteCallback(processor, PIT_PortWrite, (simulPtr) pit, 0, &from, &to);
    SIMUL_RegisterBusReadCallback (processor, PIT_PortRead,  (simulPtr) pit, 0, &from, &to);

    pit->timerid = SIMUL_RegisterTimerCallback(processor, PIT_TimerElapsed, (simulPtr) pit);

}

