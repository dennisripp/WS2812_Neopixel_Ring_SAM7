/*****************************************************************************
* Product: QDK-ARM
* Last Updated for Version: 4.1.03
* Date of the Last Update:  Mar 11, 2010
*
*                    Q u a n t u m     L e a P s
*                    ---------------------------
*                    innovating embedded systems
*
* Copyright (C) 2002-2010 Quantum Leaps, LLC. All rights reserved.
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

/*********************************************************************************
  CPSR=Current Programm Status Word
  SPSR=Saved Programm Status Word
  User mode: The only non-privileged mode. (Kein Zugriff auf CPSR)
  FIQ mode: A privileged mode that is entered whenever the processor accepts a fast interrupt request.
  IRQ mode: A privileged mode that is entered whenever the processor accepts an interrupt.
  Supervisor (svc) mode: A privileged mode entered whenever the CPU is reset or when an SVC instruction is executed.
  Abort mode: A privileged mode that is entered whenever a prefetch abort or data abort exception occurs.
  Undefined mode: A privileged mode that is entered whenever an undefined instruction exception occurs.
  System mode: The only privileged mode that is not entered by an exception. It can only be entered by executing an instruction that explicitly writes to the mode bits of the Current Program Status Register (CPSR) from another privileged mode (not from user mode).
  
  CPSR 31 30 29 28 27 .. 24 .. 19-16 .. 9 8 7 6 5 4-0
        |  |  |  |  |     +-> J Jazelle | | | | |  +--> Mode 10000=User-Mode R0..R14
        |  |  |  |  +-----> Q Underflow | | | | |            10001=FIQ-Mode  R8..R14,SPSR
        |  |  |  |            Saturation| | | | |            10010=IRQ-Mode  R13..R14,SPSR
        |  |  |  +--------> V Overflow  | | | | |            10011=SVC-Mode  R13..R14,SPSR
        |  |  +-----------> C carry     | | | | |            10111=ABORT-Mode R13..R14,SPSR
        |  +--------------> Z Zero      | | | | |            11011=UNDEF-Mode R13..R14,SPSR
        +-----------------> N Negative  | | | | |            11111=Sys-Mode  R0..R14
                                        | | | | +-----> T Thumb-Mode
                                        | | | +-------> F When set, disable FIQ interrupts
                                        | | +---------> I When set, disable IRQ interrupts
                                        | +-----------> A When set, disable imprecise aborts
                                        +-------------> Endianess, Bit or Little
***********************************************************************************
The ARM architecture (pre-ARMv8) provides a non-intrusive way of extending the instruction set using "coprocessors" 
that can be addressed using MCR, MRC, MRRC, MCRR and similar instructions. The coprocessor space is divided logically 
into 16 coprocessors with numbers from 0 to 15, coprocessor 15 (cp15) being reserved for some typical control 
functions like managing the caches and MMU operation on processors that have one.
In ARM-based machines, peripheral devices are usually attached to the processor by mapping their physical registers 
into ARM memory space, into the coprocessor space, or by connecting to another device (a bus) that in turn attaches 
to the processor. Coprocessor accesses have lower latency, so some peripherals—for example, an XScale interrupt 
controller—are accessible in both ways: through memory and through coprocessors.
**********************************************************************************/
/* Standard definitions of Mode bits and Interrupt (I & F) flags in PSRs */

    .equ    I_BIT,          0x80      /* when I bit is set, IRQ is disabled */
    .equ    F_BIT,          0x40      /* when F bit is set, FIQ is disabled */

    .equ    USR_MODE,       0x10      /* User-Mode */
    .equ    FIQ_MODE,       0x11      /* FIQ-Mode  */
    .equ    IRQ_MODE,       0x12      /* IRQ-Mode  */
    .equ    SVC_MODE,       0x13      /* Supervisor-Mode */
    .equ    ABT_MODE,       0x17      /* Abort-Mode */
    .equ    UND_MODE,       0x1B      /* Undefined-Mode */
    .equ    SYS_MODE,       0x1F      /* SystemMode*/

/* constant to pre-fill the stack */
    .equ    STACK_FILL,     0xAAAAAAAA

/*****************************************************************************
* The starupt code must be linked at the start of ROM, which is NOT
* necessarily address zero.
*/
    .text
    .code 32

    .global _vectors
    .func   _vectors
_vectors:

    /* Vector table
    * NOTE: used only very briefly until RAM is remapped to address zero
	  Note: relativer Sprung zu _reset, so dass diese auch nah anzuordnen ist
    */
    B      _reset           /* Reset: relative branch allows remap */
    B       .               /* Undefined Instruction */
    B       .               /* Software Interrupt    */
    B       .               /* Prefetch Abort        */
    B       .               /* Data Abort            */
    B       .               /* Reserved              */
    B       .               /* IRQ                   */
    B       .               /* FIQ                   */
	
.ifdef SIM
	/* Im SIM-Mode wird dieser Bereich durch low-Level 'überschrieben' */
	/* Daher hier ein Platzhalter für die absoluten Sprungadressen der */
	/* Sprungtabelle vorsehen                                          */
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
.endif

    .size   _vectors, . - _vectors   /* Tatsächliche Speichergröße für das Symbol */
	                                 /* _vectors ermitteln */
    .endfunc

        /* The copyright notice embedded prominently at the begining of the ROM */
    .string "Copyright (c) OSTFALIA Justen "
    .align 2                               /* re-align to the word boundary */


/*****************************************************************************
* _reset
*/
	.global _reset  /* Damit dieses Label in der Symboltabelle erscheint */
    .func   _reset  /* damit im MAP-File dargestellt wird                */

_reset:

    /* Call the platform-specific low-level initialization routine
    *
    * NOTE: The ROM is typically NOT at its linked address before the remap,
    * so the branch to low_level_init() must be relative (position
    * independent code). The low_level_init() function must continue to
    * execute in ARM state. Also, the function low_level_init() cannot rely
    * on uninitialized data being cleared and cannot use any initialized
    * data, because the .bss and .data sections have not been initialized yet.
    */
    LDR     r0,=_reset      /* pass the reset address as the 1st argument */
    LDR     r1,=_cstartup   /* pass the return address as the 2nd argument */
    MOV     lr,r1           /* set the return address after the remap */
    LDR     sp,=__stack_end__  /* set the temporary stack pointer */
    /*B       low_level_init   relative branch enables remap */
	/*                        Problem, da low_level_init aus 0x0000 0xxx ausgeführt wird*/
	/*                        und beim Remap der Bode unter den Boden weggezogen wird*/
    LDR     r12,=low_level_init
    BX      r12             /* absolute branch enables remap */
	                        /* bx 0x0010 0xxx  */
							/* Return Adress 0x0010 0xxx*/

    /* NOTE: Durch das absolute setzen des LR-Registers wird zur tatsächlichen Adresse
	         zurückgekehrt und nicht zur vorherigen Ausführungsadresse 0x0000 0xxxx  */

_cstartup:
.ifdef COPY_ROM2RAM
    /* Relocate .fastcode section (copy from ROM to RAM) */
    LDR     r0,=__fastcode_load
    LDR     r1,=__fastcode_start
    LDR     r2,=__fastcode_end
1:
    CMP     r1,r2
    LDMLTIA r0!,{r3}
    STMLTIA r1!,{r3}
    BLT     1b
.endif

.ifdef COPY_ROM2RAM
    /* Relocate the .data section (copy from ROM to RAM) */
    LDR     r0,=__data_load
    LDR     r1,=__data_start
    LDR     r2,=_edata
1:
    CMP     r1,r2
    LDMLTIA r0!,{r3}
    STMLTIA r1!,{r3}
    BLT     1b
.endif

    /* Clear the .bss section (zero init) */
    LDR     r1,=__bss_start__
    LDR     r2,=__bss_end__
    MOV     r3,#0
1:
    CMP     r1,r2
    STMLTIA r1!,{r3}
    BLT     1b

dummy:
    /* Fill the .stack section */
    LDR     r1,=__stack_start__
    LDR     r2,=__stack_end__
    LDR     r3,=STACK_FILL
1:
    CMP     r1,r2
    STMLTIA r1!,{r3}
    BLT     1b
    /* Initialize stack pointer for the SYSTEM mode (C-stack) */
    MSR     CPSR_c,#(SYS_MODE | I_BIT | F_BIT)
    LDR     sp,=__c_stack_top__                  /* set the C stack pointer */
    /* Initialize stack pointer for the Undefined mode 
	   -> Nicht nötig, da UND im System-Mode ausgeführt wird
    MSR     CPSR_C,#(UND_MODE | I_BIT | F_BIT)
	LDR     sp,=....                                                */
    /* Initialize stack pointer for the Abort mode 
	   -> Nicht nötig, das ABT im System-Mode ausgeführt wird
    MSR     CPSR_C,#(ABT_MODE | I_BIT | F_BIT)
	LDR     sp,=....                                                */
    /* Initialize stack pointer for the Supervsior mode 
	   -> Nicht nötig, das SVR im System-Mode ausgeführt wird 
    MSR     CPSR_C,#(SVC_MODE | I_BIT | F_BIT)
	LDR     sp,=....                                                */
    /* Initialize stack pointer for the Interrupt mode 
	   -> Nicht nötig, da IRQ im System-Mode ausgeführt wird 
    MSR     CPSR_C,#(IRQ_MODE | I_BIT | F_BIT)
	LDR     sp,=....                                                */
    /* Initialize stack pointer for the FastInterrupt mode 
	   -> Nicht nötig, das FIQ im System-Mode ausgeführt wird
    MSR     CPSR_C,#(FIQ_MODE | I_BIT | F_BIT)
	LDR     sp,=....                                                */
    /* Initialize stack pointer for the User mode
       -> Nicht nötig, da identisch zu System-Mode ausgeführt wird	
    MSR     CPSR_C,#(USR_MODE | I_BIT | F_BIT)
	LDR     sp,=....                                                */

/* extern void (*__preinit_array_start []) (void) __attribute__((weak));*/
/* extern void (*__preinit_array_end []) (void) __attribute__((weak));  */
/* extern void (*__init_array_start []) (void) __attribute__((weak));   */
/* extern void (*__init_array_end []) (void) __attribute__((weak));     */
/* extern void _init (void);                                            */
/* void __libc_init_array (void) {                                      */
/*   size_t count;                                                      */
/*   size_t i;                                                          */
/*   count = __preinit_array_end - __preinit_array_start;               */
/*   for (i = 0; i < count; i++)                                        */
/*     __preinit_array_start[i] ();                                     */
/*   _init ();                                                          */
/*   count = __init_array_end - __init_array_start;                     */
/*   for (i = 0; i < count; i++)                                        */
/*     __init_array_start[i] ();                                        */
/* }                                                                    */
/* In Section init_array wird von newLib der Zeiger auf frame_dummy()   */
/* eingetragen, welches den Stack fürs Exception Handling von C++       */
/*    vorbereitet                                                       */
/* Weiter Eintragungen mit:                                             */
/*    void __attribute__((constructor)) test(void)                      */
/*   oder händisch über                                                 */
/* 	__attribute__((section(".init_array"))) static void(*fcn)(void)     */
/*  __attribute__((unused)) = {newlib_syscalls_init};                   */
    LDR     r12,=__libc_init_array
    MOV     lr,pc
    BX      r12

    /* Enter the C/C++ code */
	mov     r0,#2           /* argc */
	ldr     r1,PARGV        /* argv */
	mov     r2,#0           /* env  */
    LDR     r12,=main
    MOV     lr,pc           /* set the return address */
    BX      r12             /* the target code can be ARM or THUMB */
    /*b main*/

	LDR     r12,=_exit
	mov     lr,pc
	bx      r12
.if 1
	b  .
.else
    SWI     0xFFFFFF        /* cause exception if main() ever returns */
.endif

/************************************************************************/
	.align 2
	.global PARGV
	PARGV: .word ARGV
	
    .size   _reset, . - _reset
    .endfunc

	.section	.rodata
	.align	2
	.global ARGV0
	ARGV0:		.ascii	"argv0\000"
	.align  2
	.global ARGV1
	ARGV1:      .ascii  "ARGV11\000"

	.global ARGV
	.align 2
	ARGV:       .word ARGV0
	            .word ARGV1
				.word 0

/************************************************************************/
    .end
