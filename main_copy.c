#include <stdio.h>
#include <unistd.h>      //fuer _exit()
#include <stdint.h>
#include <string.h>
#include <math.h>


#include "main.h"
#include "AT91SAM7S64.h"
#include "lib/nxt_avr.h"
#include "lib/aic.h"
#include "lib/nxt_motor.h"
#include "lib/pio_pwm.h"
#include "lib/display.h"
#include "lib/systick.h"
#include "term.h"
//#include "lib/adc.h"

#define ZYKLUS_MS 4
#define IDLE_MS   2


#if IDLE_MS >= ZYKLUS_MS
#error "Idle_ms muss kleiner als zyklus_ms sein"
#endif


/*****************************************************************************/
/*   Hilfsroutinen                                                           */
/*   Standard-C-Library (weitere befinden sich in newlib_syscalls.c)         */
/*****************************************************************************/
//Routine wird von C-Lib aufgerufen (bspw. printf() abort())

void _exit(int status)
{
	(void) status;
	//LED-Blinken lassen
	//Breakpoint setzen
	while(1);
}

/*****************************************************************************/
/*   Hilfsroutinen                                                           */
/*   Mutex-Funktion zur Absicherung von kritichen Bereichen                  */
/*   und zur Darstellung von Inline-Assembler                                */
/*   Nutzung dieser Funktion sollte gut durchdacht werden,                   */
/*     andernfalls Gefahr von Deadlocks                                      */
/*****************************************************************************/
void EnterCriticalSection(uint32_t *mutex)
{
//Quellen:
//https://www.doulos.com/knowhow/arm-embedded/implementing-semaphores-on-arm-processors/
//https://www.ic.unicamp.br/~celio/mc404-s2-2015/docs/ARM-GCC-Inline-Assembler-Cookbook.pdf
	asm(".equ LOCKED, 1\n\t"          
		"LDR   %%r0, =LOCKED\n\t"     /* preload "locked" value */

		"spin_lock0:\n\t"
		"SWP   %%r0, %%r0, [%0]\n\t"  /* swap register value with semaphore*/
		"CMP   %%r0, #LOCKED\n\t"     /* if semaphore was locked already*/
		"BEQ   spin_lock0\n\t"        /* retry (aktives Warten) */
		:                             /* output */
		:"r"(mutex)                   /* input  */
		:"cc","%r0","memory"          /* clobbered register */
		);
}
void LeaveCriticalSection(uint32_t *mutex)
{
	*mutex=0;
}

/*****************************************************************************/
/*   Hilfsroutinen                                                           */
/*   Zur Darstellung eines analogen Verlaufes entsprechend einem Oszillosop  */
/*   Darstellung des Puffers über 'v.draw %e trace_buf0 trace_buf1'          */
/*****************************************************************************/
#define TRACE_SIZE 200
uint16_t  trace_buf0[TRACE_SIZE];
uint16_t  trace_buf1[TRACE_SIZE];
void trace_scope(int channel,uint8_t value)
{
	if(channel==0) {
		for(int lauf=0;lauf<(TRACE_SIZE-1);lauf++)
			trace_buf0[lauf]=trace_buf0[lauf+1];
		trace_buf0[TRACE_SIZE-1]=value;
	}
	else {
		for(int lauf=0;lauf<(TRACE_SIZE-1);lauf++)
			trace_buf1[lauf]=trace_buf1[lauf+1];
		trace_buf1[TRACE_SIZE-1]=value;
	}
}

/*****************************************************************************/
/*   Hilfsroutinen                                                           */
/*****************************************************************************/


#define SPULE1_PWM (1<<23)
#define SPULE2_PWM (1<<2)
#define SPULE1_DIR (1<<18)
#define SPULE2_DIR (1<<30)
#define NXT_PORT4_ENABLE (1<<7)
#define PMC_PD10 (1<<10)
#define PWMC_START_ADDRESS 0xFFFCC000
#define PMC_START_ADDRESS  0xFFFFFC00

volatile AT91S_PWMC* pwmc			= (AT91S_PWMC*)PWMC_START_ADDRESS;
volatile AT91S_PIO*  pio			= (AT91S_PIO*)AT91C_PIOA_PER;
volatile AT91S_PMC*  pmc			= (AT91S_PMC*)PMC_START_ADDRESS;
volatile uint16_t stp_cnt = 1;
const	 uint16_t freq = (MCK / 2000);


typedef enum {
	PeripheralA,
	PeripheralB
} PeripheralSelect;


enum direction {
	clockwise = 1,
	counterclockwise = -1
};



void initPeripheral(int pinmask, PeripheralSelect select) {
	pio->PIO_PDR = pinmask;

	if (select == PeripheralA) {
		pio->PIO_ASR = pinmask;
	}
	else {
		pio->PIO_BSR = pinmask;
	}
}

void initCMOSPPoutput(int pinmask) {
	pio->PIO_PER = pinmask;
	pio->PIO_OER = pinmask;
	pio->PIO_MDDR = pinmask;
	pio->PIO_SODR = pinmask;
}


void initPMC_MCK(void) {
	pmc->PMC_PCER = PMC_PD10;
}
void disableRS485(void) {
	initCMOSPPoutput(NXT_PORT4_ENABLE);
	pio->PIO_CODR |= NXT_PORT4_ENABLE;
}


void initPWM(void) {

	//channel alignment, center alignment
	//channel polarity, 
	//1 = will modify the period at update
	//0 = will modify the dutycycle at update
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CMR = (AT91C_PWMC_CALG + AT91C_PWMC_CPOL - AT91C_PWMC_CPD);
	AT91C_BASE_PWMC->PWMC_CH[2].PWMC_CMR = (AT91C_PWMC_CALG - AT91C_PWMC_CPOL - AT91C_PWMC_CPD);

	//channel period
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CPRDR = (uint16_t)freq;
	AT91C_BASE_PWMC->PWMC_CH[2].PWMC_CPRDR = (uint16_t)freq;

	//channel dc
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CDTYR = (uint16_t)(freq / 2);
	AT91C_BASE_PWMC->PWMC_CH[2].PWMC_CDTYR = (uint16_t)(freq / 2);



	//pwm enable + sync
	pwmc->PWMC_ENA = (1 << 0) + (1 << 2);
}


void updateDutyCyclePWM(uint16_t dc1, uint16_t dc2) {
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CUPDR = (uint16_t)dc1;
	AT91C_BASE_PWMC->PWMC_CH[2].PWMC_CUPDR = (uint16_t)dc2;
}

void initCMOSPPoutputDIR(void) {
	initCMOSPPoutput(SPULE1_DIR);
	initCMOSPPoutput(SPULE2_DIR);
}


// PA2   -> PWM-Spule2      (PWM2) A
// PA23	 -> PWM-Spule1		(PWM0) B
void initPwmInput(void) {
	initPeripheral(SPULE1_PWM, PeripheralB);
	initPeripheral(SPULE2_PWM, PeripheralA);
}

void set_output_GPIO(int pin_mask) {
	pio->PIO_SODR = pin_mask;
}

void clear_output_GPIO(int pin_mask) {
	pio->PIO_CODR = pin_mask;
}


uint16_t getDutyCycle(uint8_t stp_cnt, uint8_t sinus) {
	uint16_t newDuty = 10;

	//else sparen
	double wave = cos(2 * M_PI * stp_cnt / 32);

	if (sinus) {
		wave = sin(2 * M_PI * stp_cnt / 32);
	}

	//immer positive werte
	wave = fabs(wave);
	newDuty = freq * wave;
	
	//verhindern dass wert < 10 oder > periode - 10 annimmt
	if (newDuty < 10) newDuty = 10;
	if (newDuty > (uint16_t)(freq - 10)) newDuty = (freq - 10);

	return newDuty;
}

void make_pwm_step(int8_t step) {
	switch (stp_cnt) {
	case 1:                         						
		set_output_GPIO(SPULE2_DIR);
		break;
	case 9:
		clear_output_GPIO(SPULE1_DIR);
		break;
	case 17:
		clear_output_GPIO(SPULE2_DIR);
		break;
	case 25:
		set_output_GPIO(SPULE1_DIR);
		break;


	default:
		break;
	}
  
	uint16_t duty_sin = getDutyCycle(stp_cnt, 1);
	uint16_t duty_cos = getDutyCycle(stp_cnt, 0);

	updateDutyCyclePWM(duty_sin, duty_cos);

	stp_cnt = (stp_cnt + step) & 31;
}

void task_8ms(void) 
{	
	make_pwm_step(counterclockwise);

}


void task_16ms(void) 
{

}


void task_32ms(void) 
{

}

void task_64ms(void) 
{
	
}
 
void task_128ms(void) 
{


}

void task_256ms(void) 
{
	
}

void task_512ms(void) 
{


	
}


void task_1024ms(void) 
{


}


void task_idle(void) 
{
	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: IDLE_MS
	
	//Beispielanwendung für Terminal-Schnittstelle
	       unsigned char c;
	static unsigned char string[100];
	static uint8_t      strpos=0;
	
	if(TERM_READ(&c)!=-1) {
		TERM_CHAR(c);   //Das empfangene Zeichen als Echo an das Terminal zurückschicken
		string[  strpos]=c;
		string[++strpos]=0;
		strpos=strpos>=(sizeof(string)-1)?(sizeof(string)-2):strpos;
		
		if(c=='\r') {
			string[--strpos]=0;
			TERM_STRING("\n\r==>ENTER gedr\xFC""ckt: '");
			TERM_STRING((char *)string);
			TERM_STRING("'\n\r");
			//oberen 128 ASCII zeichen des Terminal-Fensters sind wie folgt codiert
			//https://en.wikipedia.org/wiki/VT100_encoding
			
			strpos=0;
			string[strpos]=0;
		}
	}
}


/*****************************************************************************/
/*    Main-Funktion                                                          */
/*****************************************************************************/
#if 0
//Variante 1: Deklaration der main() Funktion
//da es keine CLI gibt, über welcher die Anwendung getartet wird
//sondern der start über startup.s erfolgt, macht dies kein Sinn
//und belegt unnötige Speicherplatz auf den Stack
int main(int argc, char *argv[]) 
{
	(void) argc;
	(void) argv;

#else
//Variante 2: Deklaration der main() funktion
int main(void) 
{
	
#endif


	/* 'Pflicht' Initialisierung, können nicht ausgelassen werden */
	aic_init();				//Interrupt-Controller initialisieren
	systick_init();			//System-Timer initialisieren
	interrupts_enable();    //Ohne Worte
	nxt_avr_init();
	
	/* 'Wahl' Initialisierung, hängt von den benötigten Komponenten ab */

	TERM_INIT();
	display_init();

	disableRS485();
	initCMOSPPoutputDIR();
	initPMC_MCK();
	initPwmInput();
	initPWM();


	//ANSI Escape sequences - VT100 / VT52
	//http://ascii-table.com/ansi-escape-sequences-vt-100.php
	TERM_STRING("\033[2J");  //Clear Screen
	TERM_STRING("\033[H");   //Move cursor to upper left corner
	TERM_STRING("Prog: " APP_NAME "\n\rVersion von:"__TIME__"\n\r");
//printf("test%d",7);
	//printf()
	//-> Stdout ist auf Terminal umgeleitet newlib_syscalls/_write_r()
	//-> libc      %f unterstützung, 
	//   Notwendiger Speicher für Text: .text +=42.152 Bytes
	//   Notwendiger Speicher für Heap: 4096 Bytes
	//-> libc_nano %f wird nicht unterstützt (printf() = iprintf())
	//   Notwendiger Speicher für Text: .text +=12.192 Bytes
	//   Notwendiger Speicher für Heap: ???
	//iprintf()   
	//   Notwendiger Speicher für Text: .text +=11.104 Bytes
	//   Notwendiger Speicher für HEAP: 1468 Bytes

	//scanf()
	//-> stdin ist auf Terminal umgeleteitet newlib_syscalls/_read_r()
	//-> libc       %f wird unterstützt
	//   Notwendiger Speicher für Text: .text += 68.168 Bytes
	//   Notwendiger Speicher für Heap: 1048+2084 Bytes
	//-> libc_nano %f wird nicht unterstützt (printf() = iprintf())
	//   Notwendiger Speicher für Text: .text += 12.156 Bytes
	//   Notwendiger Speicher für Heap: 436+1032 Bytes

#ifndef MODE_ROM
	/* Watchdog Disable */
	/* Mode-Register kann nur einmal beschrieben werden */
	AT91C_BASE_WDTC->WDTC_WDMR= 0xFFF | 
							    AT91C_WDTC_WDDIS |    /*WD Disable */
	                            AT91C_WDTC_WDDBGHLT | /*Debug Halt */
								AT91C_WDTC_WDIDLEHLT; /*Idle Halt  */
#else
	#if 0
	/* Watchdog Enable */
	/* Da in dieser Version kein zyklischer Reset des Watchdogs */
	/* vorhanden ist, wird von einem Watchdog Enable abgesehen  */
	/* Mit Reset wird der Wachdog aktiviert!                    */
	#else
	/* Watchdog Disable */
	/* Mode-Register kann nur einmal beschrieben werden */
	AT91C_BASE_WDTC->WDTC_WDMR= 0xFFF | 
							    AT91C_WDTC_WDDIS |    /*WD Disable */
	                            AT91C_WDTC_WDDBGHLT | /*Debug Halt */
								AT91C_WDTC_WDIDLEHLT; /*Idle Halt  */
	#endif
#endif

	//Vorangegangenen Stackaufbau 'löschen'
	stack_fill();
	
	//Label, so das mit 'go start' hierin gesprungen werden kann
start: __attribute__((unused));

	uint32_t start_tick=systick_get_ms();
	uint32_t zeitscheibe=0;
	char    *task_aktiv="";
	while(1) {
		//Warten bis zum nächsten TimeSlot
		while((start_tick-systick_get_ms())>0);
		start_tick+=ZYKLUS_MS;


		//Label, so das mit 'go zyklus' hierin gesprungen werden kann
zyklus:  __attribute__((unused))

		if     ((zeitscheibe & 0b000000001) == 0b000000001) {
			task_aktiv="8ms";
			task_8ms();
		}
		else if((zeitscheibe & 0b000000011) == 0b000000010) {
			task_aktiv="16ms";
			task_16ms();
		}
		else if((zeitscheibe & 0b000000111) == 0b000000100) {
			task_aktiv="32ms";
			task_32ms();
		}
		else if((zeitscheibe & 0b000001111) == 0b000001000) {
			task_aktiv="64ms";
			task_64ms();
		}
		else if((zeitscheibe & 0b000011111) == 0b000010000) {
			task_aktiv="128ms";
			task_128ms();
		}
		else if((zeitscheibe & 0b000111111) == 0b000100000) {
			task_aktiv="256ms";
			task_256ms();
		}
		else if((zeitscheibe & 0b001111111) == 0b001000000) {
			task_aktiv="512ms";
			task_512ms();
		}
		else if((zeitscheibe & 0b011111111) == 0b010000000) {
			task_aktiv="1024ms";
			task_1024ms();
		}
		//Zeit für IDLE-Task verfügbar
		if((start_tick-systick_get_ms()) >= IDLE_MS) {
			task_aktiv="Idle";
			task_idle();
		} 
		//Max. Zeitdauer einer Zeitscheibe überschritten?
		if((start_tick-systick_get_ms()) <= 0) {
			TERM_STRING("Timing durch '");
			TERM_STRING(task_aktiv);
			TERM_STRING("' verletzt\n\r");
		}
		
		//Zeitscheibe erhöhen
		zeitscheibe++;
		
		//Stack Testen
		if(stack_check()<(1*4)) {
			static uint8_t stack_cnt=0;
			if(stack_cnt==0) {
				stack_cnt=1;
				TERM_STRING("Stack overflow durch '");
				TERM_STRING(task_aktiv);
				TERM_STRING("'\n\r");
			}
		}
	}
	
	nxt_avr_power_down();
}
