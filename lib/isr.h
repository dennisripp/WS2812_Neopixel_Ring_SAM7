#ifndef isr_h
#define isr_h

//Die tatsächlichen Funktionen sind nicht in C sonder in Assembler geschrieben!

int  interrupts_get_and_disable(void);
void interrupts_enable(void);
int  fiq_get_and_disable(void);
void fiq_enable(void);

void QF_undef(void);
void QF_swi(void);
void QF_pAbort(void);
void QF_dAbort(void);
void QF_reserved(void);
void QK_irq(void);
void QK_fiq(void);

#endif
