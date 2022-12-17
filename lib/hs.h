#ifndef HS_H_
#define HS_H_

#include <stdint.h>

#define   HS_RX_PIN  AT91C_PIO_PA5
#define   HS_TX_PIN  AT91C_PIO_PA6
#define   HS_RTS_PIN AT91C_PIO_PA7

void hs_init(void);
//int hs_enable(void); // 10/04/16 takashic
int hs_enable(uint32_t baud_rate);
void hs_disable(void);
uint32_t hs_write(uint8_t *buf, uint32_t off, uint32_t len);
uint32_t hs_read(uint8_t * buf, uint32_t off, uint32_t len);
uint32_t hs_pending(void);

#endif /*HS_H_*/
