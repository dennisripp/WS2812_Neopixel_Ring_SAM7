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
 AIC Advanced Interrupt Controller
 
Interrupts über PORTs weitergeleiten
	- AIC_PORTIRQ_OFFSET+0    ... AIC_PORTIRQ_OFFSET+31
	- AIC_PORTIRQSYS_OFFSET+0 ... AIC_PORTIRQSYS_OFFSET+5

ToDo: Derzeit keine LatenzZeiten berücksichtigt
      - Verzugszeit beim Erkennen des Interrupts bis zur internen Detektion	  

**************************************************************************/

#include "simul.h"
#include "AT91SAM7S64.h"
#include "sim_NXT.h"
#include <stddef.h>

/**************************************************************************

**************************************************************************/
//#define DEBUG

#ifdef DEBUG
	#define DEBUG_PRINTF(a,b)  Debug_Printf(a,b)
#else
	#define DEBUG_PRINTF(a,b)
#endif	

/**************************************************************************

	Local port structure
	
**************************************************************************/

typedef struct
{
	AT91S_AIC   at91s_aic;
	simulWord   edgedetector[4];
	simulWord   port_old;
} AIC;

#define     SRCTYPE_EXT_LOW_LEVEL_INT_HIGH_LEVEL  ( 0x0 ) 
#define     SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE     ( 0x1 ) 
#define     SRCTYPE_HIGH_LEVEL                    ( 0x2 ) 
#define     SRCTYPE_POSITIVE_EDGE                 ( 0x3 ) 

/**************************************************************************
Advanced Interrupt Controller (AIC)
***************************************************************************

          +---------------------------------------------+  +---------+
          |   Advanced Interrupt Controller             |  |   ARM   |
  +---+   |  +----------+                  +----------+ |  |Processor|
  |FIQ|---|--|   PIO    |------------------|  Fast    | |  |         |
  +---+   |  |          |                  | Interrupt|-|--|nFIQ     |
          |  |Controller|       +----------|Controller| |  |         |
  +----+  |  |          |       |          +----------+ |  |         |
  |IRQ0|  |  |          |  +---------+     +----------+ |  |         |
  | .. |--|--|          |  |  Fast   |     |Interrupt |-|--|nIRQ     |
  |IRQn|  |  |          |  | Forcing |-----|Priority  | |  +---------+
  +----+  |  +----------+  |         |     |Controller| |     | Proc Clock
     |    |  +----------+  |         |-----|          | |  +---------+
     +----|--| Internal |  |         |     |          | |  |Power    |
  +----+  |  |  Source  |  +---------+     +----------+ |  |Managemnt|
  |Embe|--|--|  Input   |                               |  |Controll.|
  |dded|  |  |  Stage   |                               |--+---------+
  |Peri|  |  +----------+  +--------------+             |WakeUp
  |pher|  |                |User Interface|             |
  |als |  |                +--------------+             |
  +----+  +-----------------------+---------------------+

AIC_SMR[32];    Source Mode Register                Read/Write
AIC_SVR[32];    Source Vector Register              Read/Write
AIC_IVR;        IRQ Vector Register                 Read/(Write only in Debug)
AIC_FVR;        FIQ Vector Register                 Read
AIC_ISR;        Interrupt Status Register           Read
AIC_IPR;        Interrupt Pending Register          Read
AIC_IMR;        Interrupt Mask Register             Read
AIC_CISR;       Core Interrupt Status Register      Read
AIC_IECR;       Interrupt Enable Command Register        Write
AIC_IDCR;       Interrupt Disable Command Register       Write
AIC_ICCR;       Interrupt Clear Command Register         Write
AIC_ISCR;       Interrupt Set Command Register           Write
AIC_EOICR;      End of Interrupt Command Register        Write
AIC_SPU;        Spurious Vector Register            Read/Write
AIC_DCR;        Debug Control Register (Protect)    Read/Write
AIC_FFER;       Fast Forcing Enable Register             Write
AIC_FFDR;       Fast Forcing Disable Register            Write
AIC_FFSR;       Fast Forcing Status Register        Read
          
  
AIC_SMR0:	Source Mode Register 0			Offset 0x00  Read/Write	0x0000_0000
AIC_SMR1:	Source Mode Register 1			Offset 0x04  Read/Write	0x0000_0000
  ....                           
AIC_SMR31:	Source Mode Register 31			Offset 0x7C  Read/Write	0x0000_0000
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
|        |     SRCTYPE     |        |        |          PRIOR           |
+--------+--------+--------+--------+--------+--------+--------+--------+
PRIOR: Priority Level
- Programs the priority level for all sources except FIQ source (source 0)
- The priority level can be between 0 (lowest) and 7 (highest)
- the priority level is not used for the FIQ in the realted SMR register AIC_SMRx
SRCTYPE: Interrupt Source Type
- The active level or edge is not programmable for the internal interrupt sources
   SRCTYPE  	InternalInterruptSource		ExternalInteruptSource
  - 0 0   		High level Sensitive		Low level Sensitive
  - 0 1			Positive edge triggered		Negative edge triggered
  - 1 0			Highe level Sensitive		High Level Sensitive
  - 1 1			Positive edge triggered		Positive edge triggered

AIC_SVR0:	Source Vector Register 0 		Offset 0x80  Read/Write	0x0000_0000
AIC_SVR1:	Source Vector Register 1 		Offset 0x84  Read/Write	0x0000_0000
  ....                           
AIC_SVR31:	Source Vector Register 31 		Offset 0xFC  Read/Write	0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                           VECTOR                                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|                           VECTOR                                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                           VECTOR                                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                           VECTOR                                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
VECTOR: Source Vector
- The user may store in these registers the addresses of the corresponding 
  handler for eacht interrupt source.
  
AIC_IVR:	Interrupt Vector Register		Offset 0x100 Read		0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            IRQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            IRQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            IRQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            IRQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
IRQV: Interrupt Vector Register
- The IRQV contains the vector programmed by the user in the Source Vector Reg
  corresponding to the current interrupt
- The IRQV is indexed using the current interrupt number when the IRQV is read
- When there is no current interrupt, the interrupt Vector Register reads the value stored in AIC_SPU

AIC_FVR:	FIQ Interrupt Vector Register	Offset 0x104 Read		0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            FIQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            FIQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            FIQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            FIQV                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
FIQV: FIQ Vector Register
- The FIQV contains the vector programmed by the user in the Source Vector Register 0.
  When there is no fast interrupt, the FIQV reads the value stored in AIC_SPU


AIC_ISR:	Interrupt Status Register		Offset 0x108 Read		0x0000_0000
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
|        |        |        |               IRQID                        |
+--------+--------+--------+--------+--------+--------+--------+--------+
IRQID: Current Interrupt Identifier
- The Interrupt Status Register returns the current interrupt source number


AIC_IPR:	Interrupt Pending Register    	Offset 0x10c Read		0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    | FIQ    |
+--------+--------+--------+--------+--------+--------+--------+--------+
FIQ,SYS;PID2-PID: Interrupt Pending
- 0 = Corresponding interrupt is not pending
- 1 = Corresponding interrupt is pending

AIC_IMR:	Interrupt Mask Register 		Offset 0x110 Read		0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    | FIQ    |
+--------+--------+--------+--------+--------+--------+--------+--------+
FIQ,SYS,PID2..PID31: Interrupt Mask
- 0 = Corresponding interrupt is disabled
- 1 = Corresponding interrupt is enabled

AIC_CISR:	Core Interrupt Status Register	Offset 0x114 Read		0x0000_0000
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
|        |        |        |        |        |        |  NIRQ  |  NFIQ  |
+--------+--------+--------+--------+--------+--------+--------+--------+
NFIQ: NFIO Status
- 0 = nFIQ line is deactivated
- 1 = nFIQ line is active
NIRQ: NIRQ Status
- 0 = nIRQ line is deactivated
- 1 = nIRQ line is active

AIC_IECR:	ISR Enable Command Register 	Offset 0x120      Write ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    | FIQ    |
+--------+--------+--------+--------+--------+--------+--------+--------+
FIQ,SYS,PID2..PID31: Interrupt Enable
- 0 = No effect
- 1 = Enabled corresponding interrupt

AIC_IDCR:	ISR Disable Command Register 	Offset 0x124      Write ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    | FIQ    |
+--------+--------+--------+--------+--------+--------+--------+--------+
FIQ,SYS,PID2..PID31: Interrupt Disable
- 0 = No effect
- 1 = Disables corresponding interrupt

AIC_ICCR:	ISR Clear Command Registeer 	Offset 0x128      Write ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    | FIQ    |
+--------+--------+--------+--------+--------+--------+--------+--------+
FIQ,SYS,PID2..PID31: Interrupt Clear
- 0 = No effect
- 1 = Clears corresponding interrupt


AIC_ISCR:	ISR Set Command Register 		Offset 0x12C      Write ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    | FIQ    |
+--------+--------+--------+--------+--------+--------+--------+--------+
FIQ,SYS,PID2..PID31: Interrupt Set
- 0 = No effect
- 1 = Sets corresponding interrupt


AIC_EOICR:	End Of ISR Command Register		Offset 0x130      Write ---
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
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
the End oof Interrupt command Register is used by the interrupt routine to indicate
that the interrupt treatment is complete.
Any value can be written because it is only necessary to make a write to this
register location to signal the end of interrupt treatment.

AIC_SPU:	Spurious Inerrupt Vector Reg	Offset 0x134 Read/Write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            SIVR                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            SIVR                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            SIVR                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                            SIVR                                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
SIVR: Spurious Interrupt Vector Register
- The user may store the address of a spurious itnerrupt handler in this register.
  The written value is returned in AIC_IVR in case of a spurious interrupt and in 
  AIC_FVR in case of a spurious fast interrupt
  


AIC_DCR:	Debug Control Register			Offset 0x138 Read/Write 0x0000_0000
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
|        |        |        |        |        |        | GMSK   |   PROT |
+--------+--------+--------+--------+--------+--------+--------+--------+
PROT_ Protection Mode
- 0 = The Proctection Mode is disabled
- 1 = The Protection Mode is enabled
GMSK: General Mask
- 0 = The nIRQ and nFIQ liens are normally controlled by the AIC
- 1 = The nIRQ and nFIQ lines are tired to their inactrive state


AIC_FFER:	Fast Forcing Enable Reg 		Offset 0x140      Write ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
SYS,PID2..PID31: fast Forcing Enable
- 0 = No effect
- 1 = Enables the fast forcing feature on the corresponding interrupt


AIC_FFDR:	Fast Forcing Disable Reg 		Offset 0x144      Write ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
SYS,PID2..PID31: Fast Forcing Disable
- 0 = No effect
- 1 Disables the fast forcing freatrue on the corresponding interrupt


AIC_FFSR:	Fast Forcing Status Reg 	    Offset 0x148 Read		0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID31  | PID30  | PID29  | PID28  | PID27  | PID26  | PID25  | PID24  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID23  | PID22  | PID21  | PID20  | PID19  | PID18  | PID17  | PID16  |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID15  | PID14  | PID13  | PID12  | PID11  | PID10  | PID9   | PID8   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
| PID7   | PID6   | PID5   | PID4   | PID3   | PID2   | SYS    |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
SYS,PID2..PID31: Fast Forcing Status
- 0 = The fast Forcing feature is disabled on the corresponding interrupt
- 1 = The fast Forcing feature is enabled on the corresponding interrupt
  
***************************************************************************
Internal Interrupt Source Input Stage

                       AIC_SMRx
                       SRCTYPE
 Source          Level/Edge| 
 +-+                     |\|    AIC_IPR  /
 | |--+------------------| \       |    /         Fast Interrupt Controller
 +-+  |                  |  |------+---/ | ----->    or
      |   +----------+   |  |            |        Priority Controller 
      +---|   EDGE   |---| /      +----------+
          | Detector |   |/       |AIC_IMR   |
          |Set  Clear|            |Set  Clear|
          +----------+            +----------+
 AIC_ISCR---+      |     AIC_IECR---+      |
 AIC_ICCR----------+     AIC_IDCR----------+

External Interrupt Source Input Stage
                      AIC_SMRx
                High  SRCTYPE
             |\ Low     |
             | \---+----+------+Level
 Source  +--O|  \  |           |Edge
 +-+     |   |  |--|------+  |\|
 | |--+--+---|  /  |      |  | \   AIC_IPR  /
 +-+  |      | /   |Pos/  +--|  \     |    /       Fast Interrupt Controller
      |      |/    |Neg      |  |-----+---/ | --->    or
      |   +----------+    +--|  /           |      Priority Controller
      +---|   EDGE   |----+  | /      +----------+
          | Detector |       |/       | AIC_IMR  |
          |Set  Clear|                |Set  Clear|
          +----------+                +----------+
 AIC_ISCR---+      |        AIC_IECR-----+     |
 AIC_ICCR----------+        AIC_IDCR-----------+

Fast Forcing

 Source 0 
 (FIQ)          +---------------+ AIC_IPR  /
 +-+            | Input Stage   |    |    /                        +-----+
 | |------------|               |----+---/ | ----------------------|     |
 +-+            |Automatic Clear|          |                +------|     |
                +---------------+        AIC_IMR            | +----| OR  |--->nFIQ
                     +--- Read AIC_FVR if Fast Forcing is   | | .. |     |
                          disabled on Sources 1 to 31       | |  +-|     |
                                                            | |  | +-----+
 Source 1                                                   | |
 (SYS) +----+                                               | |
 PIT --|    |   +---------------+ AIC_IPR  /            /   | |
 RTT --|    |   | InputStage    |    |    /            /  o-+ |   +--------+
 WDT --| OR |---|               |----+---/ | ---------/ |     |   |Priority|
 DBGU -|    |   |Automatic Clear|          |            | o-------|Manager |---> nIRQ
 PMC --|    |   +---------------+        AIC_IMR      AIC_FFSR|   |        |
 RSTC -|    |        +--- Read AIC_IVR if Source n is the     |   |        |
       +----+             current interrupt and if fast       |   |        |
                          Forcing is disabled on Source n     |   |        |
 Source 2-31                                                  |   |        |
 (PIDx)         +---------------+ AIC_IPR  /            /     |   |        |
 +-+            | InputStage    |    |    /            /  o---+   |        |
 | |------------|               |----+---/ | ---------/ |         |        |
 +-+            |Automatic Clear|          |            | o-------|        |
                +---------------+        AIC_IMR      AIC_FFSR    |        |
                     +--- Read AIC_IVR if Source n is the         |        |
                          current interrupt and if fast           |        |
                          Forcing is disabled on Source n         |        |
                  
**************************************************************************/
#ifdef DEBUG
static void Debug_Printf(simulProcessor processor,simulPtr private)
{
	AIC       *aic = (AIC *) private;
	AT91S_AIC *reg = &aic->at91s_aic;
	simulWord PIDx;
	simulWord flag;
	simulWord lauf;

	SIMUL_Printf(processor,"DCR.PROT=%d DCR.GMSK=%d\n", reg->AIC_DCR&AT91C_AIC_DCR_PROT,
	                                                   (reg->AIC_DCR&AT91C_AIC_DCR_GMSK)>>1);
	for(lauf=0;lauf<2;lauf++)
	{
		if(lauf==0)
		{
			SIMUL_Printf(processor,"Fast Interrupt:\n");
			SIMUL_Printf(processor,"  nFIQ=%d FVR=%08x SPU=%08x\n",
										reg->AIC_CISR&AT91C_AIC_NFIQ,
										reg->AIC_FVR,reg->AIC_SPU);
		}
		else
		{
			SIMUL_Printf(processor,"Priority Interrupt:\n");
			SIMUL_Printf(processor,"  nIRQ=%d IVR=%08x SPU=%08x IRQID=% 2d\n",
										(reg->AIC_CISR&AT91C_AIC_NIRQ)>>1,
										reg->AIC_IVR,reg->AIC_SPU,reg->AIC_ISR&0x1F);
		}
		for(PIDx=0,flag=0x01;PIDx<AIC_PORTIRQ_WIDTH;PIDx++,flag<<=1)
		{
			char      edge[5];
			simulWord svr;
			char      srctypeI;
			char      srctypeE;
			simulWord ipr;
			simulWord imr;
			simulWord ffsr;
			simulWord prior;
			simulWord active;
		
			svr     = reg->AIC_SVR[PIDx];
			edge[0] = ((aic->edgedetector[SRCTYPE_EXT_LOW_LEVEL_INT_HIGH_LEVEL] & flag)?'L':'-');
			edge[1] = ((aic->edgedetector[SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE   ] & flag)?'<':'-');
			edge[2] = ((aic->edgedetector[SRCTYPE_HIGH_LEVEL                  ] & flag)?'H':'-');
			edge[3] = ((aic->edgedetector[SRCTYPE_POSITIVE_EDGE               ] & flag)?'>':'-');
			edge[4] = 0;
			switch(reg->AIC_SMR[PIDx] & AT91C_AIC_SRCTYPE)
			{
				case AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL:
					srctypeI='H'; srctypeE='L'; break;
				case AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE:
					srctypeI='P'; srctypeE='N'; break;
				case AT91C_AIC_SRCTYPE_HIGH_LEVEL:
					srctypeI=     srctypeE='H'; break;
				case AT91C_AIC_SRCTYPE_POSITIVE_EDGE:
					srctypeI=     srctypeE='P'; break;
			}
			ipr     =((reg->AIC_IPR  & flag)?1:0);
			imr     =((reg->AIC_IMR  & flag)?1:0);
			ffsr    =((reg->AIC_FFSR & flag)?1:0);
			prior   = (reg->AIC_SMR[PIDx] >>  0) & 0x07;
			active  = (reg->AIC_SMR[PIDx] >> 31) & 0x01;
			
			if((lauf == 0) && ((PIDx==0) || ((PIDx>0) && (ffsr==1))))
			{  	//External
				SIMUL_Printf(processor,"   %2d:%08x Edge=%s srctype=%c ipr=%d imr=%d prior=%d active=%d\n",
				                        PIDx,svr,edge  ,srctypeE  ,ipr   ,imr   ,prior   ,active);
				
			}
			if((lauf == 1) && ((PIDx>0) && (ffsr==0)))
			{ 	//Internal
				SIMUL_Printf(processor,"   %2d:%08x Edge=%s srctype=%c ipr=%d imr=%d prior=%d active=%d\n",
				                        PIDx,svr,edge  ,(PIDx<30)?srctypeI:srctypeE,ipr   ,imr   ,prior   ,active);
			}
		}
	}
}
#endif

static void InputStage(simulProcessor processor,simulPtr private)
{
	AIC       *aic = (AIC *) private;
	simulWord PIDx;
	simulWord flag;

	for(PIDx=0,flag=0x01;PIDx<AIC_PORTIRQ_WIDTH;PIDx++,flag<<=1)
	{
		if(aic->edgedetector[(aic->at91s_aic.AIC_SMR[PIDx] >> 5) & 0x03] & flag)
			aic->at91s_aic.AIC_IPR |= flag;
		else
			aic->at91s_aic.AIC_IPR &= ~flag;
	}
}

static void FastInterruptController(simulProcessor processor,simulPtr private)
{
	AIC       *aic = (AIC *) private;
	AT91S_AIC *reg = &aic->at91s_aic;

	if((reg->AIC_IPR & reg->AIC_IMR                 & 0x00000001) ||
	   (reg->AIC_IPR & reg->AIC_IMR & reg->AIC_FFSR & 0xFFFFFFFE))
	{
		reg->AIC_CISR |= AT91C_AIC_NFIQ;
		if((reg->AIC_DCR & AT91C_AIC_DCR_GMSK) == 0x00)
		{
			simulWord data = 1;
			SIMUL_SetPort(processor, SIMUL_PORT_ARM_FIQ, 1, &data);
		}
	}
	else
	{
		reg->AIC_CISR &= ~AT91C_AIC_NFIQ;
		simulWord data = 0;
		SIMUL_SetPort(processor, SIMUL_PORT_ARM_FIQ, 1, &data);
	}
}

static void PriorityController(simulProcessor processor,simulPtr private)
{
	AIC        *aic = (AIC *) private;
	AT91S_AIC  *reg = &aic->at91s_aic;
	simulWord	PIDx;
	simulWord	flag;
	int 	   	act_prio = -1;
	int         act_pid  = -1;
	int 		max_prio = -1;
	int			max_pid  = -1;

	//23.7.3.1
	//As a new interrupt condition might have happened on other interrupt sources 
	//since the nIRQ has been asserted; the priority controller determines the current
	//interrupt at the time the AIC_IVR is read
	if(reg->AIC_CISR & AT91C_AIC_NIRQ)
		return;
		
	for(PIDx=1,flag=0x02;PIDx<AIC_PORTIRQ_WIDTH;PIDx++,flag<<=1)
	{
		if(reg->AIC_IPR & reg->AIC_IMR & ~reg->AIC_FFSR & flag)
		{
			int prio=(int)(reg->AIC_SMR[PIDx] & AT91C_AIC_PRIOR);
			if(prio > max_prio)
			{
				max_prio = prio;
				max_pid  = (int)PIDx;
			}
		}
		if(reg->AIC_SMR[PIDx] & 0x80000000)
		{
			int prio=(int)(reg->AIC_SMR[PIDx] & AT91C_AIC_PRIOR);
			if(prio > act_prio)
			{
				act_prio = prio;
				act_pid  = (int)PIDx;
			}
		}
	}
	if(max_prio > act_prio)
	{
		reg->AIC_ISR           = max_pid & 0x1F;
		reg->AIC_SMR[max_pid] |= 0x80000000;
		reg->AIC_CISR         |= AT91C_AIC_NIRQ;
		if((reg->AIC_DCR & AT91C_AIC_DCR_GMSK) == 0x00)
		{
			simulWord data = 1;
			SIMUL_SetPort(processor, SIMUL_PORT_ARM_IRQ, 1, &data);
		}
	}
	else if(act_pid != -1) 
	{
		reg->AIC_ISR = act_pid & 0x1f;
	}
	else
	{
		reg->AIC_ISR = 0;
	}
}

static int SIMULAPI AIC_PortWrite(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	AIC *aic = (AIC *) private;

	if((cbs->x.bus.width == 4*8)                                && 
	  ((cbs->x.bus.address & 0x03) == 0)                        &&
	   (cbs->x.bus.address >= ((simulWord) AT91C_AIC_SMR)+0x00) &&
	   (cbs->x.bus.address <  ((simulWord) AT91C_AIC_SMR)+0x80))
	{   // Source Mode Register 0 .. 31  (Offset: 0x00..0x7F) Read/Write
		simulWord PIDx=(cbs->x.bus.address-(simulWord) AT91C_AIC_SMR)/4;
		aic->at91s_aic.AIC_SMR[PIDx]=cbs->x.bus.data & (AT91C_AIC_PRIOR | AT91C_AIC_SRCTYPE);
	}
	else if((cbs->x.bus.width == 4*8)                                && 
	       ((cbs->x.bus.address & 0x03) == 0)                        &&
	        (cbs->x.bus.address >= ((simulWord) AT91C_AIC_SVR)+0x00) &&
	        (cbs->x.bus.address <  ((simulWord) AT91C_AIC_SVR)+0x80))
	{   // Source Vector Register 0 .. 31 (Offset: 0x80..0xFF) Read/Write
		simulWord PIDx=(cbs->x.bus.address-(simulWord) AT91C_AIC_SVR)/4;
		aic->at91s_aic.AIC_SVR[PIDx]=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_IVR))
	{   //IRQ Vector Register (Offset: 0x100) Read-Only (Debug-Write)
		simulWord PIDx = (aic->at91s_aic.AIC_ISR) & 0x1F;
		simulWord flag = 1<<PIDx;

		//Only Perform in Debug Mode
		if((aic->at91s_aic.AIC_DCR & AT91C_AIC_DCR_PROT) == 1)
		{
			//23.7.3.4
			//Reading the AIC_IVR has the following effects:
			//- Sets the current interrupt to be the pending and enabled interrupt with 
			//  the highest priority. The current level is the priority level of the
			//  current interrupt
			//- De-asserts the nIRQ line on the processor, even if vectoring is not used,
			//  AIC_IVR must be read in order to de-assert nIRQ
			//- Automatically clears the interrupt, if it has been programmed to be 
			//  edge-triggered
			//- Pushes the current level and the current interrupt number on to the stack
			//- Return the value written in the AIC_SVR corresponding to the current interrupt

			aic->at91s_aic.AIC_CISR &= ~AT91C_AIC_NIRQ;
			simulWord data = 0;
			SIMUL_SetPort(processor, SIMUL_PORT_ARM_IRQ, 1, &data);
		
			aic->edgedetector[SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE]&=~flag;
			aic->edgedetector[SRCTYPE_POSITIVE_EDGE            ]&=~flag;
		}
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_IECR))
	{   //Interrupt Enable Command Register (Offset: 0x120) Write-Only
		aic->at91s_aic.AIC_IECR = cbs->x.bus.data;
		aic->at91s_aic.AIC_IMR |= cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_IDCR))
	{   //Interrupt Disable Command Register (Offset: 0x124) Write-Only
		aic->at91s_aic.AIC_IDCR  =  cbs->x.bus.data;
		aic->at91s_aic.AIC_IMR  &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_ICCR))
	{   //Interrupt Clear Command Register (Offset: 0x128) Write-Only
		
		aic->at91s_aic.AIC_ICCR  =  cbs->x.bus.data;
		//External(31..30)->Negative-Edge    Internal(29..0)->Positive-Edge
		aic->edgedetector[SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE] &= ~cbs->x.bus.data;
		//External(31..30)->Positive-Edge    Internal(29..0)->Positive-Edge	
		aic->edgedetector[SRCTYPE_POSITIVE_EDGE    ]         &= ~cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_ISCR))
	{   //Interrupt Set Command Register (Offset: 0x12C) Write-Only
		
		aic->at91s_aic.AIC_ISCR  = cbs->x.bus.data;
		//External(31..30)->Negative-Edge    Internal(29..0)->Positive-Edge
		aic->edgedetector[SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE] |= cbs->x.bus.data;
		//External(31..30)->Positive-Edge    Internal(29..0)->Positive-Edge	
		aic->edgedetector[SRCTYPE_POSITIVE_EDGE    ]         |= cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_EOICR))
	{   //End of Interrupt Command Register (Offset: 0x130) Write-Only
		aic->at91s_aic.AIC_EOICR=cbs->x.bus.data;

		//23.7.3.4 
		//The AIC_EOICR must be written in order to indicate to the AIC that the 
		//current interrupt is finished. This causes the current level to be popped
		//from the stack, restoring the previous current level if one exists on the stack
		//if another interrupt is pending, with lower or equal priority than the old current
		//level but with higher priority than the new current level, the nIRQ line is 
		//re-asserted, but the interrupt sequence does not immediately start because
		//the "I" bit is set in the core.
		aic->at91s_aic.AIC_SMR[aic->at91s_aic.AIC_ISR & 0x1F] &= ~0x80000000;

		//Setzen des neuen Wertes für AIC_ISR erfolgt über den PriorityController()
		//welcher am Ende von PortWrite aufgerufen wird
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_SPU))
	{   //Spurious Vector Register (Offset: 0x134) Read/Write
		aic->at91s_aic.AIC_SPU=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_DCR))
	{   //Debug Control Register (Protect) (Offset: 0x138) Read/Write
		aic->at91s_aic.AIC_DCR = (cbs->x.bus.data & (AT91C_AIC_DCR_PROT | AT91C_AIC_DCR_GMSK));
		
//ToDo im SingleStep Mode bit setzen		
		
		if((aic->at91s_aic.AIC_DCR & AT91C_AIC_DCR_GMSK))
		{	//IRQ FIQ Leitung inkativieren
			simulWord data = 0;
			SIMUL_SetPort(processor, SIMUL_PORT_ARM_IRQ, 1, &data);
			SIMUL_SetPort(processor, SIMUL_PORT_ARM_FIQ, 1, &data);
		}
		else
		{   //IRQ FIQ Aktivieren, sofern angefordert
			simulWord data = 1;
			if(aic->at91s_aic.AIC_CISR | AT91C_AIC_NFIQ)
				SIMUL_SetPort(processor, SIMUL_PORT_ARM_FIQ, 1, &data);
			if(aic->at91s_aic.AIC_CISR | AT91C_AIC_NIRQ)	
				SIMUL_SetPort(processor, SIMUL_PORT_ARM_IRQ, 1, &data);
		}
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_FFER))
	{   //Fast Forcing Enable Register (Offset: 0x140) Write-Only
		aic->at91s_aic.AIC_FFER  =  cbs->x.bus.data;
		aic->at91s_aic.AIC_FFSR |= (cbs->x.bus.data & 0xFFFFFFFE);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_FFDR))
	{   //Fast Forcing Disable Register (Offset: 0x144) Write-Only
		aic->at91s_aic.AIC_FFDR  =   cbs->x.bus.data;
		aic->at91s_aic.AIC_FFSR &= ~(cbs->x.bus.data & 0xFFFFFFFE); 
	}
	else
	{
SIMUL_Printf(processor,"AIC_PortWrite(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}
	
	InputStage(processor,private);
	FastInterruptController(processor,private);
	PriorityController(processor,private);
	DEBUG_PRINTF(processor,private);
	
	return SIMUL_MEMORY_OK;
}


static int SIMULAPI AIC_PortRead(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	AIC *aic = (AIC *) private;

	if((cbs->x.bus.width == 4*8)                                && 
	  ((cbs->x.bus.address & 0x03) == 0)                        &&
	   (cbs->x.bus.address >= ((simulWord) AT91C_AIC_SMR)+0x00) &&
	   (cbs->x.bus.address <  ((simulWord) AT91C_AIC_SMR)+0x80))
	{   // Source Mode Register 0 .. 31 (Offset: 0x00..0x7F) Read/Write
		simulWord PIDx=(cbs->x.bus.address-(simulWord) AT91C_AIC_SMR)/4;
		cbs->x.bus.data=aic->at91s_aic.AIC_SMR[PIDx] & (AT91C_AIC_PRIOR | AT91C_AIC_SRCTYPE);
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8)                                && 
	       ((cbs->x.bus.address & 0x03) == 0)                        &&
	        (cbs->x.bus.address >= ((simulWord) AT91C_AIC_SVR)+0x00) &&
	        (cbs->x.bus.address <  ((simulWord) AT91C_AIC_SVR)+0x80))
	{   // Source Vector Register 0 .. 31 (Offset: 0x80..0xFF) Read/Write
		simulWord PIDx=(cbs->x.bus.address-(simulWord) AT91C_AIC_SVR)/4;
		cbs->x.bus.data=aic->at91s_aic.AIC_SVR[PIDx];
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_IVR))
	{   //IRQ Vector Register (Offset: 0x100) Read-Only (Debug-Write)
		simulWord PIDx = (aic->at91s_aic.AIC_ISR) & 0x1F;
		simulWord flag = 1<<PIDx;

		aic->at91s_aic.AIC_IVR = (aic->at91s_aic.AIC_CISR & AT91C_AIC_NIRQ) ?
						    	  aic->at91s_aic.AIC_SVR[PIDx] :
								  aic->at91s_aic.AIC_SPU;
		cbs->x.bus.data=aic->at91s_aic.AIC_IVR;
			
		if(aic->at91s_aic.AIC_DCR & AT91C_AIC_DCR_PROT)
			return SIMUL_MEMORY_OK;

		//23.7.3.4
		//Reading the AIC_IVR has the following effects:
		//- Sets the current interrupt to be the pending and enabled interrupt with 
		//  the highest priority. The current level is the priority level of the
		//  current interrupt
		//- De-asserts the nIRQ line on the processor, even if vectoring is not used,
		//  AIC_IVR must be read in order to de-assert nIRQ
		//- Automatically clears the interrupt, if it has been programmed to be 
		//  edge-triggered
		//- Pushes the current level and the current interrupt number on to the stack
		//- Return the value written in the AIC_SVR corresponding to the current interrupt

		aic->at91s_aic.AIC_CISR &= ~AT91C_AIC_NIRQ;
		simulWord data = 0;
		SIMUL_SetPort(processor, SIMUL_PORT_ARM_IRQ, 1, &data);
		
		aic->edgedetector[SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE]&=~flag;
		aic->edgedetector[SRCTYPE_POSITIVE_EDGE            ]&=~flag;

		InputStage(processor,private);
		FastInterruptController(processor,private);
		PriorityController(processor,private);
		DEBUG_PRINTF(processor,private);
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_FVR))
	{   //FIQ Vector Register (Offset: 0x104) Read-Only
		aic->at91s_aic.AIC_FVR = (aic->at91s_aic.AIC_CISR & AT91C_AIC_NFIQ) ?
								  aic->at91s_aic.AIC_SVR[0] :
								  aic->at91s_aic.AIC_SPU;
		cbs->x.bus.data=aic->at91s_aic.AIC_FVR;
		//23.7.4.4
		//3. ... Reading the AIC_FVR has the effect of automatically clearing the
		//fast interrupt, if it has been programmed to be edge triggered. In this
		//case only, it de-asserts the nFIQ line on the processor.

		if(aic->at91s_aic.AIC_IPR & aic->at91s_aic.AIC_IMR & 0x00000001)
		{
			aic->edgedetector[SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE]&=~0x01;
			aic->edgedetector[SRCTYPE_POSITIVE_EDGE            ]&=~0x01;
		}
	
		InputStage(processor,private);
		FastInterruptController(processor,private);
		PriorityController(processor,private);
		DEBUG_PRINTF(processor,private);
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_ISR))
	{   //Interrupt Status Register (Offset: 0x108) Read-Only
		cbs->x.bus.data=aic->at91s_aic.AIC_ISR;
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_IPR))
	{   //Interrupt Pending Register (Offset: 0x10C) Read-Only
		cbs->x.bus.data=aic->at91s_aic.AIC_IPR;
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_IMR))
	{   //Interrupt Mask Register (Offset: 0x110) Read-Only
		cbs->x.bus.data=aic->at91s_aic.AIC_IMR;
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_CISR))
	{   //Core Interrupt Status Register (Offset: 0x114) Read-Only
		cbs->x.bus.data=aic->at91s_aic.AIC_CISR;
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_SPU))
	{   //Spurious Vector Register (Offset: 0x134) Read/Write
		cbs->x.bus.data=aic->at91s_aic.AIC_SPU;
		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_DCR))
	{   //Debug Control Register (Protect) (Offset: 0x138) Read/Write
		cbs->x.bus.data=aic->at91s_aic.AIC_DCR;

		return SIMUL_MEMORY_OK;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_AIC_FFSR))
	{   //Fast Forcing Status Register (Offset: 0x148) Read-Only
		cbs->x.bus.data=aic->at91s_aic.AIC_FFSR;
		return SIMUL_MEMORY_OK;
	}
	else
	{
SIMUL_Printf(processor,"AIC_PortRead(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}
	
}

static int SIMULAPI AIC_PortIRQChange(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    AIC *aic = (AIC *) private;
	//Hinweis: x.port.olddata entspricht nicht meiner Interpreation von Olddata
	//         daher ein eignes Old Data mitgezogen
	simulWord pos_edge = ( cbs->x.port.newdata) & (~aic->port_old);
	simulWord neg_edge = (~cbs->x.port.newdata) & ( aic->port_old);

//External(31..30)->Low-Level        Internal(29..0)->High-Level
	aic->edgedetector[SRCTYPE_EXT_LOW_LEVEL_INT_HIGH_LEVEL]   = ((~cbs->x.port.newdata) & 0xC0000001) |
	                                                            (( cbs->x.port.newdata) & 0x3FFFFFFE);
	
//External(31..30)->Negative-Edge    Internal(29..0)->Positive-Edge
	aic->edgedetector[SRCTYPE_EXT_NEG_EDGE_INT_POS_EDGE]     |= ( neg_edge              & 0xC0000001) |
	                                                            ( pos_edge              & 0x3FFFFFFE);          

//External(31..30)->High-Level       Internal(29..0)->High-Level
	aic->edgedetector[SRCTYPE_HIGH_LEVEL       ]              = cbs->x.port.newdata;
	
//External(31..30)->Positive-Edge    Internal(29..0)->Positive-Edge	
	aic->edgedetector[SRCTYPE_POSITIVE_EDGE    ]             |= pos_edge;

	aic->port_old = cbs->x.port.newdata;
	
	InputStage(processor,private);
	FastInterruptController(processor,private);
	PriorityController(processor,private);
	DEBUG_PRINTF(processor,private);
	return SIMUL_PORT_OK;
}

static int SIMULAPI AIC_PortIRQSYSChange(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
//    AIC *aic = (AIC *) private;

	//Oder Verknüfung der 6 Leitungen und Weiterleitung auf IRQ-SYSC
	if(cbs->x.port.newdata != 0)
	{
		simulWord data = 1;
		SIMUL_SetPort(processor, AIC_PORTIRQ_SYS, 1, &data);
	}
	else
	{
		simulWord data = 0;
		SIMUL_SetPort(processor, AIC_PORTIRQ_SYS, 1, &data);
	}
	return SIMUL_PORT_OK;
}

static int SIMULAPI AIC_Go(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    AIC *aic = (AIC *) private;
	aic->at91s_aic.AIC_DCR   &= ~AT91C_AIC_DCR_PROT;
}

static int SIMULAPI AIC_Break(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    AIC *aic = (AIC *) private;
	aic->at91s_aic.AIC_DCR   |= AT91C_AIC_DCR_PROT;
}

static int SIMULAPI AIC_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    AIC *aic = (AIC *) private;
	simulWord PIDx;

	for(PIDx=0;PIDx<AIC_PORTIRQ_WIDTH;PIDx++)
	{
		aic->at91s_aic.AIC_SMR[PIDx]=0x0;
#ifdef DEBUG	
		if(PIDx < 8)
			aic->at91s_aic.AIC_SVR[PIDx]=((PIDx&0x7)+1)<<0|((PIDx&0x7)+1)<<4;
		else if(PIDx < 16)
			aic->at91s_aic.AIC_SVR[PIDx]=((PIDx&0x7)+1)<<8|((PIDx&0x7)+1)<<12;
		else if(PIDx < 24)
			aic->at91s_aic.AIC_SVR[PIDx]=((PIDx&0x7)+1)<<16|((PIDx&0x7)+1)<<20;
		else 
			aic->at91s_aic.AIC_SVR[PIDx]=((PIDx&0x7)+1)<<24|((PIDx&0x7)+1)<<28;
#else
		aic->at91s_aic.AIC_SVR[PIDx]=0x0;
#endif		
	}
	aic->at91s_aic.AIC_IVR   = 0x0;
	aic->at91s_aic.AIC_FVR   = 0x0;
	aic->at91s_aic.AIC_ISR   = 0x0;
	aic->at91s_aic.AIC_IPR   = 0x0;
	aic->at91s_aic.AIC_CISR  = 0x0;
	aic->at91s_aic.AIC_IECR  = 0x0;
	aic->at91s_aic.AIC_IDCR  = 0x0;
	aic->at91s_aic.AIC_ICCR  = 0x0;
	aic->at91s_aic.AIC_ISCR  = 0x0;
	aic->at91s_aic.AIC_EOICR = 0x0;
#ifdef DEBUG	
	aic->at91s_aic.AIC_IMR   = 0x1E;
	aic->at91s_aic.AIC_SMR[1]=  AT91C_AIC_SRCTYPE_POSITIVE_EDGE | 1;
	aic->at91s_aic.AIC_SMR[2]=  AT91C_AIC_SRCTYPE_POSITIVE_EDGE | 1;
	aic->at91s_aic.AIC_SMR[3]=  AT91C_AIC_SRCTYPE_POSITIVE_EDGE | 3;
	aic->at91s_aic.AIC_SMR[4]=  AT91C_AIC_SRCTYPE_POSITIVE_EDGE | 2;
	
	aic->at91s_aic.AIC_SPU   = 0x12345678;
	aic->at91s_aic.AIC_DCR   = AT91C_AIC_DCR_PROT;
	aic->at91s_aic.AIC_FFSR  = 0x0F0000;
#else
	aic->at91s_aic.AIC_IMR   = 0x0;
	aic->at91s_aic.AIC_SPU   = 0x0;
	aic->at91s_aic.AIC_DCR   = 0x0;
	aic->at91s_aic.AIC_FFSR  = 0x0;
#endif	
	aic->at91s_aic.AIC_FFER  = 0x0;
	aic->at91s_aic.AIC_FFDR  = 0x0;
	
	aic->port_old            = 0x0;
	
	if(cbs != NULL)
	{
		simulCallbackStruct port;
		port.x.port.newdata=0;
		port.x.port.olddata=0;
		AIC_PortIRQChange(processor,&port,private);
	}
	
    return SIMUL_RESET_OK;
}


void AIC_PortInit(simulProcessor processor)
{
	AIC            *aic;
    simulWord       from, to;
	
	aic = (AIC *)SIMUL_Alloc(processor, sizeof(AIC));
	
	SIMUL_Printf(processor,"-> Peripherie AIC loaded (ohne Gewähr, nicht vollständig getestet)\n");

	//Manuell Aufrufen, zur Konfiguration der internen Daten
	AIC_PortReset(processor,NULL, (simulPtr) aic);
	
    SIMUL_RegisterResetCallback(processor, AIC_PortReset, (simulPtr) aic);

    from = (simulWord) AT91C_AIC_SMR;      //DJ:AT91C_BASE_AIC;
    to   = (simulWord) AT91C_DBGU_CR-1;    //DJ:AT91C_BASE_DBGU-1;
    SIMUL_RegisterBusWriteCallback(processor, AIC_PortWrite, (simulPtr) aic, 0, &from, &to);
    SIMUL_RegisterBusReadCallback (processor, AIC_PortRead,  (simulPtr) aic, 0, &from, &to);

	//32-IRQ Quellen registrieren
	SIMUL_RegisterPortChangeCallback(processor, AIC_PortIRQChange, (simulPtr) aic, AIC_PORTIRQ_OFFSET,AIC_PORTIRQ_WIDTH);
	
	//6-IRQ Quellen für System registrieren und auf IRQ-SYS weiterleiten
	SIMUL_RegisterPortChangeCallback(processor, AIC_PortIRQSYSChange, (simulPtr) aic, AIC_PORTIRQSYS_OFFSET,AIC_PORTIRQSYS_WIDTH);

	SIMUL_RegisterGoCallback   (processor, AIC_Go,    (simulPtr) aic);
	SIMUL_RegisterBreakCallback(processor, AIC_Break, (simulPtr) aic);

}
