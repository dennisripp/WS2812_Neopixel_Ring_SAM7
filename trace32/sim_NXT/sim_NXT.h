/*********************************************************************/
/* Grundlage: dvd/filels/demo/simul/demoport                         */
/*********************************************************************/

/*********************************************************************/
/*      Speicher Aufteilung                                          */
/*********************************************************************/
#define LCD_MEMORY_OFFSET  0x10000000
#define NXT_MEMORY_OFFSET  0x20000000


/*********************************************************************/
/*       Port Aufteilung                                             */
/*********************************************************************/
#define PIO_PORTPIN_OFFSET 0x00
#define PIO_PORTPIN_WIDTH  0x20
#define PIO_PORTPA_OFFSET  0x20
#define PIO_PORTPA_WIDTH   0x20
#define PIO_PORTPB_OFFSET  0x40
#define PIO_PORTPB_WIDTH   0x20
 
#define PIO_PORT_PA0     (PIO_PORTPIN_OFFSET+ 0)
#define PIO_PORT_PA1     (PIO_PORTPIN_OFFSET+ 1)
#define PIO_PORT_PA2     (PIO_PORTPIN_OFFSET+ 2)
#define PIO_PORT_PA3     (PIO_PORTPIN_OFFSET+ 3)
#define PIO_PORT_PA4     (PIO_PORTPIN_OFFSET+ 4)
#define PIO_PORT_PA5     (PIO_PORTPIN_OFFSET+ 5)
#define PIO_PORT_PA6     (PIO_PORTPIN_OFFSET+ 6)
#define PIO_PORT_PA7     (PIO_PORTPIN_OFFSET+ 7)
#define PIO_PORT_PA8     (PIO_PORTPIN_OFFSET+ 8)
#define PIO_PORT_PA9     (PIO_PORTPIN_OFFSET+ 9)
#define PIO_PORT_PA10    (PIO_PORTPIN_OFFSET+10)
#define PIO_PORT_PA11    (PIO_PORTPIN_OFFSET+11)
#define PIO_PORT_PA12    (PIO_PORTPIN_OFFSET+12)
#define PIO_PORT_PA13    (PIO_PORTPIN_OFFSET+13)
#define PIO_PORT_PA14    (PIO_PORTPIN_OFFSET+14)
#define PIO_PORT_PA15    (PIO_PORTPIN_OFFSET+15)
#define PIO_PORT_PA16    (PIO_PORTPIN_OFFSET+16)
#define PIO_PORT_PA17    (PIO_PORTPIN_OFFSET+17)
#define PIO_PORT_PA18    (PIO_PORTPIN_OFFSET+18)
#define PIO_PORT_PA19    (PIO_PORTPIN_OFFSET+19)
#define PIO_PORT_FIQ     (PIO_PORTPIN_OFFSET+19)
#define PIO_PORT_PA20    (PIO_PORTPIN_OFFSET+20)
#define PIO_PORT_IRQ0    (PIO_PORTPIN_OFFSET+20)
#define PIO_PORT_PA21    (PIO_PORTPIN_OFFSET+21)
#define PIO_PORT_PA22    (PIO_PORTPIN_OFFSET+22)
#define PIO_PORT_PA23    (PIO_PORTPIN_OFFSET+23)
#define PIO_PORT_PA24    (PIO_PORTPIN_OFFSET+24)
#define PIO_PORT_PA25    (PIO_PORTPIN_OFFSET+25)
#define PIO_PORT_PA26    (PIO_PORTPIN_OFFSET+26)
#define PIO_PORT_PA27    (PIO_PORTPIN_OFFSET+27)
#define PIO_PORT_PA28    (PIO_PORTPIN_OFFSET+28)
#define PIO_PORT_PA29    (PIO_PORTPIN_OFFSET+29)
#define PIO_PORT_PA30    (PIO_PORTPIN_OFFSET+30)
#define PIO_PORT_IRQ1    (PIO_PORTPIN_OFFSET+30)
#define PIO_PORT_PA31    (PIO_PORTPIN_OFFSET+31)

#define PIO_PORT_DIGIA0  (PIO_PORTPIN_OFFSET+23)
#define PIO_PORT_DIGIA1  (PIO_PORTPIN_OFFSET+18)
#define PIO_PORT_DIGIB0  (PIO_PORTPIN_OFFSET+28)
#define PIO_PORT_DIGIB1  (PIO_PORTPIN_OFFSET+19)
#define PIO_PORT_DIGIC0  (PIO_PORTPIN_OFFSET+29)
#define PIO_PORT_DIGIC1  (PIO_PORTPIN_OFFSET+20)
#define PIO_PORT_DIGID0  (PIO_PORTPIN_OFFSET+30)
#define PIO_PORT_DIGID1  (PIO_PORTPIN_OFFSET+ 2)
#define PIO_PORT_TACHOA0 (PIO_PORTPIN_OFFSET+15)
#define PIO_PORT_INTA0   (PIO_PORTPIN_OFFSET+15)
#define PIO_PORT_TACHOA1 (PIO_PORTPIN_OFFSET+ 1)
#define PIO_PORT_DIRA    (PIO_PORTPIN_OFFSET+ 1)
#define PIO_PORT_TACHOB0 (PIO_PORTPIN_OFFSET+26)
#define PIO_PORT_INTB0   (PIO_PORTPIN_OFFSET+26)
#define PIO_PORT_TACHOB1 (PIO_PORTPIN_OFFSET+ 9)
#define PIO_PORT_DIRB    (PIO_PORTPIN_OFFSET+ 9)
#define PIO_PORT_TACHOC0 (PIO_PORTPIN_OFFSET+ 0)
#define PIO_PORT_INTC0   (PIO_PORTPIN_OFFSET+ 0)
#define PIO_PORT_TACHOC1 (PIO_PORTPIN_OFFSET+ 8)
#define PIO_PORT_DIRC    (PIO_PORTPIN_OFFSET+ 8)
#define PIO_PORT_SOBT    (PIO_PORTPIN_OFFSET+12)
#define PIO_PORT_LCD_CD  (PIO_PORTPIN_OFFSET+12)




#define AIC_PORTIRQ_OFFSET 0x100
#define AIC_PORTIRQ_WIDTH  32
#define AIC_PORTIRQ_FIQ    (AIC_PORTIRQ_OFFSET+ 0) // Advanced Interrupt Controller (FIQ)
#define AIC_PORTIRQ_SYS    (AIC_PORTIRQ_OFFSET+ 1) // System Peripheral
#define AIC_PORTIRQ_PIOA   (AIC_PORTIRQ_OFFSET+ 2) // Parallel IO Controller
#define AIC_PORTIRQ_R3     (AIC_PORTIRQ_OFFSET+ 3) // Reserved
#define AIC_PORTIRQ_ADC    (AIC_PORTIRQ_OFFSET+ 4) // Analog-to-Digital Converter
#define AIC_PORTIRQ_SPI    (AIC_PORTIRQ_OFFSET+ 5) // Serial Peripheral Interface
#define AIC_PORTIRQ_US0    (AIC_PORTIRQ_OFFSET+ 6) // USART 0
#define AIC_PORTIRQ_US1    (AIC_PORTIRQ_OFFSET+ 7) // USART 1
#define AIC_PORTIRQ_SSC    (AIC_PORTIRQ_OFFSET+ 8) // Serial Synchronous Controller
#define AIC_PORTIRQ_TWI    (AIC_PORTIRQ_OFFSET+ 9) // Two-Wire Interface
#define AIC_PORTIRQ_PWMC   (AIC_PORTIRQ_OFFSET+10) // PWM Controller
#define AIC_PORTIRQ_UDP    (AIC_PORTIRQ_OFFSET+11) // USB Device Port
#define AIC_PORTIRQ_TC0    (AIC_PORTIRQ_OFFSET+12) // Timer Counter 0
#define AIC_PORTIRQ_TC1    (AIC_PORTIRQ_OFFSET+13) // Timer Counter 1
#define AIC_PORTIRQ_TC2    (AIC_PORTIRQ_OFFSET+14) // Timer Counter 2
#define AIC_PORTIRQ_R15    (AIC_PORTIRQ_OFFSET+15) // Reserved
#define AIC_PORTIRQ_R16    (AIC_PORTIRQ_OFFSET+16) // Reserved
#define AIC_PORTIRQ_R17    (AIC_PORTIRQ_OFFSET+17) // Reserved
#define AIC_PORTIRQ_R18    (AIC_PORTIRQ_OFFSET+18) // Reserved
#define AIC_PORTIRQ_R19    (AIC_PORTIRQ_OFFSET+19) // Reserved
#define AIC_PORTIRQ_R20    (AIC_PORTIRQ_OFFSET+20) // Reserved
#define AIC_PORTIRQ_R21    (AIC_PORTIRQ_OFFSET+21) // Reserved
#define AIC_PORTIRQ_R22    (AIC_PORTIRQ_OFFSET+22) // Reserved
#define AIC_PORTIRQ_R23    (AIC_PORTIRQ_OFFSET+23) // Reserved
#define AIC_PORTIRQ_R24    (AIC_PORTIRQ_OFFSET+24) // Reserved
#define AIC_PORTIRQ_R25    (AIC_PORTIRQ_OFFSET+25) // Reserved
#define AIC_PORTIRQ_R26    (AIC_PORTIRQ_OFFSET+26) // Reserved
#define AIC_PORTIRQ_R27    (AIC_PORTIRQ_OFFSET+27) // Reserved
#define AIC_PORTIRQ_R28    (AIC_PORTIRQ_OFFSET+28) // Reserved
#define AIC_PORTIRQ_R29    (AIC_PORTIRQ_OFFSET+29) // Reserved
#define AIC_PORTIRQ_IRQ0   (AIC_PORTIRQ_OFFSET+30) // Advanced Interrupt Controller (IRQ0)
#define AIC_PORTIRQ_IRQ1   (AIC_PORTIRQ_OFFSET+31) // Advanced Interrupt Controller (IRQ1)


#define AIC_PORTIRQSYS_OFFSET 0x120
#define AIC_PORTIRQSYS_WIDTH  6
#define AIC_PORTIRQSYS_PIT    (AIC_PORTIRQSYS_OFFSET+0)
#define AIC_PORTIRQSYS_RTT    (AIC_PORTIRQSYS_OFFSET+1)
#define AIC_PORTIRQSYS_WDT    (AIC_PORTIRQSYS_OFFSET+2)
#define AIC_PORTIRQSYS_DBGU   (AIC_PORTIRQSYS_OFFSET+3)
#define AIC_PORTIRQSYS_PMC    (AIC_PORTIRQSYS_OFFSET+4)
#define AIC_PORTIRQSYS_RSTC   (AIC_PORTIRQSYS_OFFSET+5)

void *NXT_Init    (simulProcessor processor);
void  AIC_PortInit(simulProcessor processor);
void  PIT_PortInit(simulProcessor processor);
void  SPI_PortInit(simulProcessor processor,void *nxt);
void  TWI_PortInit(simulProcessor processor,void *nxt);
void  PIO_PortInit(simulProcessor processor,void *nxt);


void	avr_toavr(simulProcessor processor, simulPtr private, 
                     simulWord dadr, simulWord iadr, simulWord byte, simulWord thr);
					 
void	avr_fromavr(simulProcessor processor, simulPtr private, 
                        simulWord dadr, simulWord iadr, simulWord byte, simulWord *rhr);

void	lcd_sendreceive(simulProcessor processor, simulPtr private,
							simulWord tdr, simulWord *rdr);
						
void	pio_changed(simulProcessor processor, simulPtr private,
						  simulWord PIO_ABSR, simulWord PIO_PSR ,
	                      simulWord PIO_ODSR, simulWord PIO_OSR,
						  simulWord PIO_MDSR, simulWord PIO_PPUSR,
						  simulWord PIO_PDSR);
						