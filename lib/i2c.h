#ifndef __I2C_H__
#define __I2C_H__

#include <stdint.h>

#define I2C_N_PORTS 4

void i2c_disable(int port);
void i2c_enable(int port);

void i2c_init(void);

int i2c_busy(int port);
int i2c_start_transaction(int port, 
                      uint32_t address, 
                      int internal_address, 
                      int n_internal_address_bytes, 
                      uint8_t *data, 
                      uint32_t nbytes,
                      int write);


void i2c_test(void);

#endif
