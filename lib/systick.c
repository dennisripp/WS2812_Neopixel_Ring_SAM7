#include <stdlib.h>			//Fuer NULL
#include "../AT91SAM7S64.h"
#include "systick.h"
#include "aic.h"
#include "../main.h"

//Da der AIC nur die tatsächlich verbauten IRQ-Quellen unterstützt, 
//muss auf eine vorhandene Quelle zurückgegriffen werden, die 
//hoffentlich nicht von der eigentlichen Quelle benötigt wird
//Derzeit Quelle vom USB-Bus
#define LOW_PRIORITY_IRQ   AT91C_ID_UDP

/*static*/ volatile uint32_t systick_ms;

uint32_t systick_get_ms(void)
{
	//32-Bit Variable kann auf 32-Bit System in einen Rutsch gelesen werden
	return systick_ms;
}

void systick_get_ms_cpiv(SYSTICK_MS_CPIV *ptr)
{
	//2x32-Bit Variablen bedingen beim Lesen besondere Sorgfalt 
	do {
		ptr->ms   =  systick_ms;
		ptr->cpiv = (*AT91C_PITC_PIIR) & AT91C_PITC_CPIV;
	}
	while(ptr->ms != systick_ms);
}

/* Aktives Warten auf Zeitablauf */
void systick_wait_ms(int32_t ms)
{

#if 1
	//Aufgrund des möglichen Überlaufes hier besondere vorsicht
	int32_t start=(int32_t)systick_ms;
	while(((int32_t)systick_ms-(int32_t)start)<ms);
	//sys_ms  start	(sys_ms-start)>=5		(sys_ms-start)>=7
	//	250   	250	(-6)-(-6)=0>=5=false	(-6)-(-6)=0>=7=false
	//	251 	250	(-5)-(-6)=1>=5=false	(-5)-(-6)=1>=7=false
	//	252		250	(-4)-(-6)=2>=5=false	(-4)-(-6)=2>=7=false
	//	253		250	(-3)-(-6)=3>=5=false	(-3)-(-6)=3>=7=false
	//	254		250	(-2)-(-6)=4>=5=false	(-2)-(-6)=4>=7=false
	//	255		250	(-1)-(-6)=5>=5=true		(-1)-(-6)=5>=7=false
	//	0		250	( 0)-(-6)=6>=5=true		( 0)-(-6)=6>=7=false
	//	1		250	( 1)-(-6)=7>=5=true		( 1)-(-6)=7>=7=true
	//	2		250	( 2)-(-6)=8>=5=true		( 2)-(-6)=8>=7=true
		  
#elif 0
	//So bitte nicht, da hier Probleme mit Überlauf vorhanden sind
	volatile uint32_t final = ms + systick_ms;
	while (systick_ms < final);
#elif 0
	volatile int i,k;
	for(k=ms; k>0; k--) {
		for(i=11000; i>0; i--);
	}
#endif
}

void systick_wait_ns(uint32_t ns)
{
	volatile uint32_t x = (ns >> 7) + 1;

	while (x) {
		x--;
	}
}

/************************************************************/
void systick_suspend(void)
{
	aic_mask_off(LOW_PRIORITY_IRQ);
}

void systick_resume(void)
{
	aic_mask_on(LOW_PRIORITY_IRQ);
}

/************************************************************/
static void *vl_head;

void systick_callback(SYSTICK_VL *append,systick_fcn fcn)
{
	SYSTICK_VL *ptr;
	
	/* Ende der verketteten Liste suchen */
	for(ptr=(SYSTICK_VL *)&vl_head;
	    ptr->next!=NULL;
		ptr=ptr->next);
	
	systick_suspend();
	/* Neues Element am Ende anhängen */
	ptr->next=append;
	append->fcn =fcn;
	append->next=NULL;
	systick_resume();
}

/************************************************************/
__attribute__ ((section (".text.fastcode")))
void systick_isr_entry(void)
{
	uint32_t status;

	/* Read status to confirm interrupt */
	status = *AT91C_PITC_PIVR;
	
	// Update with number of ticks since last time
	systick_ms += (status & AT91C_PITC_PICNT) >> 20;

	// Trigger low priority task
	*AT91C_AIC_ISCR = (1 << LOW_PRIORITY_IRQ);
}

/* Nachteil: der IRQ Kontext von systick_isr_entry wird rekonstruiert */
/*           und der Kontext dieser Routine gesichert, also viel Aufwand */
/* Vorteil:  Wenn diese dann doch mal länger dauert und ein zweiter */
/*           Timer-IRQ kam, so wird diese Routine ausgesetzt*/
void systick_low_priority_entry(void)
{
	SYSTICK_VL *ptr;
	//Callback Funktionen aufrufen
	for(ptr=(SYSTICK_VL *)vl_head;
	    ptr!=NULL;
		ptr=ptr->next)
		ptr->fcn();
}

/************************************************************/

void systick_init(void)
{
	uint32_t value=((MCK / 16 / BSP_TICKS_PER_SEC) - 1);
	
	vl_head=NULL;
	
	int i_state = interrupts_get_and_disable();

	aic_mask_off(LOW_PRIORITY_IRQ);
	aic_set_vector(LOW_PRIORITY_IRQ, AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE | AIC_INT_LEVEL_LOW, systick_low_priority_entry);
	aic_mask_on(LOW_PRIORITY_IRQ);

	aic_mask_off  (AT91C_ID_SYS);
	aic_set_vector(AT91C_ID_SYS, AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE |  AIC_INT_LEVEL_NORMAL, systick_isr_entry);
							   /*AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL    |       1              */
	aic_clear     (AT91C_ID_SYS);
	aic_mask_on   (AT91C_ID_SYS);
	
	*AT91C_PITC_PIMR = AT91C_PITC_PITEN | AT91C_PITC_PITIEN | value;

	if (i_state)
		interrupts_enable();
}

