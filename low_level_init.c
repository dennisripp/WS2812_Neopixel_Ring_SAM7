/*****************************************************************************
* Product: QDK/C_ARM-GNU_AT91SAM7S-EK
* Last Updated for Version: 4.4.00
* Date of the Last Update:  Feb 26, 2012
*
*                    Q u a n t u m     L e a P s
*                    ---------------------------
*                    innovating embedded systems
*
* Copyright (C) 2002-2012 Quantum Leaps, LLC. All rights reserved.
*
* This software may be distributed and modified under the terms of the GNU
* General Public License version 2 (GPL) as published by the Free Software
* Foundation and appearing in the file GPL.TXT included in the packaging of
* this file. Please note that GPL Section 2[b] requires that all works based
* on this software must also be made publicly available under the terms of
* the GPL ("Copyleft").
*
* Alternatively, this software may be distributed and modified under the
* terms of Quantum Leaps commercial licenses, which expressly supersede
* the GPL and are specifically designed for licensees interested in
* retaining the proprietary status of their code.
*
* Contact information:
* Quantum Leaps Web site:  http://www.quantum-leaps.com
* e-mail:                  info@quantum-leaps.com
*****************************************************************************/
#include <stdint.h>
#include "AT91SAM7S64.h"                        /* AT91SAMT7S64 definitions */
#include "main.h"

/*..........................................................................*/
/* low_level_init() is invoked by the startup sequence after initializing
* the C stack, but before initializing the segments in RAM.
*
* low_level_init() is invoked in the ARM state. The function gives the
* application a chance to perform early initializations of the hardware.
* This function cannot rely on initialization of any static variables,
* because these have not yet been initialized in RAM.
*/
void low_level_init(void (*reset_addr)(), void (*return_addr)()) 
{
#if 0
    static uint32_t const LDR_PC_PC = 0xE59FF000U;
    static uint32_t const MAGIC = 0xDEADBEEFU;
#else
    #define LDR_PC_PC  0xE59FF000U
    #define MAGIC      0xDEADBEEFU
#endif
	(void) return_addr;
	
#ifndef MODE_SIM
/* Bei Samba ist der Prozessor schon auf 48MHz gestellt (andernfalls */
/* w?rde USB nicht laufen). Allerdings l?uft der Timer nicht mit der */
/* erwarteten Frequenz, so dass diese Initialisierung durchgef?hrt wird */
/* #ifndef MODE_SAMBA */
    AT91PS_PMC pPMC;

    /* Set flash wait sate FWS and FMCN 
	* MCK=47923200 (wird in bsp.h gesetzt)
	* 2 cycles for Read, 3 for Write operations
	*/	
    AT91C_BASE_MC->MC_FMR = ((AT91C_MC_FMCN) & ((MCK + 500000)/1000000 << 16))
                             | AT91C_MC_FWS_1FWS;

    /* Enable External Reset */
    AT91C_BASE_RSTC->RSTC_RMR = 0xA5000001;
	
    /* Enable the Main Oscillator:
    * set OSCOUNT to 6, which gives Start up time = 8 * 6 / SCK = 1.4ms
    * (SCK = 32768Hz)
    */
    pPMC = AT91C_BASE_PMC;
    pPMC->PMC_MOR = ((6 << 8) & AT91C_CKGR_OSCOUNT) | AT91C_CKGR_MOSCEN;
    while ((pPMC->PMC_SR & AT91C_PMC_MOSCS) == 0) {/* Wait the startup time */
    }

    /* Set the PLL and Divider:
    * - div by 5 Fin = 3,6864 =(18,432 / 5)
    * - Mul 25+1: Fout = 95,8464 =(3,6864 *26)
    * for 96 MHz the error is 0.16%
    * Field out NOT USED = 0
    * PLLCOUNT pll startup time estimate at : 0.844 ms
    * PLLCOUNT 28 = 0.000844 /(1/32768)
    */
    pPMC->PMC_PLLR = ((AT91C_CKGR_DIV & 0x05)
                      | (AT91C_CKGR_PLLCOUNT & (28 << 8))
                      | (AT91C_CKGR_MUL & (25 << 16)));
    while ((pPMC->PMC_SR & AT91C_PMC_LOCK) == 0) { /* Wait the startup time */
    }
    while ((pPMC->PMC_SR & AT91C_PMC_MCKRDY) == 0) {
    }

    /* Select Master Clock and CPU Clock select the PLL clock / 2 */
    pPMC->PMC_MCKR =  AT91C_PMC_PRES_CLK_2;
    while ((pPMC->PMC_SR & AT91C_PMC_MCKRDY) == 0) {
    }

    pPMC->PMC_MCKR |= AT91C_PMC_CSS_PLL_CLK;
    while ((pPMC->PMC_SR & AT91C_PMC_MCKRDY) == 0) {
    }
	
/* #endif */

    /* Setup the exception vectors to RAM
    *
    * NOTE: the exception vectors must be in RAM *before* the remap
    * in order to guarantee that the ARM core is provided with valid vectors
    * during the remap operation.
    */
	
    extern uint32_t __ram_start;  //Sollte ?ber Linker-CMD File beginnend ab          0x00200000 liegen
	                              //Dieser Bereich wird nach dem Remap Befehl auch ab 0x00000000 liegen

    /* setup the primary vector table in RAM */
    *(&__ram_start +  0) = LDR_PC_PC | 0x18;
    *(&__ram_start +  1) = LDR_PC_PC | 0x18;
    *(&__ram_start +  2) = LDR_PC_PC | 0x18;
    *(&__ram_start +  3) = LDR_PC_PC | 0x18;
    *(&__ram_start +  4) = LDR_PC_PC | 0x18;
    *(&__ram_start +  5) = MAGIC;
    *(&__ram_start +  6) = LDR_PC_PC | 0x18;
    *(&__ram_start +  7) = LDR_PC_PC | 0x18;

    /* setup the secondary vector table in RAM */
    *(&__ram_start +  8) = (uint32_t)reset_addr;
    *(&__ram_start +  9) = 0x04U;
    *(&__ram_start + 10) = 0x08U;
    *(&__ram_start + 11) = 0x0CU;
    *(&__ram_start + 12) = 0x10U;
    *(&__ram_start + 13) = 0x14U;
    *(&__ram_start + 14) = 0x18U;
    *(&__ram_start + 15) = 0x1CU;

    /* check if the Memory Controller has been remapped already */
	/* Wird ben?tigt, da sp?ter die Vector-Tabelle auf Adresse 0 ?berschrieben wird */
    if (MAGIC != (*(uint32_t volatile *)0x14)) {
        AT91C_BASE_MC->MC_RCR = 1;   /* perform Memory Controller remapping */
    }
#else
    /* setup the primary vector table in RAM */
    //*((uint32_t *) 0x00) = LDR_PC_PC | 0x18;   Andernfalls baut der Compiler hier 
	//                                           ein 'breapoint' ein
    *((uint32_t *) 0x04) = LDR_PC_PC | 0x18;
    *((uint32_t *) 0x08) = LDR_PC_PC | 0x18;
    *((uint32_t *) 0x0C) = LDR_PC_PC | 0x18;
    *((uint32_t *) 0x10) = LDR_PC_PC | 0x18;
    *((uint32_t *) 0x14) = MAGIC;
    *((uint32_t *) 0x18) = LDR_PC_PC | 0x18;
    *((uint32_t *) 0x1C) = LDR_PC_PC | 0x18;

    /* setup the secondary vector table in RAM */
    *((uint32_t *) 0x20) = (uint32_t)reset_addr;
    *((uint32_t *) 0x24) = 0x04U;
    *((uint32_t *) 0x28) = 0x08U;
    *((uint32_t *) 0x2C) = 0x0CU;
    *((uint32_t *) 0x30) = 0x10U;
    *((uint32_t *) 0x34) = 0x14U;
    *((uint32_t *) 0x38) = 0x18U;
    *((uint32_t *) 0x3C) = 0x1CU;
#endif
}

