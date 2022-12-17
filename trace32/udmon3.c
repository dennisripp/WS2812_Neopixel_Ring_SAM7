/**************************************************************************
 Aus deubgger_arm.pdrf
 if SYStem.MemAccess ist not denied, it is possible to read from memory, 
 to wirte to memory and to set software breakpoints while the CPU
 is executing the programm. This requeires one of the follow monitors
 - CERBERUS (Infineon)
 - CPU (Instruction Set simulator)
 - DAP (Debug Access Port)
 - NEXUS
 - TSMON3: A run-time memory access is done via a Time Sharing Monitor
   The application is responsible for calling the monitor code periodically.
   The call ist typically include in a periodic interrupt or int the idle 
   task of the kernel. (runtime_memory_access)
   Besides runtime memory access TSMON3 would allow run mode debugging.
   But manual break is not possible with TSMON 3 and could only be emulated
   by polling the DCC port. 
 - PTMON3: A run-time memory access is done via a PULSE Triggered Monitor
   Whenever the debugger wants to perform a memory access while the program
   is running, the deubber generates a trigger for the trigger bus. If the
   trigger bus in configured appropriate (TrBus), this trigger is output
   via the TRIGGER connector of the TRACE32 development tool. The TRIGGER
   output can be connected to an external interrupt in order to call a 
   monitor.  (runtime_memory_access)
   Besides runtime memory acces PTMON3 would allow run mode debugging.
   But manual break is not possible with PTMON3 and could only be emulated
   by polling the DCC port.
 - UDMON3: A run-time memory access is done via a Usermode Debug Monitor
   The application is responsible for calling the montiro code periodically.
   The call is typically include in a periodic interrupt or in the idle task
   of the kernel. For runtime memory access UDMON3 behaves exactly as TSMON3. 
   (runtime_memory_access)
   Besides runtime memory access UDMON3 allows run mode debugging.
   Handling of interrupts when the application is stopped is possible when
   the background monitor is activated. On-chip breakpoints and manual 
   program break are only possible when the application runs in user (USR)
   mode. (background_monitor)
**************************************************************************/
#include <stdio.h>
#include "../term.h"
#include "../lib/systick.h"

//DCC-Schnittstelle wird wie folgt genutzt
// - Im RAM-Modus
//      Zugriff auf Speicher über '%e' Befehle
//      Zugriff auf MMU/Cache (jedoch in AT91SAM7 nicht vorhanden)
//      Terminal Datenaustausch
// - Im Simulations-Modus
//      Terminal Datenaustausch

#ifndef MODE_SIM  
#include "../lib/nxt_lcd.h"
#include "../lib/display.h"

//aus sim_NXT/sim_NXT.h
#define LCD_MEMORY_OFFSET  0x10000000
#define NXT_MEMORY_OFFSET  0x20000000

//aus sim_NXT/nxt.c
#define LCD_X      25
#define LCD_Y      50
#define LCD_WIDTH 100
#define LCD_HIGH   64

#define NXT_WIDTH 150
#define NXT_HIGH  250

//Zur Sicherheit Daten aus sim_NXT mit Daten aus nxt_lcd vergleichen
#if (NXT_LCD_DEPTH*8 != LCD_HIGH)
#error "LCD_HIGH != NXT_LCD_DEPTH"
#endif
#if (NXT_LCD_WIDTH != LCD_WIDTH)
#error "LCD_WIDTH != NXT_LCD_WIDTH"
#endif

static unsigned char *display_buffer=NULL;
#endif

void _udmon3_handler(void);
static SYSTICK_VL udmon3_systick_vl;

typedef struct
{
	unsigned short rd;
	unsigned short wr;
	unsigned char  buf[TERM_SIZE];
} FIFO_TYPE;

static FIFO_TYPE fifo_recv = { 0,0 };
static FIFO_TYPE fifo_send = { 0,0 };
	
static int fifo_push(FIFO_TYPE *fifo,unsigned char val)
{
	if(((fifo->wr+1)&(TERM_SIZE-1)) == fifo->rd)
		return(-1);
	
	fifo->buf[fifo->wr]=val;
	fifo->wr=(fifo->wr+1)&(TERM_SIZE-1);
	return(0);
}

static int fifo_pop(FIFO_TYPE *fifo,unsigned char *val)
{
	if(fifo->rd == fifo->wr)
		return(-1);
		
	*val=fifo->buf[fifo->rd];
	fifo->rd=(fifo->rd+1)&(TERM_SIZE-1);
	return(0);
}

int _term_read_available(void)
{
	if(fifo_recv.rd == fifo_recv.wr)
		return(-1);
		
	return(0);
}

int _term_read(unsigned char *c)
{
	return(fifo_pop(&fifo_recv,c));
}

int _term_write_possible(void)
{
	if(((fifo_send.wr+1)&(TERM_SIZE-1)) == fifo_send.rd)
		return(-1);
		
	return(0);
}

void _term_char(int c)
{
	//Bedingt, dass der Sendepuffer über Interruptroutine geleert wird
	while( fifo_push(&fifo_send,c) != 0);
}

void _term_string(const char *str)
{
	do
	{
		while(fifo_push(&fifo_send,*str) != 0);
		str++;
	}
	while(*str != 0);
}


void _term_hex(unsigned int val, unsigned int places)
{
  char x[9];

  char *p = &x[8];
  unsigned int p_count = 0;

  *p = 0;

  if (places > 8)
    places = 8;

  while (val) {
    p--;
    p_count++;
    *p = "0123456789ABCDEF"[val & 0x0f];
    val >>= 4;
  }

  while (p_count < places) {
    p--;
    p_count++;
    *p = '0';
  }

  _term_string(p);
}

void _term_unsigned_worker(unsigned int val, unsigned int places, unsigned int sign)
{
  char x[12];			// enough for 10 digits + sign + NULL 

  char *p = &x[11];
  unsigned int   p_count = 0;

  *p = 0;

  if (places > 11)
    places = 11;

  while (val) {
    p--;
    p_count++;
    *p = (val % 10) + '0';
    val /= 10;
  }

  if (!p_count) {
    p--;
    p_count++;
    *p = '0';
  }

  if (sign) {
    p--;
    p_count++;
    *p = '-';
  }

  while (p_count < places) {
    p--;
    p_count++;
    *p = ' ';
  }

  _term_string(p);
}

void _term_init(void)
{
  /* Callback Routine einhängen */
  systick_callback(&udmon3_systick_vl,_udmon3_handler);
}

/**************************************************************************

  CP15 access  / MMU + Cache

  Does not support all CP15 register. Can be extended appropriate to your need.

  ARM9 requires the CP15 accesses to be done in a privileged mode (not in
  user mode). In this example the Monitor_Handler will be called from the
  monitor (privileged mode), but also from the application (user mode).
  Therefore do not access CP15 register while the application is running
  (e.g. by 'Data.In EC15:0x0001 /Long').

**************************************************************************/
#if 0
static unsigned int Monitor_ReadCP15 (unsigned int address)
{
  unsigned int data = 0;

  switch (address)
  {
    case 0xf000:
		asm volatile("MRC p15, 0, %0  , c0, c0" : "=r" (data));
		break;
    case 0xf001:
		asm volatile("MRC p15, 0, %0  , c1, c0" : "=r" (data));
		break;
    case 0xf002:
		asm volatile("MRC p15, 0, %0  , c2, c0" : "=r" (data));
		break;
    case 0xf003:
		asm volatile("MRC p15, 0, %0  , c3, c0" : "=r" (data));
		break;
    case 0xf004:
		asm volatile("MRC p15, 0, %0  , c4, c0" : "=r" (data));
		break;
    case 0xf005:
		asm volatile("MRC p15, 0, %0  , c5, c0" : "=r" (data));
		break;
  }
  return data;
}

static void Monitor_WriteCP15 (unsigned int address, unsigned int data)
{
  switch (address)
  {
    case 0xf000:
		asm volatile("mcr p15, 0, %0, c0, c0" : : "r" (data));
		break;
    case 0xf001:
		asm volatile("mcr p15, 0, %0, c1, c0" : : "r" (data));
		break;
    case 0xf002:
		asm volatile("mcr p15, 0, %0, c2, c0" : : "r" (data));
		break;
    case 0xf003:
		asm volatile("mcr p15, 0, %0, c3, c0" : : "r" (data));
		break;
    case 0xf004:
		asm volatile("mcr p15, 0, %0, c4, c0" : : "r" (data));
		break;
    case 0xf005:
		asm volatile("mcr p15, 0, %0, c5, c0" : : "r" (data));
		break;
  }
}
#endif

/**************************************************************************

  CP14 access  / Debug Schnittstelle
  
  ARM family dependent DCC driver functions

**************************************************************************/

static unsigned int DCC_SendStatus (void)
{
	int status;

	asm volatile("mrc p14, 0, %0    , c0, c0" : "=r" (status));

	return (status & 2);
}

static void DCC_SendWord (unsigned int data)
{
	asm volatile("mcr p14, 0, %0, c1, c0" : : "r" (data));
}

static unsigned int DCC_ReceiveStatus (void)
{
	int  status;

	asm volatile("mrc p14, 0, %0    , c0, c0" : "=r" (status));

	return (status & 1);
}

static unsigned int DCC_ReceiveWord (void)
{
	unsigned int data;

	asm volatile("mrc p14, 0, %0  , c1, c0" : "=r" (data));

	return data;
}


/**************************************************************************

  memory access

**************************************************************************/

#ifndef MODE_SIM  
static void Monitor_ReadByte (void * buf, void * address, int len)
{
  int i;
  unsigned char *source = (unsigned char *) address;
  unsigned char *target = (unsigned char *) buf;

  if((int)source & 0xffC00000)
  {
	if(display_buffer == NULL)
		display_buffer = display_get_buffer();
		
	for (i = 0; i < len; i++)
	{ 
		int lcd_x = (((int)&source[i] - LCD_MEMORY_OFFSET) % NXT_WIDTH) - LCD_X;
		int lcd_y = (((int)&source[i] - LCD_MEMORY_OFFSET) / NXT_WIDTH) - LCD_Y;
		if((lcd_y >= 0) && (lcd_y < LCD_HIGH) && (lcd_x >= 0) && (lcd_x < LCD_WIDTH))
			target[i] = display_buffer[((lcd_y/8)*LCD_WIDTH) + lcd_x] & (1<<(lcd_y%8)) ? 200 : 50;
		else
			target[i] =0;
	}
  }
  else
	for (i = 0; i < len; i++)
		target[i] = source[i];
}


static void Monitor_ReadHalf (void * buf, void * address, int len)
{
  int i;
  unsigned short *source = (unsigned short *) address;
  unsigned short *target = (unsigned short *) buf;

  if((int)source & 0xffC00000)
  {
	if(display_buffer == NULL)
		display_buffer = display_get_buffer();
		
	for (i = 0; i < len; i++)
	{
		int lcd_x = (((int)&source[i] - LCD_MEMORY_OFFSET) % NXT_WIDTH) - LCD_X;
		int lcd_y = (((int)&source[i] - LCD_MEMORY_OFFSET) / NXT_WIDTH) - LCD_Y;
		if((lcd_y >= 0) && (lcd_y < LCD_HIGH) && (lcd_x >= 0) && (lcd_x < LCD_WIDTH))
			target[i] = (target[i]&0xff00) | ((display_buffer[((lcd_y/8)*LCD_WIDTH) + lcd_x] & (1<<(lcd_y%8)) ? 200 : 50) << 0);
		else
			target[i] = (target[i]&0xff00) | (0                                                                           << 0);
		lcd_x++;
		if((lcd_y >= 0) && (lcd_y < LCD_HIGH) && (lcd_x >= 0) && (lcd_x < LCD_WIDTH))
			target[i] = (target[i]&0x00ff) | ((display_buffer[((lcd_y/8)*LCD_WIDTH) + lcd_x] & (1<<(lcd_y%8)) ? 200 : 50) << 8);
		else
			target[i] = (target[i]&0x00ff) | (0                                                                           << 8);
	}
  }
  else
	for (i = 0; i < len; i++)
		target[i] = source[i];
}


static void Monitor_ReadWord (void * buf, void * address, int len)
{
  int i;
  unsigned int *source = (unsigned int *) address;
  unsigned int *target = (unsigned int *) buf;

  if((int)source & 0xffC00000)
  {
	if(display_buffer == NULL)
		display_buffer = display_get_buffer();
		
	for (i = 0; i < len; i++)
	{
		int lcd_x = (((int)&source[i] - LCD_MEMORY_OFFSET) % NXT_WIDTH) - LCD_X;
		int lcd_y = (((int)&source[i] - LCD_MEMORY_OFFSET) / NXT_WIDTH) - LCD_Y;
		if((lcd_y >= 0) && (lcd_y < LCD_HIGH) && (lcd_x >= 0) && (lcd_x < LCD_WIDTH))
			target[i] = (target[i]&0xffffff00) | ((display_buffer[((lcd_y/8)*LCD_WIDTH) + lcd_x] & (1<<(lcd_y%8)) ? 200 : 50) << 0);
		else
			target[i] = (target[i]&0xffffff00) | (0                                                                           << 0);
		lcd_x++;
		if((lcd_y >= 0) && (lcd_y < LCD_HIGH) && (lcd_x >= 0) && (lcd_x < LCD_WIDTH))
			target[i] = (target[i]&0xffff00ff) | ((display_buffer[((lcd_y/8)*LCD_WIDTH) + lcd_x] & (1<<(lcd_y%8)) ? 200 : 50) << 8);
		else
			target[i] = (target[i]&0xffff00ff) | (0                                                                           << 8);
		lcd_x++;
		if((lcd_y >= 0) && (lcd_y < LCD_HIGH) && (lcd_x >= 0) && (lcd_x < LCD_WIDTH))
			target[i] = (target[i]&0xff00ffff) | ((display_buffer[((lcd_y/8)*LCD_WIDTH) + lcd_x] & (1<<(lcd_y%8)) ? 200 : 50) << 16);
		else
			target[i] = (target[i]&0xff00ffff) | (0                                                                           << 16);
		lcd_x++;
		if((lcd_y >= 0) && (lcd_y < LCD_HIGH) && (lcd_x >= 0) && (lcd_x < LCD_WIDTH))
			target[i] = (target[i]&0x00ffffff) | ((display_buffer[((lcd_y/8)*LCD_WIDTH) + lcd_x] & (1<<(lcd_y%8)) ? 200 : 50) << 24);
		else
			target[i] = (target[i]&0x00ffffff) | (0                                                                           << 24);
	}
  }
  else
	for (i = 0; i < len; i++)
		target[i] = source[i];
		
}

static void Monitor_WriteByte (void * buf, void * address, int len)
{
  int i;
  unsigned char *source = (unsigned char *) address;
  unsigned char *target = (unsigned char *) buf;

  if(!((int)target & 0xffC00000))
	for (i = 0; i < len; i++)
		target[i] = source[i];
}

static void Monitor_WriteHalf (void * buf, void * address, int len)
{
  int i;
  unsigned short *source = (unsigned short *) address;
  unsigned short *target = (unsigned short *) buf;

  if(!((int)target & 0xffC00000))
	for (i = 0; i < len; i++)
		target[i] = source[i];
}

static void Monitor_WriteWord (void * buf, void * address, int len)
{
  int i;
  unsigned int *source = (unsigned int *) address;
  unsigned int *target = (unsigned int *) buf;

  if(!((int)target & 0xffC00000))
	for (i = 0; i < len; i++)
		target[i] = source[i];
}
#endif


/**************************************************************************

  Monitor

  When you put in your application make sure the Monitor_Handler() will be
  called periodically.

  Please note that also the monitor itself calls the Monitor_Handler().
  The monitor will fail if you try to debug this function, especially if
  you place software breakpoints there or if you single-step into this
  function or if you try to restart from an on-chip breakpoint in this function.

	TSMON, UDMON:
	The call is typically included in a periodic interrupt or in the idle
	task of the kernel.
	
	PTMON: 
	The call is typically included in the interrupt service routine which 
	will be triggered by the trigger output signal coming from the debugger.

**************************************************************************/

#ifndef MODE_SIM
static unsigned int Monitor_AddressHigh, Monitor_AddressLow, Monitor_Buffer[16];
static          int Monitor_Index, Monitor_Count;
#endif

#ifdef PTMON3
#define MONITOR_STACKSIZE 0x40
unsigned int Monitor_StackSize = MONITOR_STACKSIZE;
unsigned int Monitor_RegistersAndStack[MONITOR_STACKSIZE+40];
unsigned int * Monitor_StackBase = Monitor_RegistersAndStack+MONITOR_STACKSIZE;
#endif

__attribute__ ((section (".text.fastcode")))
void _udmon3_handler(void)
{
  unsigned int data;
#ifndef MODE_SIM
  int index, len;
  unsigned int address;
#endif

  // receive data from debugger via DCC channel
  if (DCC_ReceiveStatus())
  {
    data = DCC_ReceiveWord();

    switch (data >> 28)
    {
      case 0x00:
      switch (data >> 24)
      {
        case 0x00: // reserved for terminal input in DCC3 protocol mode
			fifo_push(&fifo_recv,(unsigned char)((data>> 0)&0xff));
			break;
        case 0x01:
			fifo_push(&fifo_recv,(unsigned char)((data>> 0)&0xff));
			fifo_push(&fifo_recv,(unsigned char)((data>> 8)&0xff));
			break;
        case 0x02:
			fifo_push(&fifo_recv,(unsigned char)((data>> 0)&0xff));
			fifo_push(&fifo_recv,(unsigned char)((data>> 8)&0xff));
			fifo_push(&fifo_recv,(unsigned char)((data>>16)&0xff));
			break;

        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07: // reserved for FDX transfer to buffer in DCC3 protocol mode
			break;

#ifdef PTMON3
        case 0x08: // get monitor data base
			Monitor_Index = 0;
			Monitor_Count = 4;
			Monitor_Buffer[0] = (unsigned int) Monitor_StackBase;
			break;

        case 0x09: // set monitor data base
			((unsigned char *) Monitor_Buffer)[4 - 1] = (data >> 16) & 0xff;
			Monitor_StackBase = (unsigned int *) Monitor_Buffer[0];
			break;

        case 0x0f: // stop application (on UDMON this is done by a breakpoint range)
			break;
#endif
		}
		break;
		
#ifndef MODE_SIM
      case 0x01: // 8-bit read access
		len = ((data >> 24) & 0x0f) + 1;
		address = (Monitor_AddressLow & ~0xffffff) | (data & 0xffffff);
		Monitor_ReadByte (Monitor_Buffer, (void *) address, len);
		Monitor_Index = 0;
		Monitor_Count = len;
		break;
      case 0x02: // 16-bit read access
		len = ((data >> 24) & 0x0f) + 1;
		address = (Monitor_AddressLow & ~0xffffff) | (data & 0xffffff);
		Monitor_ReadHalf (Monitor_Buffer, (void *) address, len);
		Monitor_Index = 0;
		Monitor_Count = len * 2;
		break;

      case 0x03: // 32-bit read access
		len = ((data >> 24) & 0x0f) + 1;
		address = (Monitor_AddressLow & ~0xffffff) | (data & 0xffffff);
		Monitor_ReadWord (Monitor_Buffer, (void *) address, len);
		Monitor_Index = 0;
		Monitor_Count = len * 4;
		break;

      case 0x04: // 8-bit write access
		len = ((data >> 24) & 0x0f) + 1;
		((unsigned char *) Monitor_Buffer)[len - 1] = (data >> 16) & 0xff;
		address = (Monitor_AddressLow & ~0xffff) | (data & 0xffff);
		Monitor_WriteByte ((void *) address, Monitor_Buffer, len);
		break;

      case 0x05: // 16-bit write access
		len = ((data >> 24) & 0x0f) + 1;
		((unsigned char *) Monitor_Buffer)[len * 2 - 1] = (data >> 16) & 0xff;
		address = (Monitor_AddressLow & ~0xffff) | (data & 0xffff);
		Monitor_WriteHalf ((void *) address, Monitor_Buffer, len);
		break;

      case 0x06: // 32-bit write access
		len = ((data >> 24) & 0x0f) + 1;
		((unsigned char *) Monitor_Buffer)[len * 4 - 1] = (data >> 16) & 0xff;
		address = (Monitor_AddressLow & ~0xffff) | (data & 0xffff);
		Monitor_WriteWord ((void *) address, Monitor_Buffer, len);
		break;
		
      case 0x0d: // set (part of the) address e.g. for a memory write request
		if ((data & 0x01000000) == 0)
		{
			/* Bits 16..39 */
			Monitor_AddressLow = (data << 16);
			Monitor_AddressHigh = (Monitor_AddressHigh & ~0xff) | ((data >> 16) & 0xff);
		}
		else
		{
			/* Bits 40..63 */
			Monitor_AddressHigh = (Monitor_AddressHigh & ~0xffffff00) | ((data << 8) & 0xffffff00);
		}
		break;
		
      case 0x0e: // set (part of the) data to buffer e.g. for a memory write request
      case 0x0f:
		index = ((data >> 24) & 0x1f) * 3;
		if (index < 21)
		{
			((unsigned char *) Monitor_Buffer)[index] = data & 0xff;
			((unsigned char *) Monitor_Buffer)[index + 1] = (data >> 8) & 0xff;
			((unsigned char *) Monitor_Buffer)[index + 2] = (data >> 16) & 0xff;
		}
		break;
#endif
#if 0
      case 0x07: // 32-bit CP15 read access
		Monitor_Buffer[0] = Monitor_ReadCP15 (data & 0xffff);
		Monitor_Index = 0;
		Monitor_Count = 4;
		break;
      case 0x08: // 32-bit CP15 write access
		((unsigned char *) Monitor_Buffer)[4 - 1] = (data >> 16) & 0xff;
		Monitor_WriteCP15 (data & 0xffff, Monitor_Buffer[0]);
		break;
#endif
    }
  }
  
  if(!DCC_SendStatus())
  {
	data=0x0;
	
#ifndef MODE_SIM
	// send data e.g. of a memory read request to TRACE32 GUI via DCC channel
	if (Monitor_Index < Monitor_Count )
	{
		data = (((unsigned char *) Monitor_Buffer)[Monitor_Index])           | 
			   (((unsigned char *) Monitor_Buffer)[Monitor_Index + 1] << 8)  | 
		       (((unsigned char *) Monitor_Buffer)[Monitor_Index + 2] << 16) | 
		        0x10000000;
		DCC_SendWord(data);
		Monitor_Index += 3;
	}
	else 
#endif
#ifdef PTMON3
	// send word to host if monitor has been entered due to a debug event
	if(*Monitor_StackBase & 0x0001)
	{
		*Monitor_StackBase &= ~0x0001;
		data = (*Monitor_StackBase & 0xffff) | 0x0f000000;
		DCC_SendWord(data);
	}
	else 
#endif
	if(fifo_pop(&fifo_send,((unsigned char *)&data)+0) != -1)
	{
		if(fifo_pop(&fifo_send, ((unsigned char *)&data)+1) != -1)
		{
			if(fifo_pop(&fifo_send, ((unsigned char *)&data)+2) != -1)
			{
				DCC_SendWord(data | 0x2000000); //3 Bytes
			}
			else
			{
				DCC_SendWord(data | 0x1000000); //2 Bytes
			}
			
		}
		else
		{
			DCC_SendWord(data | 0x0000000); //1 Bytes
		}
	}
  }
}

#ifdef PTMON3
;=================================================================
; Aus background_monitor/monitor_entry.s
;=================================================================
;  Background Monitor for TRACE32 ARM JTAG Debugger
;  PEG, July 2008
;
;  If the debugger is switched from halt mode debugging to monitor
;  mode debugging (Go.MONitor, Break.SETMONITOR ON), a trap
;  (PABORT, DABORT) will happen instead of halting the program execution
;  when a breakpoint hits or when the BREAK button gets pushed.
;  (The BREAK button causes an on-chip breakpoint range 0-0xffffffff
;  for user mode.)
;
;  When UDMON3 is selected, all onchip breakpoints will be specified
;  for user mode only. It assumes the application which shall be debugged 
;  is running in user mode. TSMON3 and PTMON3 can theoretically also 
;  be used with the background monitor, but then the BREAK button can
;  not be imlemented by a breakpoint range, since even the monitor code 
;  would cause a re-entry into the monitor. Therefore the other two modes 
;  shall not be used for background monitor debugging.
;
;  This example has been tested on an ARM966E-S and ARM926EJ-S Integrator
;  Core Module from ARM. An ARM9..E-S derivative is needed since on other
;  ARM9 derivatives the on-chip breakpoints can not be specified for user mode.
;  Theoretically it will also work on ARM11 and Cortex, but this has not
;  been tested. Then a few changes of the monitor code might be required,
;  but at least the DCC_ functions need to be adapted.
;
;  This trap routine saves the processor registers in a data block at
;  Monitor_StackBase where the TRACE32 software will read it.
;  See the order below.
;
;  The halt reason will be stored at the first data block location.
;  Bit 0 will be set to signalize the Monitor_Handler that the TRACE32
;  software shall be informed that a debug event has happened. The 
;  Monitor_Handler sends the message and clears bit 0. At the beginning
;  this bit needs to be cleared (see monitor.cmm).
;
;  The monitor waits in a loop until TRACE32 causes a re-start of
;  the application. In the same loop memory accesses requested by 
;  TRACE32 will be serviced by calling the Monitor_Handler.
;  TRACE32 can modify the register values by writing to the data block.
;
;  If you enter 'DIAG 3800' in the TRACE32 command line, the communication
;  of the TRACE32 GUI with the Monitor_Handler will be printed in the 
;  'AREA' window.
;
;  Note that the monitor uses the DCC channel. Therefore it can not additionally 
;  be used for terminal or semihosting, except it is done based on a memory interface
;  instead of DCC.
;
;  Monitor_StackBase
;  + 0x00 entry reason, 'debug event just happened' flag (bit 0), re-start signal
;  + 0x04 R0
;  + 0x08 R1
;  + 0x0c R2
;  + 0x10 R3
;  + 0x14 R4
;  + 0x18 R5
;  + 0x1c R6
;  + 0x20 R7
;  + 0x24 R8_USR
;  + 0x28 R9_USR
;  + 0x2c R10_USR
;  + 0x30 R11_USR
;  + 0x34 R12_USR
;  + 0x38 R15
;  + 0x3c CPSR
;  + 0x40 R13_USR
;  + 0x44 R14_USR
;  + 0x48 R13_SVC
;  + 0x4c R14_SVC
;  + 0x50 SPSR_SVC
;  + 0x54 R8_FIQ
;  + 0x58 R9_FIQ
;  + 0x5c R10_FIQ
;  + 0x60 R11_FIQ
;  + 0x64 R12_FIQ
;  + 0x68 R13_FIQ
;  + 0x6c R14_FIQ
;  + 0x70 SPSR_FIQ
;  + 0x74 R13_IRQ
;  + 0x78 R14_IRQ
;  + 0x7c SPSR_IRQ
;  + 0x80 R13_ABT
;  + 0x84 R14_ABT
;  + 0x88 SPSR_ABT
;  + 0x8c DACR
;
;  R13_UND, R14_UND, SPSR_UND are used by the monitor and are
;  therefore of no importance for the application debugging.
;
;=================================================================

; entrypoint with information header for compile phase informations

  AREA  Monitor, CODE, READONLY

  EXPORT  Monitor_EntryRES
  EXPORT  Monitor_EntryUND
  EXPORT  Monitor_EntrySWI
  EXPORT  Monitor_EntryPABT
  EXPORT  Monitor_EntryDABT
  EXPORT  Monitor_EntryIRQ
  EXPORT  Monitor_EntryFIQ
  EXPORT  Monitor_Entry
  EXPORT  Monitor_Polling

  IMPORT  Monitor_Handler


;=================================================================
;  Sample interrupt vector table
;=================================================================

InitVectors LDR PC, _res_vec    ;0x00 Reset
            LDR PC, _und_vec    ;0x04 UNDEF
            LDR PC, _swi_vec    ;0x08 SWI
            LDR PC, _pabt_vec   ;0x0C PABORT
            LDR PC, _dabt_vec   ;0x10 DABORT
            LDR PC, _rsv_vec    ;0x14 reserved
            LDR PC, _irq_vec    ;0x18 IRQ
            LDR PC, _fiq_vec    ;0x1c FIQ

_res_vec  DCD Monitor_EntryRES    ;0x20
_und_vec  DCD Monitor_EntryUND    ;0x24
_swi_vec  DCD Monitor_EntrySWI    ;0x28
_pabt_vec DCD Monitor_EntryPABT   ;0x2c
_dabt_vec DCD Monitor_EntryDABT   ;0x30
_rsv_vec  DCD Monitor_EntrySWI    ;0x34
_irq_vec  DCD Monitor_EntryIRQ    ;0x38
_fiq_vec  DCD Monitor_EntryFIQ    ;0x3c



;=================================================================
;  Monitor Entry Points
;=================================================================

  IMPORT  Monitor_StackBase
Monitor_RegistersPtr
  DCD Monitor_StackBase


Monitor_EntryRES
  MRS R14, CPSR
  MSR CPSR_c, #0xdb     ; switch to undefined mode

  LDR R13, Monitor_RegistersPtr
  LDR R13, [R13]

  STR   R12, [SP, #0x34]
  MOV R12, #0x41        ; reason for entry is RESET

  STR R14, [SP, #0x3c]  ; CPSR

  MOV R14, #0
  STR R14, [SP, #0x38]  ; PC (=0)
  STR R7,  [SP, #0x20]

  B Monitor_Entry


Monitor_EntryUND
  STR   R12, [SP, #0x34]
  MOV R12, #0x11

  SUB R14, R14, #0x04
  STR R14, [SP, #0x38]  ; R14_und = PC
  MRS R14, SPSR
  STR R14, [SP, #0x3c]  ; SPSR_und = CPSR
  STR R7,  [SP, #0x20]

  B Monitor_Entry


Monitor_EntrySWI
  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode
  STR R12, [SP, #0x34]  ; save r12
  STR R7,  [SP, #0x20]  ; save r7
  MOV R7, SP            ; load SP_und into R7

  MSR CPSR_c, #0xD3     ; switch back to SVC mode

  SUB R14, R14, #0x04   ; calculate and save PC
  STR R14, [R7, #0x38]

  MRS R12, SPSR         ; save spsr
  STR R12, [R7, #0x3c]

  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode

  MOV R12, #0x11
  B Monitor_Entry


Monitor_EntryPABT
  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode
  STR R12, [SP, #0x34]  ; save r12
  STR R7,  [SP, #0x20]  ; save r7
  MOV R7, SP            ; load SP_und into R7

  MSR CPSR_c, #0xD7     ; switch back to ABT mode

  SUB R14, R14, #0x04   ; calculate and save PC
  STR R14, [R7, #0x38]

  MRS R12, SPSR         ; save spsr
  STR R12, [R7, #0x3c]

  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode

  MOV R12, #0x11
  B Monitor_Entry


Monitor_EntryDABT
  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode

  LDR R13,Monitor_RegistersPtr
  LDR R13, [R13]

  LDR R14, [SP]
  CMP R14,#0            ; executed from inside monitor ?
  MOVNE R12, #0xf1
  STRNE R12, [SP]
  BNE Monitor_Polling   ; enter loop directly

  STR R12, [SP, #0x34]  ; save r12
  STR R7,  [SP, #0x20]  ; save r7
  MOV R7, SP            ; load SP_und into R7

  MSR CPSR_c, #0xD7     ; switch back to ABT mode

  SUB R14, R14, #0x04   ; calculate and save PC
  STR R14, [R7, #0x38]

  MRS R12, SPSR         ; save spsr
  STR R12, [R7, #0x3c]

  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode

  MOV R12, #0x11
  B Monitor_Entry


Monitor_EntryIRQ
  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode
  STR R12, [SP, #0x34]  ; save r12
  STR R7,  [SP, #0x20]  ; save r7
  MOV R7, SP            ; load SP_und into R7

  MSR CPSR_c, #0xD2     ; switch back to IRQ mode

  SUB R14, R14, #0x04   ; calculate and save PC
  STR R14, [R7, #0x38]

  MRS R12, SPSR         ; save spsr
  STR R12, [R7, #0x3c]

  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode

  MOV R12, #0x11
  B Monitor_Entry


Monitor_EntryFIQ
  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode
  STR R12, [SP, #0x34]  ; save r12
  STR R7,  [SP, #0x20]  ; save r7
  MOV R7, SP            ; load SP_und into R7

  MSR CPSR_c, #0xD1     ; switch back to FIQ mode

  SUB R14, R14, #0x04   ; calculate and save PC
  STR R14, [R7, #0x38]

  MRS R12, SPSR         ; save spsr
  STR R12, [R7, #0x3c]

  MSR CPSR_c, #0xDB     ; switch to undefined opcode mode

  MOV R12, #0x11
  B Monitor_Entry


;=================================================================
;  Breakpoint Entry
;=================================================================

Monitor_Entry
  STR R11, [SP, #0x30]
  STR R10, [SP, #0x2c]
  STR R9,  [SP, #0x28]
  STR R8,  [SP, #0x24]

  STR R6,  [SP, #0x1c]
  STR R5,  [SP, #0x18]
  STR R4,  [SP, #0x14]
  STR R3,  [SP, #0x10]
  STR R2,  [SP, #0x0c]
  STR R1,  [SP, #0x08]
  STR R0,  [SP, #0x04]


;==============================================================
  MRS R0, CPSR          ; switch to sys mode
  ORR R1, R0, #0x0F
  MSR CPSR_c, R1

  MOV R11, R13
  MOV R4, R14

  MSR CPSR_c, R0        ; switch back

  STR R11, [SP, #0x40]
  STR R4, [SP, #0x44]

;==============================================================
  MRS R0, CPSR          ; switch to svc mode
  ORR R1, R0, #0x03
  AND R1, R1, #0xF3
  MSR CPSR_c, R1

  MOV R11, R13
  MOV R4, R14
  MRS R5, SPSR

  MSR CPSR_c, R0        ; switch back

  STR R11, [SP, #0x48]
  STR R4, [SP, #0x4c]
  STR R5, [SP, #0x50]

;==============================================================
  MRS R0, CPSR          ; switch to fiq mode(1)
  ORR R1, R0, #0x01
  AND R1, R1, #0xf1
  MSR CPSR_c, R1

  MOV R3,  R8
  MOV R4,  R9
  MOV R5,  R10
  MOV R6,  R11

  MSR CPSR_c, R0        ; switch back

  STR R3, [SP, #0x54]
  STR R4, [SP, #0x58]
  STR R5, [SP, #0x5c]
  STR R6, [SP, #0x60]

  MRS R0, CPSR          ; switch to fiq mode(2)
  ORR R1, R0, #0x01
  AND R1, R1, #0xf1
  MSR CPSR_c, R1

  MOV R3,  R12
  MOV R4,  R13
  MOV R5,  R14
  MRS R6,  SPSR

  MSR CPSR_c, R0        ; switch back

  STR R3, [SP, #0x64]
  STR R4, [SP, #0x68]
  STR R5, [SP, #0x6c]
  STR R6, [SP, #0x70]

;==============================================================
  MRS R0, CPSR          ; switch to irq mode
  ORR R1, R0, #0x02
  AND R1, R1, #0xF2
  MSR CPSR_c, R1

  MOV R11, R13
  MOV R4, R14
  MRS R5, SPSR

  MSR CPSR_c, R0        ; switch back

  STR R11, [SP, #0x74]
  STR R4, [SP, #0x78]
  STR R5, [SP, #0x7c]

;==============================================================
  MRS R0, CPSR          ; switch to abt mode
  ORR R1, R0, #0x07
  AND R1, R1, #0xF7
  MSR CPSR_c, R1

  MOV R11, R13
  MOV R4, R14
  MRS R5, SPSR

  MSR CPSR_c, R0        ; switch back

  STR R11, [SP, #0x80]
  STR R4, [SP, #0x84]
  STR R5, [SP, #0x88]

;==============================================================

  IF :DEF: MONITOR_OVERWRITE_PROTCR ; same effect as 'SYStem.Option DACR ON' in halt mode debugging on ARM926EJ-S
  MRC p15, 0, r4, c3, c0
  STR R4, [SP, #0x8c]
  MOV R4, -1
  MCR p15, 0, r4, c3, c0
  ENDIF

;==============================================================

  STR R12, [SP]         ; save monitor entry reason code

Monitor_Polling
  BL  Monitor_Handler   ; wait for go and service memory read/write requests in the meantime
  LDR R12, [SP]
  CMP R12,#0
  BNE Monitor_Polling

;==============================================================

  IF :DEF: MONITOR_OVERWRITE_PROTCR
  LDR R4, [SP, #0x8c]
  MCR p15, 0, r4, c3, c0
  ENDIF


;==============================================================
  LDR R11, [SP, #0x40]
  LDR R4, [SP, #0x44]

  MRS R0, CPSR          ; switch to sys mode
  ORR R1, R0, #0x0F
  MSR CPSR_c, R1

  MOV R13, R11
  MOV R14, R4

  MSR CPSR_c, R0        ; switch back


;==============================================================
  LDR R11, [SP, #0x48]
  LDR R4, [SP, #0x4c]
  LDR R5, [SP, #0x50]

  MRS R0, CPSR          ; switch to svc mode
  ORR R1, R0, #0x03
  AND R1, R1, #0xF3
  MSR CPSR_c, R1

  MOV R13, R11
  MOV R14, R4
  MSR SPSR_cxsf, R5

  MSR CPSR_c, R0        ; switch back

;==============================================================
  LDR R3, [SP, #0x54]
  LDR R4, [SP, #0x58]
  LDR R5, [SP, #0x5c]
  LDR R6, [SP, #0x60]

  MRS R0, CPSR          ; switch to fiq mode(1)
  ORR R1, R0, #0x01
  AND R1, R1, #0xf1
  MSR CPSR_c, R1

  MOV R8, R3
  MOV R9, R4
  MOV R10, R5
  MOV R11, R6

  MSR CPSR_c, R0        ; switch back

  LDR R3, [SP, #0x64]
  LDR R4, [SP, #0x68]
  LDR R5, [SP, #0x6c]
  LDR R6, [SP, #0x70]

  MRS R0, CPSR          ; switch to fiq mode(2)
  ORR R1, R0, #0x01
  AND R1, R1, #0xf1
  MSR CPSR_c, R1

  MOV R12, R3
  MOV R13, R4
  MOV R14, R5
  MSR SPSR_cxsf, R6

  MSR CPSR_c, R0        ; switch back

;==============================================================
  LDR R11, [SP, #0x74]
  LDR R4, [SP, #0x78]
  LDR R5, [SP, #0x7c]

  MRS R0, CPSR          ; switch to irq mode
  ORR R1, R0, #0x02
  AND R1, R1, #0xF2
  MSR CPSR_c, R1

  MOV R13, R11
  MOV R14, R4
  MSR SPSR_cxsf, R5

  MSR CPSR_c, R0        ; switch back

;==============================================================
  LDR R11, [SP, #0x80]
  LDR R4, [SP, #0x84]
  LDR R5, [SP, #0x88]

  MRS R0, CPSR          ; switch to abt mode
  ORR R1, R0, #0x07
  AND R1, R1, #0xF7
  MSR CPSR_c, R1

  MOV R13, R11
  MOV R14, R4
  MSR SPSR_cxsf, R5

  MSR CPSR_c, R0        ; switch back

  LDR R0, [SP, #0x3c]   ; SPSR_und (=CPSR)
  MSR SPSR_cxsf, R0

  LDR R14, [SP, #0x38]  ; R14_und (=PC)
  LDR R0,  [SP, #0x04]  ; R0
  LDR R1,  [SP, #0x08]  ; R1
  LDR R2,  [SP, #0x0c]  ; R2
  LDR R3,  [SP, #0x10]  ; R3
  LDR R4,  [SP, #0x14]  ; R4
  LDR R5,  [SP, #0x18]  ; R5
  LDR R6,  [SP, #0x1c]  ; R6
  LDR R7,  [SP, #0x20]  ; R7
  LDR R8,  [SP, #0x24]  ; R8
  LDR R9,  [SP, #0x28]  ; R9
  LDR R10, [SP, #0x2c]  ; R10
  LDR R11, [SP, #0x30]  ; R11
  LDR R12, [SP, #0x34]  ; R12

  MOVS  PC, R14         ; return to application

;==============================================================

  END
#endif
