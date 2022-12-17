#ifndef _ECROBOT_RS485_H_
#define _ECROBOT_RS485_H_

//#include "ecrobot_types.h"
#include <stdint.h>

#define MAX_RS485_TX_DATA_LEN   (64)
#define MAX_RS485_RX_DATA_LEN   (64)
#define DEFAULT_BAUD_RATE_RS485 (921600) // [bps]

/* NXT RS485 API */
extern void ecrobot_init_rs485(uint32_t baud_rate);
extern void ecrobot_term_rs485(void);
extern  uint32_t ecrobot_send_rs485(uint8_t* buf, uint32_t off, uint32_t len);
extern  uint32_t ecrobot_read_rs485(uint8_t* buf, uint32_t off, uint32_t len);

#endif
