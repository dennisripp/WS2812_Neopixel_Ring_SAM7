/* Driver for the AT91SAM7's Advanced Interrupt Controller (AIC).
 *
 * The AIC is responsible for queuing interrupts from other
 * peripherals on the board. It then hands them one by one to the ARM
 * CPU core for handling, according to each peripheral's configured
 * priority.
 */

#include "../AT91SAM7S64.h"
#include "../term.h"
#include "aic.h"

 uint8_t QK_intNest_=0; /* start with nesting level of 0 */
 uint8_t QK_intMax_ =0;  /* Max nexting int */
/* uint8_t QK_schedPrio_(void);  ->qk_schedPrio.c                           */


void Q_onAssert(char const * const file, uint32_t line) 
{
	/* interrupts_get_and_disable(); */
	TERM_STRING("!!!!!! ");
	TERM_STRING(file);
	TERM_STRING("PC=");
	TERM_HEX(line,8);
    while(1);
}

/* Default handlers for the three general kinds of interrupts that the
 * ARM core has to handle.  */
void  __attribute__((weak)) default_isr(void)
{
	TERM_STRING("AIC-IRQ nicht zugeordnet");
	while(1);
}

void  __attribute__((weak)) default_fiq(void)
{
	TERM_STRING("AIC-FIQ-IRQ nicht zugeordnet");
	while(1);
}
void  __attribute__((weak)) spurious_isr(void)
{
	TERM_STRING("SPURIOUS-IRQ nicht zugeordnet");
	while(1);
}

__attribute__ ((section (".text.fastcode")))
void BSP_irq(void) {
    IntVector vect = (IntVector)AT91C_BASE_AIC->AIC_IVR;    /* read the IVR */
    AT91C_BASE_AIC->AIC_IVR = (AT91_REG)vect; /* write AIC_IVR if protected */
    asm("MSR cpsr_c,#(0x1F)");                  /* allow nesting interrupts */
	if(vect !=spurious_isr)
    (*vect)();              /* call the IRQ ISR via the pointer to function */
    asm("MSR cpsr_c,#(0x1F | 0x80 | 0x40)");  /* lock IRQ/FIQ before return */
    AT91C_BASE_AIC->AIC_EOICR = 0;    /* write AIC_EOICR to clear interrupt */
	
	/*ToDo: Damit bei anstehenden niederprioren Task nicht erneut der Kontext gesichert */
	/*      werden muss, wäre hier eine Abfrage sinnvoll, ob eine weitere Prio zur      */
	/*      Bearbeitung ansteht. Würde bedingen, dass der AIC den Interrupt per Level   */
	/*      und nicht per Edge Sensitiv an den Prozessor meldet                         */
}

__attribute__ ((section (".text.fastcode")))
void BSP_fiq(void) {
    /* NOTE: Do NOT enable interrupts throughout the whole FIQ processing. */
    /* NOTE: Do NOT write EOI to the AIC */

	/* NOTE: The QK FIQ assembly "wrapper" QK_fiq() calls the FIQ handler  */
	/*       BSP_fiq() with interrupts locked at the ARM core level.       */
	/*       In contrast to the IRQ line, the FIQ line is NOT prioritized  */
	/*       by the AIC. Therefore, you must NOT enable interrupts while   */
	/*       processing FIQ. All FIQs should be the highest-priority in    */
	/*       the system. All FIQs run at the same (highest) priority level.*/
}


void aic_init(void)
{
	int i;

	interrupts_get_and_disable();

	/* In die vom low_level_init() vorgefertigte Sprungtabelle neue Ziele eintragen */
    *(uint32_t volatile *)0x24 = (uint32_t)&QF_undef;
    *(uint32_t volatile *)0x28 = (uint32_t)&QF_swi;
    *(uint32_t volatile *)0x2C = (uint32_t)&QF_pAbort;
    *(uint32_t volatile *)0x30 = (uint32_t)&QF_dAbort;
    *(uint32_t volatile *)0x34 = (uint32_t)&QF_reserved;

    *(uint32_t volatile *)0x38 = (uint32_t)&QK_irq;
    *(uint32_t volatile *)0x3C = (uint32_t)&QK_fiq;

  /* If we're coming from a warm boot, the AIC may be in a weird
   * state. Do some cleaning up to bring the AIC back into a known
   * state:
   *  - All interrupt lines disabled,
   *  - No interrupt lines handled by the FIQ handler,
   *  - No pending interrupts,
   *  - AIC idle, not handling an interrupt.
   */
	AT91C_BASE_AIC->AIC_IDCR = 0xFFFFFFFF;    /* disable all interrupts */
	AT91C_BASE_AIC->AIC_FFDR = 0xFFFFFFFF;    /* disable fast forcing */
	AT91C_BASE_AIC->AIC_ICCR = 0xFFFFFFFF;    /* clear all interrupts */
	//AT91C_BASE_AIC->AIC_EOICR = 1;
	for (i = 0; i < 8; ++i) {
		AT91C_BASE_AIC->AIC_EOICR = 0;        /* write AIC_EOICR 8 times */
	}

  /* Enable debug protection. This is necessary for JTAG debugging, so
   * that the hardware debugger can read AIC registers without
   * triggering side-effects.
   */
#ifndef MODE_SIM
  //Selbstgebauter AIC-Simulator hat hier noch ein Bug, bzw. unterstützt dieses Flag nicht
  //Wenn im Simulationsmodus eingeschaltet, wird direkt ein IRQ ausgelöst, der dann 
  //aufgrund einer fehlenden Quelle als spurious-IRQ ausgeführt wird
  AT91C_BASE_AIC->AIC_DCR = 1;
#endif

  /* Set default handlers for all interrupt lines. */
  for (i = 0; i < 32; i++) {
    AT91C_BASE_AIC->AIC_SMR[i] = 0;
    AT91C_BASE_AIC->AIC_SVR[i] = (uint32_t) default_isr;
  }
  /* Macht eigentlich  keinen Sinn, da der FIQ über eigenen 'PIN' reinkommt*/
  AT91C_BASE_AIC->AIC_SVR[AT91C_ID_FIQ] = (uint32_t) default_fiq;
  AT91C_BASE_AIC->AIC_SPU               = (uint32_t) spurious_isr;

}


/* Register an interrupt service routine for an interrupt line.
 *
 * Note that while this function registers the routine in the AIC, it
 * does not enable or disable the interrupt line for that vector. Use
 * aic_mask_on and aic_mask_off to control actual activation of the
 * interrupt line.
 *
 * Args:
 *   vector: The peripheral ID to claim (see AT91SAM7.h for peripheral IDs)
 *   mode: The priority of this interrupt in relation to others. See aic.h
 *         for a list of defined values.
 *   isr: A pointer to the interrupt service routine function.
 */
void aic_set_vector(uint32_t vector, uint32_t mode, IntVector isr)
{
	if (vector < 32) {
		int i_state = interrupts_get_and_disable();

		AT91C_BASE_AIC->AIC_SMR[vector] = mode;
		AT91C_BASE_AIC->AIC_SVR[vector] = (uint32_t)isr;
		if (i_state)
			interrupts_enable();
	}
}

/* Enable handling of an interrupt line in the AIC.
 *
 * Args:
 *   vector: The peripheral ID of the interrupt line to enable.
 */
void aic_mask_on(uint32_t vector)
{
	int i_state = interrupts_get_and_disable();

	AT91C_BASE_AIC->AIC_IECR = (1 << vector);
	if (i_state)
		interrupts_enable();
}


/* Disable handling of an interrupt line in the AIC.
 *
 * Args:
 *   vector: The peripheral ID of the interrupt line to disable.
 */
void aic_mask_off(uint32_t vector)
{
	int i_state = interrupts_get_and_disable();

	AT91C_BASE_AIC->AIC_IDCR = (1 << vector);
	if (i_state)
		interrupts_enable();
}


/* Clear an interrupt line in the AIC.
 *
 * Args:
 *   vector: The peripheral ID of the interrupt line to clear.
 */
void aic_clear(uint32_t vector)
{
	int i_state = interrupts_get_and_disable();

	AT91C_BASE_AIC->AIC_ICCR = (1 << vector);
	if (i_state)
		interrupts_enable();
}
