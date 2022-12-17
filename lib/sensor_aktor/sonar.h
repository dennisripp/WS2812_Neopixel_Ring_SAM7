/*
 * sonar.h
 *
 *  Created on: 17.09.2012
 *      Author: Bormann
 */

#ifndef SONAR_H_
#define SONAR_H_

#include <stdint.h>

#define LOWSPEED_9V 1
#define LOWSPEED    2


typedef enum {
	NXT_PORT_S1,
	NXT_PORT_S2,
	NXT_PORT_S3,
	NXT_PORT_S4
}SENSOR_PORT_T;

typedef enum {
	EXECUTED_FROM_FLASH,
	EXECUTED_FROM_SRAM,
	DEVICE_NO_INIT,
	DEVICE_INITIALIZED,
	BT_NO_INIT,
	BT_INITIALIZED,
	BT_CONNECTED,
	BT_STREAM,
}SYSTEM_T;

/* NXT ultrasonic sensor API */
extern void    ecrobot_init_sonar_sensor(uint8_t port_id);
extern int32_t ecrobot_get_sonar_sensor (uint8_t port_id);
extern void    ecrobot_term_sonar_sensor(uint8_t port_id);


extern void ecrobot_init_i2c(uint8_t port_id, uint8_t type);
extern int  ecrobot_send_i2c(uint8_t port_id, uint32_t address, int i2c_reg, uint8_t *buf, uint32_t len);



#endif /* SONAR_H_ */
