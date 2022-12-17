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
 SPI Serial Peripheral Interface
 Nicht implementiert:
 - Slave Mode
 - LASTXFER, NCPHA, CPO, MODFDIS, MODF, NSSR
 - Prioritäten bei mehren PDC-Channels
 - keine Berücksichtiugng der PIO Signale
 - keine Berücksichtigung des Power Managements (und damit der Taktrate)
 - SPI_MR ist in AT91SAM7S64.h ein weiteres Bit vorhanden (AT91C_SPI_FDIV)
ToDo: 
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
	SERIALIZER_START,
	SERIALIZER_DLYBCS1,
	SERIALIZER_DLYBS,
	SERIALIZER_DATATRANSFER,
	SERIALIZER_DLYBCT,
	SERIALIZER_DLYBCS2
} SERIALIZER_MODE;

typedef struct
{
    void           *timerid;
	SERIALIZER_MODE mode;
	simulWord       tdr;
	simulWord       rdr;
	simulWord       npcs;
} SERIALIZER;

typedef struct
{
	void        *nxt;
	AT91S_SPI   at91s_spi;
	SERIALIZER  serializer;
	simulWord   debug;
} SPI;


static int SIMULAPI SPI_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private);

#define NPCS_CALCULATE(pcs)	(spi->at91s_spi.SPI_MR & AT91C_SPI_PCSDEC ? \
							( pcs & 0x000F0000):                        \
							((pcs & 0x00010000)==0x00000000 ? 0 :       \
							 (pcs & 0x00030000)==0x00010000 ? 4 :       \
							 (pcs & 0x00070000)==0x00030000 ? 8 : 12)  )

/**************************************************************************
Serial Peripheral Interface (SPI)
***************************************************************************

  
SPI_CR:   SPI Control Register               Offset 0x00  Write-Only  ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |LASTXFER|
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
| SWRST  |        |        |        |        |        | SPIDIS | SPIEN  |
+--------+--------+--------+--------+--------+--------+--------+--------+
SPIEN: SPI Enable
- 0 = No effect
- 1 = Enables the SPI to transfer and receive data
SPIDIS: SPI Disable
- 0 = No effect
- 1 = Disables the SPI
As soon as SPIDIS is set, SPI finishes its transfer.
All pins are set in input mode and no data is received or transmitted
If a transfer is in progress, there transfer is finished before the SPI is disabled
If both SPIEN and SPIDIS are equal to one the control register is written, the SPI is disabled
SWRST: SPI Software Reset
- 0 = No effect
- 1 = Reset the SPI. A software-triggered hardware reset of the SPI interface is performed
The SPI is in slave mode after software reset
PDC channels are not effected by software reset
LASTXFER: Last Transfer
- 0 = No effect
- 1 = The current NPCS will be deasserted after the character written in TD has been
      transfered. When CSAAT is set, this allows to close the communication with the
	  current serial peripheral by raising the corresponding NPCS line as soon
	  as TD transfer has completed
	  
SPI_MR:   SPI Mode Register                  Offset 0x04  Read/Write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                DLYBCS                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |                PCS                |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|  LLB   |        |        | MODFDIS|        |PCSDEC  |  PS    | MSTR   |
+--------+--------+--------+--------+--------+--------+--------+--------+
MSTR: Master/Slave Mode
- 0 = SPI is in Slave Mode
- 1 = SPI is in Master Mode
PS: Peripheral Select
- 0 = Fixed Peripheral Select
- 1 = Variable Peripheral Select
PCSDEC: Chip Select Decode
- 0 = The chip selects are directly connected to a peripheral device
- 1 = The four chip select lines are connected to a 4- to 16-Bit decoder
  When PCSDEC equals one, up to 15 Chip Select signals can be generated with the four lines
  using an external 4- to 16-bit decoder. The Chip Select Registers define the characteristics
  of the 15 chip selcets according to the following rules
    SPI_CSR0 defines peripheral chip select signals 0 to 3
    SPI_CSR1 defines peripheral chip select signals 4 to 7
    SPI_CSR2 defines peripheral chip select signals 8 to 11
    SPI_CSR3 defines peripheral chip select signals 12 to 14
MODFDIS: Mode Fault Detection
- 0 = Mode fault detection is enabled
- 1 = Mode fault detection is disabled
LLB: Local Loopback Enable
- 0 Local loopback path disabled
- 1 Local loopback path enabled
  LLB controls the local loopback of the data serializer for testing in Master Mode only.
  (MISO is internally connected on MOSI)  
PCS: Peripheral Chip Select
  This field is only used if Fixed Peripheral Select is active (PS=0)
  IF PCSDEC=0:
    PCS=xxx0   NPCS[0..3] = 1110
    PCS=xx01   NPCS[0..3] = 1101
    PCS=x011   NPCS[0..3] = 1011
    PCS=0111   NPCS[0..3] = 0111
    PCS=1111   forbidden (no peripheral is selected)
  IF PCSDEC=1:
     NPCS[3:0] output signals = PCS
DLYBCS: Delay Between Chip Selects
  this fields defines the delay from NPCS inactive to the activation of another NPCS.
  The DLYBCS time guarantees non-overlapping chip selects and solves bus contentions 
  in case of peripheral having long data float times.
  If DLYBCS is less than or equal to six, six MCK periods will be inserted by default
  Otherwise, the following equation determines the delay
       Delay Between Chip Selects = CLYBCS/MCK

SPI_RDR:  SPI Receive Data Register          Offset 0x08  Read-Only  0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |                PCS                |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  RD                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  RD                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
RD: Receive Data
  Data received by the SPI interface is stored in this register right-justified. 
  Unused bits read zero.
PCS: Peripheral Chip Select
  In Master Mode only, these bits indicate the value on the NPCS pins at the end
  of a transfer. Otherwise, these bits read zero

SPI_TDR:  SPI Transmit Data Register         Offset 0x0C  Write-Only  ---
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |LASTXFER|
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |                PCS                |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  TD                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                  TD                                   |
+--------+--------+--------+--------+--------+--------+--------+--------+
TD: Transmit Data
Data to be transmitted by the SPI Interface is stored in this register. Information 
to be transmitted must be written to the transmit register in a right-justified format
PCS: Peripheral Chip Select
This field is only used if Variable Peripheral Select is active (PS=1)
IF PCSDEC=0:
  PCS=xxx0   NPCS[0..3] = 1110
  PCS=xx01   NPCS[0..3] = 1101
  PCS=x011   NPCS[0..3] = 1011
  PCS=0111   NPCS[0..3] = 0111
  PCS=1111   forbidden (no peripheral is selected)
IF PCSDEC=1:
   NPCS[3:0] output signals = PCS
LASTXFER: Last Transfer
- 0 = No effect
- 1 = The current NPCS will be deasserted after the character written in TD has been
      transfered. When CSAAT is set, this allows to close the communication with the
	  current serial peripheral by raising the corresponding NPCS line as soon
	  as TD transfer has completed
This field is only used if Variable Peripheral Select is active (PS=1)	  

SPI_SR:   SPI Status Register                Offset 0x10  Read-Only  0x0000_00F0
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        |        |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        | SPIENS |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |TXEMPTY |  NSSR  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|TXBUFE  |RXBUFF  |ENDTX   |ENDRX   |OVRES   |MODF    |TDRE    |RDRF    |
+--------+--------+--------+--------+--------+--------+--------+--------+
RDRF: Receive data Register Full
- 0 = No data has been received since the last read of SPI_RDR
- 1 = Data has been received and the received data has been transferred from the serializer
      to SPI_RDR since the last read of SPI_RDR
TDRE: Transmit data Register Empty
- 0 = Data has been written to SPI_TDR and not yet transferred to the serializer
- 1 = The last data written in the Transmit Data Register has been transferred to the 
      serializer. TDRE equals zero when the SPI is disabled or at reset. 
	  The SPI enable command sets this bit to one
MODF: Mode Fault Error
- 0 = No mode fault has been detected since the last read of SPI_SR
- 1 = A Mode fault occurred since the last read of the SPI_SR
OVRES: Overrun ERror Status
- 0 = No overrun has been detected since the last read of SPI_SR
- 1 = An overrun has occured since the last read of SPI_SR
      An overrun occurs when SPI_RDR is loaded at least twice from the serializer
	  since the last read of the SPI_RDR
ENDRX: End of RX buffer
- 0 = The Receive Counter register has not reached 0 since the last write in SPI_RCR or SPI_RNCR
- 1 = The receive counter register has reached 0 since the last write in SPI__RCR or SPI_RNCR
ENDTX: End of TX buffer
- 0 = The transmit counter register has not reached 0 since the last write in SPI_TCR or SPI_TNCR
- 1 = The transmit counter register has reached 0 since the last write in SPI_TCR or SPI_TNCR
RXBUFF: RX Buffer Full
- 0 = SPI_RCR or SPI_RNCR has a value other than 0
- 1 = Both SPI_RCR and SPI_RNCR has a value of 0
TXBUFE: TX Buffer Empty
- 0 = SPI_TCR or SPI_TNCR has a value other than 0
- 1 = Both SPI_TCR and SPI_TNCR have a value of 0
NSSR: NSS Rising
- 0 = No rising edge detected on NSS pin since last read
- 1 = A rising edge occurred on NSS pin since last read
TXEMPTY: Transmission Registers Empty
- 0 = As soon as data is written in SPI_TDR
- 1 = SPI_TDR and internal shifter are empty. If a transfer delay has been defined,
      TXEMPTY is set after the completion of such delay
SPIENS: SPI Enable Status
- 0 = SPI is disabled
- 1 = SPI is enabled

SPI_IER:  SPI Interrupt Enable Register      Offset 0x14  Write-Only  ---
SPI_IDR:  SPI Interrupt Disable Register     Offset 0x18  Write-Only  ---
SPI_IMR:  SPI Interrupt Mask Register        Offset 0x1C  Read-Only  0x0000_0000
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
|        |        |        |        |        |        |TXEMPTY |NSSR    |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|TXBUFE  |RXBUFF  |ENDTX   |ENDRX   |OVRES   |MODF    |TDRE    |RDRF    |
+--------+--------+--------+--------+--------+--------+--------+--------+
RDRF: Receive Data Register Full Interrupt Enable / Disable / Mask
TDRE: SPI Transmit DAta Register Empty Interrupt Enable / Disable / Mask
MODF: Mode Fault Error Interrupt Enable / Disable / Mask
OVRES: Overrun Error Interrupt Enable / Disable / Mask
ENDRX: End of Receive Buffer Interrupt Enable / Disable / Mask
ENDTX: End of Transmit Buffer Interrupt Enable / Disable / Mask
RXBUFF: Receive Bufffer Full Interrupt Enable / Disable / Mask
TXBUFE: Transmit Buffer Empty Interrupt Enable / Disable / Mask
TXEMPTY: Transmission Registers Empty Enable / Disable / Mask
NSSR: NSS Rising Interrupt Enable / Disable / Mask
- 0 = No effect        / The corresponding interrupt is not enabled
- 1 = Enable / Disable / The corresponding interrupt is enabled

SPI_CSR0: SPI Chip Select Register 0         Offset 0x30  Read/Write 0x0000_0000
SPI_CSR1: SPI Chip Select Register 1         Offset 0x34  Read/Write 0x0000_0000
SPI_CSR2: SPI Chip Select Register 2         Offset 0x38  Read/Write 0x0000_0000
SPI_CSR3: SPI Chip Select Register 3         Offset 0x3C  Read/Write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 DLYBCT                                |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 DLYBS                                 |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                                 SCBR                                  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                BITS               | CSAAT  |        | NCPHA  | CPO    |
+--------+--------+--------+--------+--------+--------+--------+--------+
CPOL: Clock Polarity
- 0 = The inactive state value of SPCK is logic level zero
- 1 = The inactive state value of SPCK is logic level one
  CPOS is used to determine the inactive state value of the serial clock (SPCK). 
  It is used with NCPHA to produce the required clock/data relationship between master
  and slave devices
NCPHA: Clock Phase
  - 0 = Data is changed on the leading edge of SPCK and captured on the following edge of SPCK
  - 1 = Data is captured on the leading edge of SPCK and changed on the following edge of SPCK
  NPCHA determines which edge of SPCK causes data to change and which edge causes data to
  be captured. NCPHA is used with CPOL to produce the required clock/data relationship 
  between master and slave devices.
CSAAT: Chip Select Active After Transfer
- 0 = The Peripheral Chip Select Line rises as soon as the last transfer is achieved
- 1 = The Peripheral Chip Select does not rise after the last transfer is achieved. 
      It remains active until a new transfer is requested on a different chip select.
BITS: Bits per Transfer
The BITS field determines the number of data bits transfered. Reserved values should not 
be used
   BITS = 0000   Bits per Transfer =  8
   BITS = 0001   Bits per Transfer =  9
   BITS = 0010   Bits per Transfer = 10
   BITS = 0011   Bits per Transfer = 11
   BITS = 0100   Bits per Transfer = 12
   BITS = 0101   Bits per Transfer = 13
   BITS = 0110   Bits per Transfer = 14
   BITS = 0111   Bits per Transfer = 15
   BITS = 1000   Bits per Transfer = 16
   BITS = Rest   Bits per Transfer = Reserved
SCBR: Serial Clock Baud Rate
   In Master Mode, the SPI  Interface uses a modulus counter to derive the SPCK baud rate
   from the Master Clock MCK. The Baud rate is selected by writing a vlaue from 1 to 255
   in the SCBR field. The following equations determine the  SPCK baud rate:
      SPCK Baudrate = MCK / SCBR
   Programming the SCBR field at 0 is forbidden. Triggering a transfer while SCBR is at 0
   can lead to unpredictable results. At reset, SCBR is 0 and the user has to program
   it at a valid value before performing the first transfer.
DLYBS: Delay Before SPCK
   This field defines the delay from NPCS valid to the first valid SPCK transition.
   When DLYBS equals zero, the NPCS valid to SPCK transition is 1/2 the SPCK clock period.
   Otherwise, the following equations determine the delay.   
      Delay Before SPCK = DLYBS / MCK
DLYBCT: Delay Between Consecutive Transfers
   This field defines the delay between two consecutive transfers with the same peripheral
   without removing the chip select. The delay is always inserted after each transfer and
   before removing the chip select if needed.
   When DLYBCT equals zero, no delay between consecutive transfers is inserted and the
   clock keeps its duty cycle over the chracter transfer. 
   Otherwise, the following equation determines the delay
      Delay Between Consecutive Transfers = 32 * DLYBCT /  MCK
   
SPI_RPR:  PDC SPI Receive Pointer Register       Offset 0x100 Read/Write 0x0000_0000
SPI_TPR:  PDC SPI Transmit Pointer Register      Offset 0x108 Read/Write 0x0000_0000
SPI_RNPR: PDC SPI Receive Next Pointer Register  Offset 0x110 Read/Write 0x0000_0000
SPI_TNPR: PDC SPI Transmit Next Pointer Register Offset 0x118 Read/Write 0x0000_0000
    31      30       29        28      27       26       25        24
+--------+--------+--------+--------+--------+--------+--------+--------+
|                  RXPTR / TXPTR / RXNPTR / TXNPTR                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
    23      22       21        20      19       18       17        16
+--------+--------+--------+--------+--------+--------+--------+--------+
|                  RXPTR / TXPTR / RXNPTR / TXNPTR                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
    15      14       13        12      11       10        9         8
+--------+--------+--------+--------+--------+--------+--------+--------+
|                  RXPTR / TXPTR / RXNPTR / TXNPTR                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                  RXPTR / TXPTR / RXNPTR / TXNPTR                      |
+--------+--------+--------+--------+--------+--------+--------+--------+
RXPTR: Receive Pointer Address
  Address of the next receive transfer
TXPTR: Transmit Pointer Address
  Address of the transmit buffer
RXNPTR: Receive Next Pointer Register
  RXNPTR is the address of the next buffer to fill with received data when the
  current buffer is full
TXNPTR: Transmit Next Pointer Address
  TXNPTR is the address of the next buffer to transmit when the current buffer is empty
  
SPI_RCR:  PDC SPI Receive Counter Register       Offset 0x104 Read/Write 0x0000_0000
SPI_TCR:  PDC SPI Transmit Counter Register      Offset 0x10C Read/Write 0x0000_0000
SPI_RNCR: PDC SPI Receive Next Counter Register  Offset 0x114 Read/Write 0x0000_0000
SPI_TNCR: PDC SPI Transmit Next Counter Register Offset 0x11C Read/Write 0x0000_0000
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
|                   RXCTR / TXCTR / RXNCR / TXNCR                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|                   RXCTR / TXCTR / RXNCR / TXNCR                       |
+--------+--------+--------+--------+--------+--------+--------+--------+
RXCTR: Receive Counter Value
  Number of receive transfers to be performed
TXCTR: Transmit Counter Value
  TXCTR is the size of the transmit transfer to ber performed. At zero, the peripheral
  data transfer is stopped.
RXNCR: Receive Next Counter Value
  RXNCR ist the size of the next buffer to receive
TXNCR: Transmit Next Counter Value
  TXNCR is the size of the next buffer to transmit

SPI_PTCR: PDC SPI PDC Transfer Control Register  Offset 0x120 Write-Only 0x0000_0000 
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
|        |        |        |        |        |        |TXTDIS  |TXTEN   |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |RXTDIS  |TXTEN   |
+--------+--------+--------+--------+--------+--------+--------+--------+
RXTEN: Receiver Transfer Enable
- 0 = No effect
- 1 = Enables the receiver PDC transfer requests if RXTDIS is not set
RXTDIS: Receiver Transfer Disable
- 0 = No effect
- 1 = Disables the receiver PDC transfer requests
TXTEN: Transmitter Transfer Enable
- 0 = No effect
- 1 = Enables the transmitter PDC transfer requests.
TXTDIS: Transmitter Transfer Disable
- 0 = No effect
- 1 = Disables the transmitter PDC transfer requests

SPI_PTSR: PDC SPI PDC Transfer Status Register   Offset 0x124 Read-Only 0x0000_0000
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
|        |        |        |        |        |        |        | TXTEN  |
+--------+--------+--------+--------+--------+--------+--------+--------+
     7       6        5         4       3        2        1         0
+--------+--------+--------+--------+--------+--------+--------+--------+
|        |        |        |        |        |        |        | RXTEN  |
+--------+--------+--------+--------+--------+--------+--------+--------+
RXTEN: Receiver Transfer Enable
- 0 = Receiver PDC transfer requests are disabled
- 1 = Receiver PDC transfer requests are enabled
TXTEN: Transmitter Transfer Enable
- 0 = Transmitter PDC transfer requests are disabled
- 1 = Transmitter PDC transfer requests are enabled
-
***************************************************************************
Master Mode Block Diagram

        +--------------+
        |SPI_CSR0..3   |
        |      SCBR    |
        +--------------+
		          |                                           SPCK
        +--------------------------------+                     +-+
MCK ----|   Baud Rate Generator          |---------------------| |
        +--------------------------------+                     +-+
		                  |SPI Clock
        +--------------+  |  +-----------+
        |SPI_CSR0..3   |  |  |SPI_RDR    |--> RDRF
        |      BITS    |  |  |       RD  |--> OVRES
		|      NCPHA   |  |  +-----------+
        |      CPOL    |  |     /\
        +--------------+  |     |
MISO             |        |     |                             MOSI
+-+   LSB +-------------------------+ MSB                      +-+
| |-------|     Shift Register      |--------------------------| |
+-+       +-------------------------+                          +-+
                                /\
                                |
                             +-----------+	
                             |SPI_TDR    |
                             |       TD  |--> TDRE
                             +-----------+							 
            +--------------+ +-----------+
		    |SPI_CSR0..3   | |SPI_RDR    |
            |       CSAAT  | |   +-> PCS |
		    +--------------+ +---+-------+
                        |        |
                    +--------------------+              NPCS3  +-+
  +--------+ +----+ |                    |---------------------| |
  |SPI_MR  | | PS | |     Current        |                     +-+
  |   PCS  | +----+ |    Peripheral      |              NPCS2  +-+
  +--------+   |    |                    |---------------------| |
        |    |\|    |                    |                     +-+
        +----|0\    |                    |              NPCS1  +-+
             |  |   |                    |---------------------| |
        +----|1/    |                    |                     +-+
		|    |/     |                    |              NPCS0  +-+
  +--------+        |                    |---------------------| |
  |SPI_TDR |        +--------------------+                     +-+
  |    PCS |      PCSDEC ---+
  +--------+         +-----+            /
              MSTR---|     |           /       +------+
+-+NPCS0       |\    | AND |----------/ | -----| MODF |
| |------------| O---|     |            |      +------+
+-+            |/    +-----+	      MODFDIS


Slave Mode Block Diagram
+-+ SPCK
| |---------+
+-+         |
            |   +-----+
+-+ NSS |\  +---|     |
| |-----| O-----| AND |---+
+-+     |/  +---|     |   |
            |   +-----+   |
    SPIENS--+			  |SPI Clock
        +--------------+  |  +-----------+
        |SPI_CSR0      |  |  |SPI_RDR    |--> RDRF
        |      BITS    |  |  |       RD  |--> OVRES
		|      NCPHA   |  |  +-----------+
        |      CPOL    |  |     /\
        +--------------+  |     |
MISO             |        |     |                             MOSI
+-+   LSB +-------------------------+ MSB                      +-+
| |-------|     Shift Register      |--------------------------| |
+-+       +-------------------------+                          +-+
                                /\
                                |
                             +-----------+	
                             |SPI_TDR    |
                             |       TD  |--> TDRE
                             +-----------+							 
			  
**************************************************************************/

static void interrupt_check(simulProcessor processor,simulPtr private)
{
	SPI        *spi = (SPI *) private;
	
	if(spi->at91s_spi.SPI_MR & AT91C_SPI_MODFDIS)
	{	//MODFDIS == 1 : Mode fault detection is disabled
		spi->at91s_spi.SPI_SR &= ~AT91C_SPI_MODF;
	}
	else
	{	//MODFDIS == 0 : Mode fault detection is enabled
		//Fehler wird hier aber nicht berücksichtigt
		spi->at91s_spi.SPI_SR &= ~AT91C_SPI_MODF;
	}
	
	//NSSR: a rising Edge occured on NSS pin since last read
	spi->at91s_spi.SPI_SR &= ~AT91C_SPI_NSSR;
	
	if(spi->at91s_spi.SPI_SR & spi->at91s_spi.SPI_IMR & (AT91C_SPI_RDRF  | AT91C_SPI_TDRE  |
		                                                 AT91C_SPI_MODF  | AT91C_SPI_OVRES |
													     AT91C_SPI_ENDRX | AT91C_SPI_ENDTX |
													     AT91C_SPI_RXBUFF| AT91C_SPI_TXBUFE|
													     AT91C_SPI_NSSR  | AT91C_SPI_TXEMPTY))
	{
		simulWord data = 1;
		SIMUL_SetPort(processor, AIC_PORTIRQ_SPI, 1, &data);
	}
	else
	{
		simulWord data = 0;
		SIMUL_SetPort(processor, AIC_PORTIRQ_SPI, 1, &data);
	}
}

/**************************************************************************/
static void pdc_idle(simulProcessor processor, simulPtr private)
{
    SPI      *spi = (SPI *)private;
	simulWord data;
	simulWord address;
	simulWord offset;
	
	if(spi->at91s_spi.SPI_MR & AT91C_SPI_PS)
	{	//PS == 1 (Variable Peripheral)
		//28.6.3.5
		//Da dass PCS Feld hier Berüksichtigt wird, müssen immer 32-Bit übertragen werden
		offset = 4;
	}
	else
	{	//PS == 0 (Fixed Peripheral)
		//28.6.3.5
		//In diesem Mode wird das PCS Feld im TDR nicht berücksichtigt. Die Anzahl
		//der zu übertragenden Bytes ergibt sich aus der Datenbreite
		simulWord npcs = NPCS_CALCULATE(spi->at91s_spi.SPI_MR);
		simulWord BITS=(spi->at91s_spi.SPI_CSR[npcs>>2]>>4)&0x00F;
		if((8+BITS) == 8)
		{
			offset = 1;
		}
		else
		{
			offset = 2;
		}
	}
	
	//28.6.3  The TDRE bit is used to trigger the transmit PDC channel
	if((spi->at91s_spi.SPI_SR   & AT91C_SPI_TDRE ) &&
	   (spi->at91s_spi.SPI_PTSR & AT91C_PDC_TXTEN) &&
	   ((spi->at91s_spi.SPI_TCR & 0xffff) != 0   ))
	{	//The last data written in the Transmit Data Register has been tranfered
		//to the serializer
		
		address=spi->at91s_spi.SPI_TPR;
		SIMUL_ReadMemory(processor, 0, &address, offset*8, SIMUL_MEMORY_HIDDEN, &data);
			
		//Ruft die CallBack Funktion 'SPI_PortWrite', welches TDRE dann löscht
		address=(simulWord)AT91C_SPI_TDR;
		SIMUL_WriteMemory(processor, 0, &address, 4*8, SIMUL_MEMORY_HIDDEN, &data);
			
		spi->at91s_spi.SPI_TPR += offset;
		spi->at91s_spi.SPI_TCR  =(spi->at91s_spi.SPI_TCR-1)&0xFFFF;
	}
	
	if((spi->at91s_spi.SPI_SR   & AT91C_SPI_RDRF ) &&
	   (spi->at91s_spi.SPI_PTSR & AT91C_PDC_RXTEN) &&
	   ((spi->at91s_spi.SPI_RCR & 0xFFFF) != 0   ))
	{	//Data has been receive and the received data has been transfered
		//from the serializer to SPI_RDR since the last read of SPI_RDR

		//Ruft die CallBack Funktion 'SPI_PortRead', welches RDRF dann löscht
		address=(simulWord)AT91C_SPI_RDR;
		SIMUL_ReadMemory(processor, 0, &address, 4*8, SIMUL_MEMORY_HIDDEN, &data);
		
		address=spi->at91s_spi.SPI_RPR;
		SIMUL_WriteMemory(processor, 0, &address, offset*8, SIMUL_MEMORY_HIDDEN, &data);
		
		spi->at91s_spi.SPI_RPR += offset;
		spi->at91s_spi.SPI_RCR  =(spi->at91s_spi.SPI_RCR -1)&0xFFFF;
	}

	if((spi->at91s_spi.SPI_TCR & 0xffff) == 0)
	{
		spi->at91s_spi.SPI_SR |= AT91C_SPI_ENDTX;

		if((spi->at91s_spi.SPI_TNCR & 0xffff) == 0)
		{
			spi->at91s_spi.SPI_SR  |= AT91C_SPI_TXBUFE;
		}
		else
		{
			spi->at91s_spi.SPI_SR  &= ~AT91C_SPI_TXBUFE;
			
			spi->at91s_spi.SPI_TPR  = spi->at91s_spi.SPI_TNPR;
			spi->at91s_spi.SPI_TCR  = spi->at91s_spi.SPI_TNCR;
			spi->at91s_spi.SPI_TNCR = 0;
		}
	}
	else
		spi->at91s_spi.SPI_SR &= ~AT91C_SPI_ENDTX;
	
	if((spi->at91s_spi.SPI_RCR & 0xffff) == 0)
	{
		spi->at91s_spi.SPI_SR |= AT91C_SPI_ENDRX;

		if((spi->at91s_spi.SPI_RNCR & 0xffff) == 0)
		{
			spi->at91s_spi.SPI_SR  |= AT91C_SPI_RXBUFF;
		}
		else
		{
			spi->at91s_spi.SPI_SR  &= ~AT91C_SPI_RXBUFF;
			
			spi->at91s_spi.SPI_RPR  = spi->at91s_spi.SPI_RNPR;
			spi->at91s_spi.SPI_RCR  = spi->at91s_spi.SPI_RNCR;
			spi->at91s_spi.SPI_RNCR = 0;
		}
	}
	else
		spi->at91s_spi.SPI_SR &= ~AT91C_SPI_ENDRX;
		
}


/**************************************************************************/
static void serializer_idle(simulProcessor processor, simulPtr private)
{
    SPI      *spi = (SPI *)private;
	simulTime clocks;
	
	if(!spi->serializer.timerid)
	{
		SIMUL_Warning(processor,"Error-SPI: spi->serializer.timerid == 0\n");
		return;
	}

	if((spi->serializer.mode == SERIALIZER_OFF) ||
	   (spi->serializer.mode == SERIALIZER_RESET))
	{
		if(spi->at91s_spi.SPI_SR & AT91C_SPI_SPIENS)
		{
			if(spi->at91s_spi.SPI_MR & AT91C_SPI_MSTR) 
			{ 	//Master Mode
				if((spi->at91s_spi.SPI_SR & AT91C_SPI_TDRE) == 0)
				{	//New Byte in TDR available
					spi->serializer.mode=SERIALIZER_START;
					//Start Timer
					clocks = 1;
					SIMUL_StartTimer(processor, spi->serializer.timerid, 
									SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
									&clocks);
				}					
			}
			else //Slave Mode
			{
				//Wird derzeit nicht unterstützt
			}
		}	
	}
}

static int SIMULAPI SPI_TimerElapsed(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    SPI      *spi = (SPI *)private;
	simulTime clocks;
	simulWord SCBR;
	simulWord DLYBCS;
	simulWord DLYBS;
	simulWord BITS;
	simulWord DLYBCT;
	
	//28.6.3.2 Flow Master Mode Flow Diagram
	switch(spi->serializer.mode)
	{
		case SERIALIZER_RESET:  //OFF durch SoftwareAnforderung (AT91C_SPI_SWRST)
			spi->serializer.mode = SERIALIZER_OFF;
			SIMUL_StopTimer(processor, spi->serializer.timerid);
			break;
		case SERIALIZER_OFF:	//OFF Selbstständig nach Übertragung des letzten Bytes
							    //In diesem Zustand sollte somit nie auftreten können
			break;
		case SERIALIZER_START:
			//TDRE=0, ansonsten wären wir nicht hier!
			if(                       (spi->serializer.npcs != 0xff) &&
			   (spi->at91s_spi.SPI_CSR[spi->serializer.npcs>>2] & AT91C_SPI_CSAAT))
			{	//CSAAT == 1
SERIALIZER_START_LABEL:		
				if(spi->at91s_spi.SPI_MR & AT91C_SPI_PS)
				{	//PS == 1 (Variable Peripheral)
					if(spi->serializer.npcs == NPCS_CALCULATE(spi->at91s_spi.SPI_TDR))
					{
						goto SERIALIZER_DLYBS_LABEL;
					}
					spi->serializer.npcs = NPCS_CALCULATE(spi->at91s_spi.SPI_TDR);
				}
				else
				{	//PS == 0 (Fixed Peripheral)
					if(spi->serializer.npcs == NPCS_CALCULATE(spi->at91s_spi.SPI_MR))
					{
						goto SERIALIZER_DLYBS_LABEL;
					}
					spi->serializer.npcs = NPCS_CALCULATE(spi->at91s_spi.SPI_MR);
				}
				
				//NPCS = 0xf  bewusst hier ausgelassen, 
				//sonst wäre anschließend wieder das Laden von NPCS notwendig
				
				//If DLYBCS is less than or equal to six, six MCK periods will be 
				//inserted by default.
				//Otherwise, delay Delay Between Chip Selects = CLYBCS/MCK
				DLYBCS = (spi->at91s_spi.SPI_MR>>24)&0xFF;
				if(DLYBCS <= 6)
					clocks=6;
				else
					clocks=DLYBCS;
				spi->serializer.mode = SERIALIZER_DLYBCS1;
				SIMUL_StartTimer(processor, spi->serializer.timerid, 
										SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
										&clocks);
				break;
			}
			else
			{	//CSAAT = 0
				if(spi->at91s_spi.SPI_MR & AT91C_SPI_PS)
				{  	//PS == 1 (Variable Peripheral)
					spi->serializer.npcs = NPCS_CALCULATE(spi->at91s_spi.SPI_TDR);
				}
				else
				{	//PS == 0 (Fixed Peripheral)
					spi->serializer.npcs = NPCS_CALCULATE(spi->at91s_spi.SPI_MR);
				}
			}
			//Break bewusst ausgelassen
		case SERIALIZER_DLYBCS1:
			spi->serializer.mode = SERIALIZER_DLYBS;

			//When DLYBS equals zero, the NPCS valid to SPCK transition is 1/2 the 
			//SPCK clock period.  Otherwise: Delay Before SPCK = DLYBS / MCK
			DLYBS = (spi->at91s_spi.SPI_CSR[spi->serializer.npcs>>2]>>16)&0xFF;
			SCBR  = (spi->at91s_spi.SPI_CSR[spi->serializer.npcs>>2]>> 8)&0xFF;
			if(DLYBS == 0)
				clocks=SCBR >= 2 ? SCBR>>1 : 1;
			else
				clocks=DLYBS;
			SIMUL_StartTimer(processor, spi->serializer.timerid, 
		                                SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
		                               &clocks);
			break;
		case SERIALIZER_DLYBS:
SERIALIZER_DLYBS_LABEL:		
			spi->serializer.tdr  = spi->at91s_spi.SPI_TDR;
			spi->at91s_spi.SPI_SR |= AT91C_SPI_TDRE;
			
			BITS=(spi->at91s_spi.SPI_CSR[spi->serializer.npcs>>2]>>4)&0x00F;
			if(BITS >= 8)
				clocks=16;
			else
				clocks=8+BITS;
			
			SCBR=(spi->at91s_spi.SPI_CSR[spi->serializer.npcs>>2]>>8)&0xFF;
			if(SCBR == 0)
			{
				SIMUL_Warning(processor,"Error-SPI: SPI_CSR.SCBR == 0\n");
				SCBR = 1;
			}
			else
			{
				clocks*=SCBR;
			}			
			spi->serializer.mode = SERIALIZER_DATATRANSFER;
			SIMUL_StartTimer(processor, spi->serializer.timerid, 
		                                SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
		                               &clocks);
			break;
		case SERIALIZER_DATATRANSFER:
			lcd_sendreceive(processor,spi->nxt,spi->serializer.tdr,&spi->serializer.rdr);
		
			if(spi->at91s_spi.SPI_MR & AT91C_SPI_LLB)
				spi->at91s_spi.SPI_RDR = spi->serializer.tdr;
			else
				spi->at91s_spi.SPI_RDR = spi->serializer.rdr;
			
			//An overrun occurs when SPI_RDR is loaded at least twice from the 
			//serializer since the last read of the SPI_RDR
			if(spi->at91s_spi.SPI_SR & AT91C_SPI_RDRF)
				spi->at91s_spi.SPI_SR |= AT91C_SPI_OVRES;
			
			spi->at91s_spi.SPI_SR |= AT91C_SPI_RDRF;

			//LASTXFER: Last Transfer (Wird nicht berücksichtigt)
			
			//When DLYBCT equals zero, no delay between consecutive transfers is inserted 
			//and the clock keeps its duty cycle over the chracter transfer. 
			//Otherwise: Delay Between Consecutive Transfers = 32 * DLYBCT /  MCK
			DLYBCT=(spi->at91s_spi.SPI_CSR[spi->serializer.npcs>>2]>>24)&0xFF;
			if(DLYBCT == 0)
				clocks = 1;
			else
				clocks=32 * DLYBCT;
			spi->serializer.mode = SERIALIZER_DLYBCT;
			SIMUL_StartTimer(processor, spi->serializer.timerid, 
		                                SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
		                               &clocks);
			break;
		case SERIALIZER_DLYBCT:
			if((spi->at91s_spi.SPI_SR & AT91C_SPI_TDRE) == 0)
			{
				spi->serializer.mode = SERIALIZER_START;
				goto SERIALIZER_START_LABEL;
			}
			else
			{
				//The end of transfer is indicated by the TXEMPTY flag in the SPI_SR register
				//If a transfer delay (DLYBCT) is greater than 0 for the last transfer,
				//TXEMPTY is set after the completion of said delay. The master clock MCK
				//can be switched off at this time
				spi->at91s_spi.SPI_SR |= AT91C_SPI_TXEMPTY;
				
				if(spi->at91s_spi.SPI_CSR[spi->serializer.npcs>>2] & AT91C_SPI_CSAAT)
				{	//CSAAT == 1
			
					spi->serializer.mode = SERIALIZER_OFF;
					SIMUL_StopTimer(processor, spi->serializer.timerid);
				}
				else
				{	//CSAAT == 0
					spi->serializer.npcs=0xFF;
					//If DLYBCS is less than or equal to six, six MCK periods will be 
					//inserted by default.
					//Otherwise, delay Delay Between Chip Selects = CLYBCS/MCK
					DLYBCS = (spi->at91s_spi.SPI_MR>>24)&0xFF;
					if(DLYBCS <= 6)
						clocks=6;
					else
						clocks=DLYBCS;
					spi->serializer.mode = SERIALIZER_DLYBCS2;
					SIMUL_StartTimer(processor, spi->serializer.timerid, 
	                                SIMUL_TIMER_REL | SIMUL_TIMER_CLOCKS | SIMUL_TIMER_SINGLE,
	                               &clocks);
				}
			}				
			break;
		case SERIALIZER_DLYBCS2:
			spi->serializer.mode = SERIALIZER_OFF;
			SIMUL_StopTimer(processor, spi->serializer.timerid);
			break;
	}
	
	pdc_idle(processor,private);
	interrupt_check(processor,private);
    return SIMUL_TIMER_OK;
}
	
/**************************************************************************/

static int SIMULAPI SPI_PortWrite(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	SPI *spi = (SPI *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_CR))
	{   //(SPI) Control Register (Offset: 0x00) Write-Only
		spi->at91s_spi.SPI_CR = cbs->x.bus.data;
		if((cbs->x.bus.data & (AT91C_SPI_SPIEN | AT91C_SPI_SPIDIS)) == (AT91C_SPI_SPIEN | AT91C_SPI_SPIDIS))
		{
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_SPIENS;
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_TDRE;
		}
		if((cbs->x.bus.data & (AT91C_SPI_SPIEN | AT91C_SPI_SPIDIS)) == (                  AT91C_SPI_SPIDIS))
		{
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_SPIENS;
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_TDRE;
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_TXEMPTY;
		}
		if((cbs->x.bus.data & (AT91C_SPI_SPIEN | AT91C_SPI_SPIDIS)) == (AT91C_SPI_SPIEN                   ))
		{
			spi->at91s_spi.SPI_SR |=  AT91C_SPI_SPIENS;
			spi->at91s_spi.SPI_SR |=  AT91C_SPI_TDRE;
			spi->at91s_spi.SPI_SR |=  AT91C_SPI_TXEMPTY;
		}
		  
		if(cbs->x.bus.data & AT91C_SPI_SWRST)
		{
			SPI_PortReset(processor, (simulCallbackStruct *) 1,private);
			
			//Errata 40.14.8.9 SPI: Software Reset and SPIEN Bit
			//The SPI Command "software reset" does not reset the SPIEN config bit.
			//Therefore rewriting an SPI enables command does not set TX_READY,TX_EMPTy flags.
			//Problem Fix / Workaround
			//Send SPI disable command after a software reset
		}
		if(cbs->x.bus.data & AT91C_SPI_LASTXFER)
		{
		}
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_MR))
	{   //(SPI) Mode Register (Offset: 0x04) Read/Write
		spi->at91s_spi.SPI_MR = cbs->x.bus.data & (AT91C_SPI_DLYBCS | AT91C_SPI_PCS     | 
		                                           AT91C_SPI_LLB    | AT91C_SPI_MODFDIS |
												   AT91C_SPI_PCSDEC | AT91C_SPI_PS      |
												   AT91C_SPI_MSTR);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TDR))
	{   //(SPI) Transmit Data Register (Offset: 0x0C) Write-Only
		spi->at91s_spi.SPI_TDR  =  cbs->x.bus.data;
		//The transfer of a data written in SPI_TDR in the Shift Register
		//is indicated by the TDRE bit. When new data is written in SPI_TDR,
		//this bit is cleared. The TDRE bit is used to trigger the Transmit 
		//PDC channel
		spi->at91s_spi.SPI_SR &= ~AT91C_SPI_TDRE;
		
		spi->at91s_spi.SPI_SR &= ~AT91C_SPI_TXEMPTY;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_IER))
	{   //(SPI) Interrupt Enable Register (Offset: 0x14) Write-Only
		spi->at91s_spi.SPI_IER  = cbs->x.bus.data;
		spi->at91s_spi.SPI_IMR |= cbs->x.bus.data & (AT91C_SPI_RDRF  | AT91C_SPI_TDRE  |
		                                             AT91C_SPI_MODF  | AT91C_SPI_OVRES |
													 AT91C_SPI_ENDRX | AT91C_SPI_ENDTX |
													 AT91C_SPI_RXBUFF| AT91C_SPI_TXBUFE|
													 AT91C_SPI_NSSR  | AT91C_SPI_TXEMPTY);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_IDR))
	{   //(SPI) Interrupt Disable Register (Offset: 0x18) Write-Only
		spi->at91s_spi.SPI_IDR  =  cbs->x.bus.data;
		spi->at91s_spi.SPI_IMR &= ~cbs->x.bus.data & (AT91C_SPI_RDRF  | AT91C_SPI_TDRE  |
		                                              AT91C_SPI_MODF  | AT91C_SPI_OVRES |
										  			  AT91C_SPI_ENDRX | AT91C_SPI_ENDTX |
											  		  AT91C_SPI_RXBUFF| AT91C_SPI_TXBUFE|
												  	  AT91C_SPI_NSSR  | AT91C_SPI_TXEMPTY);
	}
	else if((cbs->x.bus.width == 4*8)                                && 
	       ((cbs->x.bus.address & 0x03) == 0)                        &&
	        (cbs->x.bus.address >= ((simulWord) AT91C_SPI_CSR)+0x00) &&
	        (cbs->x.bus.address <  ((simulWord) AT91C_SPI_CSR)+0x10))
	{   //(SPI) Chip Select Register (Offset: 0x30..0x3F) Read/Write
		simulWord CS=(cbs->x.bus.address-(simulWord) AT91C_SPI_CSR)/4;
		spi->at91s_spi.SPI_CSR[CS]=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RPR))
	{   //(PDC_SPI) Receive Pointer Register (Offset: 0x100) Read/Write
		spi->at91s_spi.SPI_RPR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RCR))
	{   //(PDC_SPI) Receive Counter Register (Offset: 0x104) Read/Write
		spi->at91s_spi.SPI_RCR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TPR))
	{   //(PDC_SPI) Transmit Pointer Register (Offset: 0x108) Read/Write
		spi->at91s_spi.SPI_TPR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TCR))
	{   //(PDC_SPI) Transmit Counter Register (Offset: 0x10C) Read/Write
		spi->at91s_spi.SPI_TCR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RNPR))
	{   //(PDC_SPI) Receive Next Pointer Register (Offset: 0x110) Read/Write
		spi->at91s_spi.SPI_RNPR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RNCR))
	{   //(PDC_SPI) Receive Next Counter Register (Offset: 0x114) Read/Write
		spi->at91s_spi.SPI_RNCR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TNPR))
	{   //(PDC_SPI) Transmit Next Pointer Register (Offset: 0x118) Read/Write
		spi->at91s_spi.SPI_TNPR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TNCR))
	{   //(PDC_SPI) Transmit Next Counter Register (Offset: 0x11C) Read/Write
		spi->at91s_spi.SPI_TNCR=cbs->x.bus.data;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_PTCR))
	{   //(PDC_SPI) PDC Transfer Control Register (Offset: 0x12) Write-Only
		spi->at91s_spi.SPI_PTCR = cbs->x.bus.data;
		if((cbs->x.bus.data & (AT91C_PDC_RXTEN | AT91C_PDC_RXTDIS))  == AT91C_PDC_RXTEN)
			spi->at91s_spi.SPI_PTSR |= AT91C_PDC_RXTEN;
		if((cbs->x.bus.data & (                  AT91C_PDC_RXTDIS))  == AT91C_PDC_RXTDIS)
			spi->at91s_spi.SPI_PTSR &= ~AT91C_PDC_RXTEN;
		if((cbs->x.bus.data & (AT91C_PDC_TXTEN | AT91C_PDC_TXTDIS))  == AT91C_PDC_TXTEN)
			spi->at91s_spi.SPI_PTSR |= AT91C_PDC_TXTEN;
		if((cbs->x.bus.data & (                  AT91C_PDC_TXTDIS))  == AT91C_PDC_TXTDIS)
			spi->at91s_spi.SPI_PTSR &= ~AT91C_PDC_TXTEN;
	}
	else
	{
SIMUL_Printf(processor,"SPI_PortWrite(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}

	serializer_idle(processor,private);
	pdc_idle(processor,private);
	interrupt_check(processor,private);
	
	return SIMUL_MEMORY_OK;
}


static int SIMULAPI SPI_PortRead(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
	SPI *spi = (SPI *) private;

	if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_MR))
	{   //(SPI) Mode Register (Offset: 0x00) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_MR & (AT91C_SPI_DLYBCS | AT91C_SPI_PCS     | 
		                                         AT91C_SPI_LLB    | AT91C_SPI_MODFDIS |
												 AT91C_SPI_PCSDEC | AT91C_SPI_PS      |
												 AT91C_SPI_MSTR);
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RDR))
	{   //(SPI) Receive Data Register (Offset: 0x08) Read-Only
		cbs->x.bus.data=spi->at91s_spi.SPI_RDR;

		if(spi->debug == 0)
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_RDRF;

	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_SR))
	{   //(SPI) Status Register (Offset: 0x10) Read-Only
		cbs->x.bus.data=spi->at91s_spi.SPI_SR & (AT91C_SPI_RDRF   | AT91C_SPI_TDRE   |
		                                         AT91C_SPI_MODF   | AT91C_SPI_OVRES  |
												 AT91C_SPI_ENDRX  | AT91C_SPI_ENDTX  |
												 AT91C_SPI_RXBUFF | AT91C_SPI_TXBUFE |
												 AT91C_SPI_NSSR   | AT91C_SPI_TXEMPTY|
												 AT91C_SPI_SPIENS);
												 
		//When a mode fault is detected, the MODF bit in the SPI_SR is set until 
		//the SPI_SR is read and the SPI is automatically disabled until 
		//re-enabled by writing the SPIEN bit in the SPI_CR at 1
		if((spi->at91s_spi.SPI_SR & AT91C_SPI_MODF) && (spi->debug == 0))
		{
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_MODF;
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_SPIENS;
		}
		if((spi->at91s_spi.SPI_SR & AT91C_SPI_OVRES) && (spi->debug == 0))
			spi->at91s_spi.SPI_SR &= ~AT91C_SPI_OVRES;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_IMR))
	{   //(SPI) Interrupt Mask Register (Offset: 0x1C) Read-Only
		cbs->x.bus.data=spi->at91s_spi.SPI_IMR & (AT91C_SPI_RDRF  | AT91C_SPI_TDRE  |
		                                          AT91C_SPI_MODF  | AT91C_SPI_OVRES |
												  AT91C_SPI_ENDRX | AT91C_SPI_ENDTX |
												  AT91C_SPI_RXBUFF| AT91C_SPI_TXBUFE|
												  AT91C_SPI_NSSR  | AT91C_SPI_TXEMPTY);
	}
	else if((cbs->x.bus.width == 4*8)                                && 
	       ((cbs->x.bus.address & 0x03) == 0)                        &&
	        (cbs->x.bus.address >= ((simulWord) AT91C_SPI_CSR)+0x00) &&
	        (cbs->x.bus.address <  ((simulWord) AT91C_SPI_CSR)+0x80))
	{   // (SPI) Chip Select Register (Offset: 0x30..0x3F) Read/Write
		simulWord CS=(cbs->x.bus.address-(simulWord) AT91C_SPI_CSR)/4;
		cbs->x.bus.data=spi->at91s_spi.SPI_CSR[CS];
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RPR))
	{   //(PDC_SPI) Receive Pointer Register (Offset: 0x100) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_RPR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RCR))
	{   //(PDC_SPI) Receive Counter Register (Offset: 0x104) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_RCR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TPR))
	{   //(PDC_SPI) Transmit Pointer Register (Offset: 0x108) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_TPR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TCR))
	{   //(PDC_SPI) Transmit Counter Register (Offset: 0x10C) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_TCR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RNPR))
	{   //(PDC_SPI) Receive Next Pointer Register (Offset: 0x110) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_RNPR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_RNCR))
	{   //(PDC_SPI) Receive Next Counter Register (Offset: 0x114) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_RNCR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TNPR))
	{   //(PDC_SPI) Transmit Next Pointer Register (Offset: 0x118) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_TNPR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_TNCR))
	{   //(PDC_SPI) Transmit Next Counter Register (Offset: 0x11C) Read/Write
		cbs->x.bus.data=spi->at91s_spi.SPI_TNCR;
	}
	else if((cbs->x.bus.width == 4*8) && (cbs->x.bus.address == (simulWord) AT91C_SPI_PTSR))
	{   //(PDC_SPI) PDC Transfer Status Register (Offset: 0x124) Read-Only
		cbs->x.bus.data=spi->at91s_spi.SPI_PTSR & (AT91C_PDC_RXTEN | AT91C_PDC_TXTEN);
	}
	else
	{
SIMUL_Printf(processor,"SPI_PortRead(Fail) %x %x %x \n",cbs->x.bus.width,cbs->x.bus.data,cbs->x.bus.address);
		return SIMUL_MEMORY_FAIL;
	}
	
	serializer_idle(processor,private);
	pdc_idle(processor,private);
	interrupt_check(processor,private);
	
	return SIMUL_MEMORY_OK;
}

/**************************************************************************/

static int SIMULAPI SPI_Go(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    SPI *spi = (SPI *) private;
	spi->debug=0;
}

static int SIMULAPI SPI_Break(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    SPI *spi = (SPI *) private;
	spi->debug=1;
}

static int SIMULAPI SPI_PortReset(simulProcessor processor, simulCallbackStruct * cbs, simulPtr private)
{
    SPI *spi = (SPI *) private;
	simulWord CS;
	
	spi->debug              = 0x1;

	for(CS=0;CS<4;CS++)
	{
		spi->at91s_spi.SPI_CSR[CS]=0x0;
	}
	spi->at91s_spi.SPI_CR    = 0x0;
	spi->at91s_spi.SPI_MR    = 0x0;
	spi->at91s_spi.SPI_RDR   = 0x0;
	spi->at91s_spi.SPI_TDR   = 0x0;
	spi->at91s_spi.SPI_SR	 = 0xF0;
	spi->at91s_spi.SPI_IER   = 0x0;
	spi->at91s_spi.SPI_IDR   = 0x0;
	spi->at91s_spi.SPI_IMR   = 0x0;
	
	spi->serializer.mode     = SERIALIZER_RESET;
	spi->serializer.tdr      = 0x0;
	spi->serializer.rdr      = 0x0;
	spi->serializer.npcs     = 0xFF;

	if(cbs != (simulCallbackStruct *) 1)
	{
		spi->at91s_spi.SPI_RPR   = 0x0;     // Receive Pointer Register
		spi->at91s_spi.SPI_RCR   = 0x0;     // Receive Counter Register
		spi->at91s_spi.SPI_TPR   = 0x0;     // Transmit Pointer Register
		spi->at91s_spi.SPI_TCR   = 0x0;     // Transmit Counter Register
		spi->at91s_spi.SPI_RNPR  = 0x0;     // Receive Next Pointer Register
		spi->at91s_spi.SPI_RNCR  = 0x0;     // Receive Next Counter Register
		spi->at91s_spi.SPI_TNPR  = 0x0;     // Transmit Next Pointer Register
		spi->at91s_spi.SPI_TNCR  = 0x0;     // Transmit Next Counter Register
		spi->at91s_spi.SPI_PTCR  = 0x0;     // PDC Transfer Control Register
		spi->at91s_spi.SPI_PTSR  = 0x0;     // PDC Transfer Status Register
	}
	
    return SIMUL_RESET_OK;
}


void SPI_PortInit(simulProcessor processor,void *nxt)
{
	SPI            *spi;
    simulWord       from, to;
	
	spi = (SPI *)SIMUL_Alloc(processor, sizeof(SPI));
	spi->nxt=nxt;
	
	SIMUL_Printf(processor,"-> Peripherie SPI loaded (ohne Gewähr, nicht vollständig getestet, nur Master-Mode)\n");

	//Manuell Aufrufen, zur Konfiguration der internen Daten
	SPI_PortReset(processor,NULL, (simulPtr) spi);
	
    SIMUL_RegisterResetCallback(processor, SPI_PortReset, (simulPtr) spi);

    from = (simulWord) AT91C_SPI_CR; //DJ:AT91C_BASE_SPI;
    to   = (simulWord) AT91C_SPI_CR  /*DJ:AT91C_BASE_SPI*/+sizeof(AT91S_SPI)-1;
    SIMUL_RegisterBusWriteCallback(processor, SPI_PortWrite, (simulPtr) spi, 0, &from, &to);
    SIMUL_RegisterBusReadCallback (processor, SPI_PortRead,  (simulPtr) spi, 0, &from, &to);

    spi->serializer.timerid = SIMUL_RegisterTimerCallback(processor, SPI_TimerElapsed, (simulPtr) spi);

	SIMUL_RegisterGoCallback   (processor, SPI_Go,    (simulPtr) spi);
	SIMUL_RegisterBreakCallback(processor, SPI_Break, (simulPtr) spi);

}
