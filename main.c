#include <stdio.h>
#include <unistd.h>      //fuer _exit()
#include <stdint.h>
#include <string.h>

#include "main.h"
#include "AT91SAM7S64.h"
#include "lib/nxt_avr.h"
#include "lib/aic.h"
//#include "lib/adc.h"
#include "lib/nxt_motor.h"
#include "lib/pio_pwm.h"
#include "lib/display.h"
#include "lib/systick.h"
#include <math.h>
#include "term.h"

#define ZYKLUS_MS 4
#define IDLE_MS   2

#if IDLE_MS >= ZYKLUS_MS
#error "Idle_ms muss kleiner als zyklus_ms sein"
#endif

//Hinweis: Period bewusst von 1,25us auf 2us gesetzt, so dass mehr 'Spielraum' ist
//         Der WS2812 unterscheiden zwischen einer 1 und einer 0 nur über die Pulslänge
//         Die Pausenlänge kann känger sein (max Reset-Zeit), welches hier über das Hochsetzen
//         des PERIOD-Registers erfolgt. 
#define WS2812_PERIOD (0.00000200 * MCK / 1) + 1 //0.00000125
#define WS2812_0H     (0.00000040 * MCK / 1)  //TON  für 0
#define WS2812_0L     (0.00000085 * MCK / 1)  //TOFF für 0
#define WS2812_1H     (0.00000080 * MCK / 1)  //TON  für 1
#define WS2812_1L     (0.00000045 * MCK / 1)  //TOFF für 1
#define WS2812_RESET  (0.000061   * MCK / 1)  //TOFF für Reset (50µs vs 300µs)

#define RESET_NS	50000
#define RING_LEDS 8
#define DATA_SIZE 24

//Datenstruktur
//           +---------+---------+---------+
//           | G7...G0 | R7...R0 | B7...B0 |
//           +---------+---------+---------+
//Union
// Integer
// +---------+---------+---------+---------+
// |31     24 23     16 15      8 7       0|
// +---------+---------+---------+---------+
// Struct
// +---------+
// | Dummy   |
// +---------+
//           +---------+
//           | Green   |
//           +---------+
//                           ..


typedef union {
			struct {uint8_t dummy; 
					uint8_t blue; 
                    uint8_t red;
         			uint8_t green; };
            uint32_t rgb; 
		} WS2812_COMPOSITION_T;
		
typedef WS2812_COMPOSITION_T WS2812_RING_T[RING_LEDS];

WS2812_RING_T ledring;


//Farbdefinition
#define COLOR_BLACK       (WS2812_COMPOSITION_T){.red=0x00, .green=0x00, .blue=0x00}
#define COLOR_WHITE10     (WS2812_COMPOSITION_T){.red=0x08, .green=0x08, .blue=0x08}
#define COLOR_WHITE25     (WS2812_COMPOSITION_T){.red=0x10, .green=0x10, .blue=0x10}
#define COLOR_WHITE50     (WS2812_COMPOSITION_T){.red=0x20, .green=0x20, .blue=0x20}
#define COLOR_WHITE75     (WS2812_COMPOSITION_T){.red=0x40, .green=0x40, .blue=0x40}
#define COLOR_WHITE       (WS2812_COMPOSITION_T){.red=0xff, .green=0xff, .blue=0xff}

#define COLOR_RED         (WS2812_COMPOSITION_T){.red=0xff, .green=0x00, .blue=0x00}
#define COLOR_ROSE        (WS2812_COMPOSITION_T){.red=0xff, .green=0x00, .blue=0x7f}
#define COLOR_MAGENTA     (WS2812_COMPOSITION_T){.red=0xff, .green=0x00, .blue=0xff}
#define COLOR_VIOLET      (WS2812_COMPOSITION_T){.red=0x7f, .green=0x00, .blue=0xff}
#define COLOR_BLUE        (WS2812_COMPOSITION_T){.red=0x00, .green=0x00, .blue=0xff}
#define COLOR_AZURE       (WS2812_COMPOSITION_T){.red=0x00, .green=0x7f, .blue=0xff}
#define COLOR_CYAN        (WS2812_COMPOSITION_T){.red=0x00, .green=0xff, .blue=0xff}
#define COLOR_AUQUAMARINE (WS2812_COMPOSITION_T){.red=0x00, .green=0xff, .blue=0x7f}
#define COLOR_GREEN       (WS2812_COMPOSITION_T){.red=0x00, .green=0xff, .blue=0x00}
#define COLOR_CHARTREUSE  (WS2812_COMPOSITION_T){.red=0x7f, .green=0xff, .blue=0x00}
#define COLOR_YELLOW      (WS2812_COMPOSITION_T){.red=0xff, .green=0xff, .blue=0x00}
#define COLOR_ORANGE      (WS2812_COMPOSITION_T){.red=0xff, .green=0x7f, .blue=0x00}


const WS2812_COMPOSITION_T ws2812_regenbogen[]= { 
	COLOR_RED   ,COLOR_ROSE       , COLOR_MAGENTA, COLOR_VIOLET     ,
    COLOR_BLUE  ,COLOR_AZURE      , COLOR_CYAN   , COLOR_AUQUAMARINE,
	COLOR_GREEN ,COLOR_CHARTREUSE , COLOR_YELLOW , COLOR_ORANGE };


const WS2812_COMPOSITION_T ws2812_rgb[]= { 
	COLOR_RED   ,COLOR_BLUE       , COLOR_GREEN
	};

#define NXT_PORT4_ENABLE (1<<7)
#define PMC_PD10 (1<<10)
#define PWMC_START_ADDRESS 0xFFFCC000
#define PMC_START_ADDRESS  0xFFFFFC00

volatile AT91S_PWMC* pwmc = (AT91S_PWMC*)PWMC_START_ADDRESS;
volatile AT91S_PIO* pio = (AT91S_PIO*)AT91C_PIOA_PER;
volatile AT91S_PMC* pmc = (AT91S_PMC*)PMC_START_ADDRESS;


void initPMC_MCK(void) {
	pmc->PMC_PCER = PMC_PD10;
}

void ws2812_init(void)
{
	int pinmask = AT91C_PIO_PA23;
	pmc->PMC_PCER = PMC_PD10;

	pio->PIO_PDR = pinmask;
	pio->PIO_BSR = pinmask;


	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CMR = (AT91C_PWMC_CPOL);

	//channel period
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CPRDR = WS2812_PERIOD;

	//channel dc
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CDTYR = 2;
	pwmc->PWMC_IER = (1<<0);
	pwmc->PWMC_ENA = (1<<0);
}

volatile uint8_t regenbogen_cnt = 0;

//Volle Compileroptimierung einschalten
#pragma GCC push_options
#pragma GCC optimize ("-O3")
//Code aus dem RAM laufen lassen, welchen einen schnellere Zugriff
//als das Flash hat, so dass die Funktion mit max. Geschwindigekit
//ausgeführt wird
__attribute__ ((section (".text.fastcode")))
void ws2812_send(void)
{

//	     |     |      +--+   +---+  +--+   +--+   +--+   +--+   |      |
//	     |     |      |G7|   |G6 |  |..|   |..|   |B1|   |B0|   Reset+Übrnahme
//	   --+-----+------+  +---+   +--+  +---+  +---+  +---+  +---+------+
//	ISR-Flag   |      |      |      |      |      |      |      |      |
//  UPDR 2     G7     G6     ..     ..     B1     B0     2      2      2
	

//Interrupt ausschalten
	int i_state = interrupts_get_and_disable();

//Zum Einsynchronisieren zunächst ISR-Status zurücksetzen
//und dann einmal auf einen Impuls warten!

	regenbogen_cnt = (regenbogen_cnt + 1) % 12;

	for (int led = RING_LEDS; led; led--) {
		uint32_t output = ledring[led - 1].rgb;
		for (int i = 0; i < DATA_SIZE; i++) {
			while (!(AT91C_BASE_PWMC->PWMC_ISR & (1 << 0)));
			AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CUPDR = output & 0x80000000 ? WS2812_1H : WS2812_0H;
			output <<= 1;		
		}
	}
	while (!(AT91C_BASE_PWMC->PWMC_ISR & (1 << 0)));
	AT91C_BASE_PWMC->PWMC_CH[0].PWMC_CUPDR = 2;
	systick_wait_ns(RESET_NS);
	
	if (i_state)
		interrupts_enable();
}
//Vorherige CompilerOptions wieder herstellen
#pragma GCC pop_options

void ledring_init(void)
{
	//Poti-Initialisieren
	//Da durch AVR Prozessor erfolgt, nichts zu tun
	
	//DIGI-A1->PA18 (äußerer Pin) als Eingabe initialisieren (für Taste)
	//PullUp Disable
	pio->PIO_PER = AT91C_PIO_PA18; 
	pio->PIO_ODR = AT91C_PIO_PA18;         
	pio->PIO_PPUDR = AT91C_PIO_PA18;
	
	ws2812_init();
}

typedef enum {RING_LL_SINGLE      =0,
		      RING_LL_NACHLEUCHTEN=10, 
			  RING_LL_REGENBOGEN  =20,
			  } RING_MODE;
			  
struct {
	int speed_pos;
	int speed_max; 
	RING_MODE ring_mode;
} led={
	.speed_pos=0,
	.ring_mode=RING_LL_SINGLE,
};

#define POTI_MAX 1195


uint8_t curr_pixel = 0;
uint8_t color_picked = 0;
volatile uint32_t last_time = 0;


int ledring_update(void)
{	
	uint32_t time_delta = systick_get_ms() - last_time;


		//Farbe in Abhängigkeit des Modes und der Geschwindigkeit setzen
		//speed=1023 -> 10 Sekunden für eine Runde ->10s/8LED=1,25s
	led.speed_max=(sensor_adc(0)*10000)/8/1024;

	// value of poti translated to range of 0-100
	uint16_t poti_value_percent =  ((double)led.speed_max) / 1200.0 * 100.0;

	//avoid 0
	poti_value_percent = poti_value_percent == 0 ? 1 : poti_value_percent;

	//if update is not ready, return
	if(!(time_delta >= (12.5 * poti_value_percent))) return;
	last_time = systick_get_ms();
	

	switch(led.ring_mode)
	{
		case RING_LL_SINGLE:
			{	
				ledring[(led.speed_pos - 1) & 7] = COLOR_BLACK;
				ledring[led.speed_pos] = COLOR_RED;
			}	
		break;
	
		case RING_LL_NACHLEUCHTEN:
			{
				//distribute different brightness level depending on distance to each other
				for(int i = 0; i < RING_LEDS; i++) {
	
					int a = 255;
					int p = 16;

					double triangle_wave = fabs(((2.0 * a) / M_PI) * asin(sin((2 * M_PI) / p * ((i + led.speed_pos)&7))));
					uint8_t brightness = (int)(triangle_wave);
					ledring[i].blue = 0;					
					ledring[i].red = 0;					
					ledring[i].green = brightness;					
				}
			}
		break;

		case RING_LL_REGENBOGEN:
		{
			for (int i = 0; i < RING_LEDS; i++) 
				ledring[i] = ws2812_regenbogen[color_picked];
			break;	
		}
	
		default:
			break;
	}

	//next position
	led.speed_pos = (led.speed_pos + 1)&(RING_LEDS-1);
	color_picked = (color_picked+1)%12;

	ws2812_send();


	return 0;
}


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
/*****************************************************************************/


void task_8ms(void) 
{

	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS
}


void task_16ms(void) 
{
		

	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS
}


enum states {high, low};
uint8_t state = 0;

void task_32ms(void) 
{
	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS
	ledring_update();

	//non blocking debouncing
	uint8_t button_pressed = (pio->PIO_PDSR >> 18) & 1;

	if(button_pressed && state == low) {
		state = high;
	}

	if(state == high) {
		button_pressed = (pio->PIO_PDSR >> 18) & 1;
		if(button_pressed) return;

		for(int i = 0; i < RING_LEDS; i++) {
			//reset colors when mode changed
			ledring[i] = COLOR_BLACK;
		}

		led.ring_mode = (led.ring_mode + 10)%30;
		state = low;
	}
}


void task_64ms(void) 
{

	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS
}
 
void task_128ms(void) 
{
	// volatile uint8_t button_pressed = (pwmc->PWMC_ISR >> 0) & 1;
	// volatile uint8_t button_pressed2 = (pwmc->PWMC_ISR >> 1) & 1;
	// volatile uint8_t button_pressed3 = (pwmc->PWMC_ISR >> 2) & 1;
	// volatile uint8_t button_pressed4 = (pwmc->PWMC_ISR >> 3) & 1;
	// TERM_INT(button_pressed, 5);
	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS
}

void task_256ms(void) 
{
	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS
}

void task_512ms(void) 
{
	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS
}

void task_1024ms(void) 
{
	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: ZYKLUS_MS

	//set_pixel();

	char dst[10];
	strcpy(dst,"xyz");
	
	//Beispielanwendung für Display
	static uint32_t count=0;
	display_goto_xy(0,1);
	display_unsigned(++count,4);
	display_update();
}

void task_idle(void) 
{
	//Keine blockierende Aufrufe
	//Max. Bearbeitungsdauer: IDLE_MS
	
}

void initCMOSPPoutput(int pinmask) {
	pio->PIO_PER = pinmask;
	pio->PIO_OER = pinmask;
	pio->PIO_MDDR = pinmask;
	pio->PIO_SODR = pinmask;
}

void disableRS485(void) {
	initCMOSPPoutput(NXT_PORT4_ENABLE);
	pio->PIO_CODR |= NXT_PORT4_ENABLE;
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
	
	disableRS485();

	TERM_INIT();
	display_init();
	ledring_init();
	display_clear(0);

	display_string(APP_NAME " : " __TIME__);
	display_update();

	//ANSI Escape sequences - VT100 / VT52
	//http://ascii-table.com/ansi-escape-sequences-vt-100.php
	TERM_STRING("\033[2J");  //Clear Screen
	TERM_STRING("\033[H");   //Move cursor to upper left corner
	TERM_STRING("Prog: " APP_NAME "\n\rVersion von:"__TIME__"\n\r");

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
	
	//set_pixel();

	while(1) {
		//Warten bis zum nächsten TimeSlot
		while((int)(start_tick-systick_get_ms())>0);
		start_tick+=ZYKLUS_MS;
		
		//ws2812_send();



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
		if((int)(start_tick-systick_get_ms()) >= IDLE_MS) {
			task_aktiv="Idle";
			task_idle();
		} 
		// //Max. Zeitdauer einer Zeitscheibe überschritten?
		// if((int)(start_tick-systick_get_ms()) <= 0) {
		// 	TERM_STRING("Timing durch '");
		// 	TERM_STRING(task_aktiv);
		// 	TERM_STRING("' verletzt\n\r");
		// }
		
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
