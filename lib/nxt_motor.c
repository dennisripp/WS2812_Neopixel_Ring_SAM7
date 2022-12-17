#include <stddef.h>
#include "../AT91SAM7S64.h"     //fuer GPIO / Timer 
#include "../main.h"            //fuer MCK
#include "nxt_motor.h"
#include "pio_pwm.h"
#include "nxt_avr.h"            //fuer nxt_avr_set_motor()

/**********************************************************************************
 *                        Linksrum <-  |  -> Rechtsrum
 *  A(Mx0)         ----+     +------+     +------+     +------+     +---
 *                     |     |      |     |      |     |      |     |
 *                     +-----+      +-----+      +-----+      +-----+   
 *  B(Mx1)          ------+     +-------------+     +------+     +------
 *                        |     |             |     |      |     |
 *                        +-----+             +-----+      +-----+
 *                        |  |   |  |
 *                        |  +-- | -+--> Timer LDRA+Overrun-ISR
 *                        +------+-----> GPIO-ISR
 *                                    
 * pinChanges A     00010000010000001000001000000100000100000010000010000
 * pinChanges B     00000010000010000000000000100000100000010000010000000
 * currentPins A    11100000011111110000001111111000000111111100000011111
 * currentPins B    11111100000011111111111111000000111111100000011111111
 * 
 * Position über Steigende/Fallende Flanke von A(Timer LDRA-ISR) und GPIO-ISR
 * Geschwindigkeit über Periodendauer von A (Pausen- und Pulsdauer)
 *    Mittelwertbildung immer nur bis zur über nxt_motor_init() angegebenen
 *    Zyklusdauer
 **********************************************************************************/

#define MODE_PIO_PWM 0
#define MODE_GPIO    1
#define MODE         MODE_PIO_PWM

#define MA0 15     //PerA=TF    PerB=TioA1   <-- Pos=Timer-IRQ  Geschwindigkeit=Timer1
#define MA1 1      //PerA=PWM1  PerB=TioB0   <-- Pos=Gpio-IRQ
#define MB0 26     //PerA=DCD1  PerB=TioA2   <-- Pos=Timer-IRQ  Geschwindigkeit=Timer2
#define MB1 9      //PerA=DRxD  PerB=NPCs1   <-- Pos=Gpio-IRQ
#define MC0 0      //PerA=PWM0  PerB=TioA0   <-- Pos=Timer-IRQ  Geschwindigkeit=Timer0
#define MC1 8      //PerA=CTS0  PerB=ADTrg   <-- Pos=Gpio-IRQ

typedef enum {SPEED_STOP, SPEED_01, SPEED_02, SPEED_03, SPEED_04, 
						  SPEED_05, SPEED_06, SPEED_07, SPEED_08,
						  SPEED_09, SPEED_10, SPEED_11, SPEED_12,
						  SPEED_13, SPEED_14, SPEED_MAX=0x0f 
			  } __attribute__((packed)) SPEED_FLAG;

#define   MOTOR_SPEED_BUF 8

struct {
	struct motor {
		uint32_t   pos;
		uint32_t   speed[MOTOR_SPEED_BUF];
		 int32_t   idx;
		SPEED_FLAG flag;
		 int8_t    dir;
	} motor[3];
	uint64_t timer_zyklus;
} motor;

#if MODE==MODE_PIO_PWM
void pos_isr_ma1(void)   //ISR für steigende/fallende Flanke von Ltg MA1
{
	uint32_t currentPins = *AT91C_PIOA_PDSR;	// Read pins
	 int8_t  dir=((currentPins>>MA0)&1) == ((currentPins>>MA1)&1) ? +1 : -1;
	if(dir!=motor.motor[0].dir)
		motor.motor[0].flag=SPEED_STOP;
	motor.motor[0].pos+=motor.motor[0].dir=dir;
}

void pos_isr_mb1(void)   //ISR für steigende/fallende Flanke von Ltg MB1
{
	uint32_t currentPins = *AT91C_PIOA_PDSR;	// Read pins
	 int8_t  dir=((currentPins>>MB0)&1) == ((currentPins>>MB1)&1) ? +1 : -1;
	if(dir != motor.motor[1].dir)
		motor.motor[1].flag=SPEED_STOP;
	motor.motor[1].pos+=motor.motor[1].dir=dir;
}

void pos_isr_mc1(void)   //ISR für steigende/fallende Flanke von Ltg MC1
{
	uint32_t currentPins = *AT91C_PIOA_PDSR;	// Read pins
	 int8_t  dir=((currentPins>>MC0)&1) == ((currentPins>>MC1)&1) ? +1 : -1;
	if(dir != motor.motor[2].dir)
		motor.motor[2].flag=SPEED_STOP;
	motor.motor[2].pos+=motor.motor[2].dir=dir;
}

#endif
#if MODE==MODE_GPIO

void gpio_isr_entry(void)
{
//  uint32_t i_state = interrupts_get_and_disable();

	uint32_t pinChanges  = *AT91C_PIOA_ISR;		// Acknowledge change
	uint32_t currentPins = *AT91C_PIOA_PDSR;	// Read pins
	 int8_t  dir;
	 
	if(pinChanges & (1<<MA1)) {
		dir=((currentPins>>MA0)&1) == ((currentPins>>MA1)&1) ? +1 : -1;
		if(dir != motor.motor[0].dir)
			motor.motor[0].flag=SPEED_STOP;
		motor.motor[0].pos+=motor.motor[0].dir=dir;
	}
		
	if(pinChanges & (1<<MB1)) {
		dir=((currentPins>>MB0)&1) == ((currentPins>>MB1)&1) ? +1 : -1;
		if(dir != motor.motor[1].dir)
			motor.motor[1].flag=SPEED_STOP;
		motor.motor[1].pos+=motor.motor[1].dir=dir;
	}

	if(pinChanges & (1<<MC1)) {
		dir=((currentPins>>MC0)&1) == ((currentPins>>MC1)&1) ? +1 : -1;
		if(dir != motor.motor[2].dir)
			motor.motor[2].flag=SPEED_STOP;
		motor.motor[2].pos+=motor.motor[2].dir=dir;
	}

//  if (i_state)
//    interrupts_enable();
}

#endif
void timer0_isr_entry(void)   //MC0 (Motor 2) -> Timer0
{
	struct motor *mot=&motor.motor[2];
	uint32_t tc_sr = AT91C_BASE_TCB->TCB_TC0.TC_SR;
	if(tc_sr & AT91C_TC_LDRAS) {   //RA-Load
		//Position Update
		uint32_t currentPins = *AT91C_PIOA_PDSR;	         	// Read pins
		 int8_t  dir=((currentPins>>MC0)&1) == ((currentPins>>MC1)&1) ? -1 : +1;
		if(dir != mot->dir)
			mot->flag=SPEED_STOP;
		mot->pos+=mot->dir=dir;
		//Geschwindigkeit Update
		mot->idx=(mot->idx+1) & (MOTOR_SPEED_BUF-1);
		mot->speed[mot->idx]=AT91C_BASE_TCB->TCB_TC0.TC_RA;
		mot->flag=mot->flag < SPEED_MAX ? mot->flag+1 : SPEED_MAX;
		//IDX    0    1     2     3     4     5     6     7     0     1     2
		//Value --   InV  Valid Valid Valid Valid Valid Valid Valid Valid Valid
		//Flag  STO   01    02    03    04    05    06    07    08    09 .. 15
		//                  H     L     H     L     H     L     H     L     H
	}
	
	if(tc_sr & AT91C_TC_CPCS) {  //Compare -> Motor-Stop
		mot->flag=SPEED_STOP;
	}	
//	if(tc_sr & AT91C_TC_LOVRS)  {   //Load Overrun
//		mot->flag=SPEED_ERROR;
//	}
}

void timer1_isr_entry(void)   //MA0 (Motor 0) -> timer 1
{
	struct motor *mot=&motor.motor[0];
	uint32_t tc_sr = AT91C_BASE_TCB->TCB_TC1.TC_SR;
	if(tc_sr & AT91C_TC_LDRAS) {   //RA-Load
		//Position Update
		uint32_t currentPins = *AT91C_PIOA_PDSR;	            // Read pins
		 int8_t  dir=((currentPins>>MA0)&1) == ((currentPins>>MA1)&1) ? -1 : +1;
		if(dir != mot->dir)
			mot->flag=SPEED_STOP;
		mot->pos+=mot->dir=dir;
		//Geschwindigkeit Update
		mot->idx=(mot->idx+1) & (MOTOR_SPEED_BUF-1);
		mot->speed[mot->idx]=AT91C_BASE_TCB->TCB_TC1.TC_RA;
		mot->flag=mot->flag < SPEED_MAX ? mot->flag+1 : SPEED_MAX;
	}
	
	if(tc_sr & AT91C_TC_CPCS) {  //Compare -> Motor-Stop
		mot->flag=SPEED_STOP;
	}	
//	if(tc_sr & AT91C_TC_LOVRS)  {   //Load Overrun
//		mot->flag=SPEED_ERROR;
//	}
}

void timer2_isr_entry(void)   //MB0 (Motor 1) -> timer 2
{
	struct motor *mot=&motor.motor[1];
	uint32_t tc_sr = AT91C_BASE_TCB->TCB_TC2.TC_SR;
	if(tc_sr & AT91C_TC_LDRAS) {   //RA-Load
		//Position Update
		uint32_t currentPins = *AT91C_PIOA_PDSR;	            // Read pins
		 int8_t  dir=((currentPins>>MB0)&1) == ((currentPins>>MB1)&1) ? -1 : +1;
		if(dir != mot->dir)
			mot->flag=SPEED_STOP;
		mot->pos+=mot->dir=dir;
		//Geschwindigkeit Update
		mot->idx=(mot->idx+1) & (MOTOR_SPEED_BUF-1);
		mot->speed[mot->idx]=AT91C_BASE_TCB->TCB_TC2.TC_RA;
		mot->flag=mot->flag < SPEED_MAX ? mot->flag+1 : SPEED_MAX;
	}
	
	if(tc_sr & AT91C_TC_CPCS) {  //Compare -> Motor-Stop
		mot->flag=SPEED_STOP;
	}	
//	if(tc_sr & AT91C_TC_LOVRS)  {   //Load Overrun
//		mot->flag=SPEED_ERROR;
//	}
}


int nxt_motor_init(uint32_t zyklus)
{
	//PWM-Signale laufen über den AVR-Prozessor
	//Initialisierung nicht notwendig, da dieser zuvor 
	//bereits ininitialisiert wurde
	//nxt_avr_init()
    nxt_avr_set_motor(0,0,0);
	nxt_avr_set_motor(1,0,0);
	nxt_avr_set_motor(2,0,0);

	//Position Initialisieren
	for(int lauf=0;lauf<3;lauf++)
		motor.motor[lauf]=(struct motor){.pos=0, .speed={}, .flag=SPEED_STOP, .dir=0};
	
	//Zyklus in Taktzyklen umrechnen un dspeichern
	motor.timer_zyklus=((uint64_t)zyklus*MCK/(32*1000));
	
#if MODE==MODE_PIO_PWM
	pio_init(4,0,PIO_CONFIG_CHANNELB | PIO_CONFIG_GLITCH,NULL);         //MA0
	pio_init(4,1,PIO_CONFIG_INPUT    | PIO_CONFIG_GLITCH 
	                                 | PIO_CONFIG_DPU
									 | PIO_CONFIG_IRQ   ,pos_isr_ma1);  //MA1
	pio_init(5,0,PIO_CONFIG_CHANNELB | PIO_CONFIG_GLITCH,NULL);         //MB0
	pio_init(5,1,PIO_CONFIG_INPUT    | PIO_CONFIG_GLITCH
	                                 | PIO_CONFIG_DPU
									 | PIO_CONFIG_IRQ   ,pos_isr_mb1);  //MB1
	pio_init(6,0,PIO_CONFIG_CHANNELB | PIO_CONFIG_GLITCH,NULL);         //MC0
	pio_init(6,1,PIO_CONFIG_INPUT    | PIO_CONFIG_GLITCH
	                                 | PIO_CONFIG_DPU
									 | PIO_CONFIG_IRQ   ,pos_isr_mc1);  //MC1
#endif
#if MODE==MODE_GPIO
	AT91C_BASE_PIOA->PIO_PDR   = (1<<MA0) | (1<<MB0) | (1<<MC0);	//Peripheral Mode
	AT91C_BASE_PIOA->PIO_BSR   = (1<<MA0) | (1<<MB0) | (1<<MC0);	//Peripheral B

	AT91C_BASE_PIOA->PIO_PER   = (1<<MA1) | (1<<MB1) | (1<<MC1);	//GPIO Mode
	AT91C_BASE_PIOA->PIO_ODR   = (1<<MA1) | (1<<MB1) | (1<<MC1);	//configure as input pin */
	
	AT91C_BASE_PIOA->PIO_PPUDR = (1<<MA0) | (1<<MB0) | (1<<MC0) |
	                             (1<<MA1) | (1<<MB1) | (1<<MC1);    //Disable PullUp
	
	AT91C_BASE_PIOA->PIO_IFER  = (1<<MA0) | (1<<MB0) | (1<<MC0) |
	                             (1<<MA1) | (1<<MB1) | (1<<MC1);    //Enable Glitch Filter

	aic_mask_off(AT91C_ID_PIOA);
	aic_set_vector(AT91C_ID_PIOA, AIC_INT_LEVEL_ABOVE_NORMAL,gpio_isr_entry);
							 /*AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL */
	aic_mask_on(AT91C_ID_PIOA);
	
	uint32_t pinChanges  = *AT91C_PIOA_ISR;	// Acknowledge change
	(void) pinChanges;
	AT91C_BASE_PIOA->PIO_IER   = (1<<MA1) | (1<<MB1) | (1<<MC1);	//Interrupt Enable Register
#endif

	uint32_t tc_sr;
	//Timer mit Clock versorgen
	AT91C_BASE_PMC->PMC_PCER = (1<<AT91C_ID_TC0) | (1<<AT91C_ID_TC1) | (1<<AT91C_ID_TC2);
	
	//Timer initialisieren
	AT91C_BASE_TCB->TCB_BCR = 0; //Kein Sync aktivieren
	AT91C_BASE_TCB->TCB_BMR = AT91C_TCB_TC0XC0S_NONE |
	                          AT91C_TCB_TC1XC1S_NONE |
							  AT91C_TCB_TC2XC2S_NONE;
	
	//Motor Impulsrate
	//  nis    ->  1/nis  / 360 = TI   //nis:Drehzahl in U/sec   TI:Impulsbreite in s
	//  nim    -> 60/nim  / 360 = TI   //nim:Drehzahl in U/min   TI:Impulsbreite in s
	//2,0U/sec -> 0,5s/U / 360 =  1,38ms
	//0,2U/sec -> 5,0s/U / 360 = 13,88ms
	//Timer Taktrate
	//   2/48MHz*65536 =    2,73ms  Timer-Clock1
	//   8/48MHzÜ65536 =   10,92ms  Timer-Clock2
	//  32/48MHz*65536 =   43,69ms  Timer-Clock3  <----
	// 128/48MHz*65536 =  174,76ms  Timer-Clock4
	//1024/48MHz*65536 = 1398,10ms  Timer-Clock5

	//Timer0 -> MC0
	AT91C_BASE_TCB->TCB_TC0.TC_CMR = 
			AT91C_TC_CLKS_TIMER_DIV3_CLOCK |  //MCK/32
	        /* AT91C_TC_CLKI | */       //ClockInvert
			AT91C_TC_BURST_NONE |       //Kein Burst mit XC012
			/* AT91C_TC_LDBSTOP | */    //Counter clock stopped with RB Load
			/* AT91C_TC_LDBDIS  | */    //Counter Clock Disabled with RB Loading
			AT91C_TC_ETRGEDG_BOTH |     //External Trigger(Reset) Edge Selection
			AT91C_TC_ABETRG  |          //TIOA or TIOB External Trigger(Reset) Selection
			/* AT91C_TC_CPCTRG  | */    //RC Compare Trigger Enable
			/* AT91C_TC_WAVE    | */    //Enable Waveform Mode
			AT91C_TC_LDRA_BOTH  |       //each edge of TIOA for CaputreA
			AT91C_TC_LDRB_NONE;         //None edge of TIOA for CaptureB

	//Compare-Register für Motor-Stop auf 30ms setzen
	AT91C_BASE_TCB->TCB_TC0.TC_RC = (0.030*MCK/32);
	
	//Timer-ISR initialiseren
	aic_mask_off(AT91C_ID_TC0);
	aic_set_vector(AT91C_ID_TC0, AIC_INT_LEVEL_ABOVE_NORMAL,timer0_isr_entry);
							 /*AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL */
	aic_mask_on(AT91C_ID_TC0);
	tc_sr = AT91C_BASE_TCB->TCB_TC0.TC_SR;                    //ISR-Reset
	(void)tc_sr;
	AT91C_BASE_TCB->TCB_TC0.TC_IER = AT91C_TC_LDRAS | AT91C_TC_CPCS;   //ISR-Enable
	
	AT91C_BASE_TCB->TCB_TC0.TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;  //Timer-Start

	//Timer1 -> MA0
	AT91C_BASE_TCB->TCB_TC1.TC_CMR = 
			AT91C_TC_CLKS_TIMER_DIV3_CLOCK |  //MCK/32
	        /* AT91C_TC_CLKI | */       //ClockInvert
			AT91C_TC_BURST_NONE |       //Kein Burst mit XC012
			/* AT91C_TC_LDBSTOP | */    //Counter clock stopped with RB Load
			/* AT91C_TC_LDBDIS  | */    //Counter Clock Disabled with RB Loading
			AT91C_TC_ETRGEDG_BOTH |     //External Trigger(Reset) Edge Selection
			AT91C_TC_ABETRG  |          //TIOA or TIOB External Trigger(Reset) Selection
			/* AT91C_TC_CPCTRG  | */    //RC Compare Trigger Enable
			/* AT91C_TC_WAVE    | */    //Enable Waveform Mode
			AT91C_TC_LDRA_BOTH  |       //each edge of TIOA for CaputreA
			AT91C_TC_LDRB_NONE;         //None edge of TIOA for CaptureB

	//Compare-Register für Motor-Stop auf 30ms setzen
	AT91C_BASE_TCB->TCB_TC1.TC_RC = (0.030*MCK/32);
	
	//Timer-ISR initialiseren
	aic_mask_off(AT91C_ID_TC1);
	aic_set_vector(AT91C_ID_TC1, AIC_INT_LEVEL_ABOVE_NORMAL,timer1_isr_entry);
							 /*AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL */
	aic_mask_on(AT91C_ID_TC1);
	tc_sr = AT91C_BASE_TCB->TCB_TC1.TC_SR;                    //ISR-Reset
	(void)tc_sr;
	AT91C_BASE_TCB->TCB_TC1.TC_IER = AT91C_TC_LDRAS | AT91C_TC_CPCS;   //ISR-Enable
	
	AT91C_BASE_TCB->TCB_TC1.TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;  //Timer-Start

	//Timer2 -> MB0
	AT91C_BASE_TCB->TCB_TC2.TC_CMR = 
	        AT91C_TC_CLKS_TIMER_DIV3_CLOCK |  //MCK/32
	        /* AT91C_TC_CLKI | */       //ClockInvert
			AT91C_TC_BURST_NONE |       //Kein Burst mit XC012
			/* AT91C_TC_LDBSTOP | */    //Counter clock stopped with RB Load
			/* AT91C_TC_LDBDIS  | */    //Counter Clock Disabled with RB Loading
			AT91C_TC_ETRGEDG_BOTH |     //External Trigger(Reset) Edge Selection
			AT91C_TC_ABETRG  |          //TIOA or TIOB External Trigger(Reset) Selection
			/* AT91C_TC_CPCTRG  | */    //RC Compare Trigger Enable
			/* AT91C_TC_WAVE    | */    //Enable Waveform Mode
			AT91C_TC_LDRA_BOTH  |       //each edge of TIOA for CaputreA
			AT91C_TC_LDRB_NONE;         //None edge of TIOA for CaptureB

	//Compare-Register für Motor-Stop auf 30ms setzen
	AT91C_BASE_TCB->TCB_TC2.TC_RC = (0.030*MCK/32);
	
	//Timer-ISR initialiseren
	aic_mask_off(AT91C_ID_TC2);
	aic_set_vector(AT91C_ID_TC2, AIC_INT_LEVEL_ABOVE_NORMAL,timer2_isr_entry);
							 /*AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL */
	aic_mask_on(AT91C_ID_TC2);
	tc_sr = AT91C_BASE_TCB->TCB_TC2.TC_SR;                    //ISR-Reset
	(void)tc_sr;
	AT91C_BASE_TCB->TCB_TC2.TC_IER = AT91C_TC_LDRAS | AT91C_TC_CPCS;   //ISR-Enable
	
	AT91C_BASE_TCB->TCB_TC2.TC_CCR = AT91C_TC_CLKEN | AT91C_TC_SWTRG;  //Timer-Start
	
	return 0;
}

int nxt_motor_get(uint32_t port,uint32_t *pos,int16_t *speed)
{
	if(port >= NXT_N_MOTORS)
		return -1;

	struct motor *mot=&motor.motor[port];

	if(pos != NULL)
		*pos = mot->pos;
	
	//Anzahl der zu mittelnden Werte hängt
	//a) von der Anzahl der vorliegenden Werten motor[].flag  ab
	//b) von der Umdrehungsgeschwindigkeit des Motors
	//Zeitbedarf = f(n,Werte)
	//   U->   0,1    0,5	  1.0     1,5     2       2,5
	//n= 1    0,0555  0,0111  0,0055  0,0037  0,0027  0,0022
	//n= 2	  0,1111  0,0222  0,0111  0,0074  0,0055  0,0044
	//n= 4	  0,2222  0,0444  0,0222  0,0148  0,0111  0,0088
	//n= 8	  0,4444  0,0888  0,0444  0,0296  0,0222  0,0177
	//n=16	  0,8888  0,1777  0,0888  0,0592  0,0444  0,0355
	//n=32	  1,7777  0,3555  0,1777  0,1185  0,0888  0,0711
	//n=64	  3,5555  0,7111  0,3555  0,2370  0,1777  0,1422

	if(speed != NULL) {
		if(mot->flag<=SPEED_02) {
			*speed=0;
		}
		else {
			int        idx=mot->idx;	//Zwecks Datenintegrität, da ISR diese erhöhen könnte
			uint64_t   timer_sum=0;
			SPEED_FLAG flag=mot->flag;
			uint32_t   n=2;

			timer_sum=(uint64_t)mot->speed[ idx                       ]+
					  (uint64_t)mot->speed[(idx-1)&(MOTOR_SPEED_BUF-1)];
					  
			//Anzahl der mittelnden WErte wird dynamisch bestimmt
			while((flag >= SPEED_05) && (n<=8)) {
				uint64_t timer_add;
				
				timer_add=(uint64_t)mot->speed[(idx-n-0)&(MOTOR_SPEED_BUF-1)]+
						  (uint64_t)mot->speed[(idx-n-1)&(MOTOR_SPEED_BUF-1)];
						  
				if(timer_sum+timer_add > motor.timer_zyklus)
					break;
				timer_sum+=timer_add;
				n+=2;
				flag-=2;
			}
		
			//  TI = value * 32/MCK
			//  TI =    1/nis   / 360   //nis:Drehzahl in      U/sec   TI:Impulsbreite in s
			//  TI =   60/nim   / 360   //nim:Drehzahl in      U/min   TI:Impulsbreite in s
			//  TI = 1000/nits  / 360   //nit:Drehzahl in 1000*U/sec   TI:Impulsbreite in s
			//  nis  = (   1 * MCK) / (value * 32 * 360)   
			//  nim  = (  60 * MCK) / (value * 32 * 360)
			//  nits = (1000 * MCK) / (value * 32 * 360)
			//Gegenrechnung (Verdeutlichung des Zahlenbereiches)
			//  2,0U/sec -> 0,5s/U / 360 =  1,38ms  -> value =  2.083
			//  0,2U/sec -> 5,0s/U / 360 = 13,88ms  -> value = 20.833
			//  nis  = (   1 * MCK  ) / ( 2.083 * 32 * 360)
			//             48.000.000 /    23.996.160      = 2
			//  nis  = (   1 * MCK  ) / (20.833 * 32 * 360)
			//             48.000.000 /   239.996.160      = 0,2
			//  nim  = (  60 * MCK  ) / ( 2.083 * 32 * 360)
			//          2.880.000.000 /    23.996.160      = 120
			//  nim  = (  60 * MCK  ) / (20.833 * 32 * 360)
			//          2.880.000.000 /   239.996.160      =  12
			//  nits = ( 100 * MCK  ) / (2.083  *   1152  )
			//          4.800.000.000 /       239.545       = 2.003  <---
			//  nits = ( 100 * MCK  ) / (20.833 *   115   )
			//          4.800.000.000 /     2.395.795       =   200  <---
			*speed = mot->dir * (int16_t)((100LL * MCK * n) / (timer_sum * 1152));
		}	
	}
	return 0;
}

int nxt_motor_set(uint32_t n, int speed_percent, NXT_MOTOR_BRAKE brake)
{
	if((n < NXT_N_MOTORS) && (brake <=1)) {
		if (speed_percent > 100)
			speed_percent = 100;
		if (speed_percent < -100)
			speed_percent = -100;
		nxt_avr_set_motor(n, speed_percent, brake);
		return 0;
	}
	return -1;
}
