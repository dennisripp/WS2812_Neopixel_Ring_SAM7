/*
 * sonar.c
 *
 *  Created on: 17.09.2012
 *      Author: Bormann
 */


#include "i2c.h"
#include "sonar.h"
#include "nxt_avr.h"
#include "systick.h"


static volatile uint8_t deviceStatus = DEVICE_NO_INIT;


/*==============================================================================
 * NXT I2C API
 *=============================================================================*/
/**
 * init a NXT sensor port for I2C communication
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 * @param type: LOWSPEED_9V/LOWSPEED
 */
void ecrobot_init_i2c(uint8_t port_id, uint8_t type)
{
	if (deviceStatus == DEVICE_NO_INIT)
	{
		nxt_avr_set_input_power(port_id, type);
		i2c_enable(port_id);
	}
}

/**
 * wait until I2C communication is ready
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 * @param wait: wait time out in msec
 * @return: 1(I2C is ready)/0(time out)
 */
uint8_t ecrobot_wait_i2c_ready(uint8_t port_id, uint32_t wait)
{
	volatile uint32_t time_out;

	time_out = systick_get_ms() + wait;
	while(systick_get_ms() <= time_out)
	{
		if (i2c_busy(port_id) == 0) return 1;
	}
	return 0; /* time out */
}

/**
 * send I2C data
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 * @param address: 0x01 to 0x7F
 *  Note that addresses are from 0x01 to 0x7F not
 *  even numbers from 0x02 to 0xFE as given in some I2C device specifications.
 *  They are 7-bit addresses not 8-bit addresses.
 * @param i2c_reg: I2C register e.g. 0x42
 * @param buf: buffer containing data to send
 * @param len: length of the data to send
 * @return: 1(success)/0(failure)
 */
int ecrobot_send_i2c(uint8_t port_id, uint32_t address, int i2c_reg, uint8_t *buf, uint32_t len)
{
	int ret;

	ecrobot_wait_i2c_ready(port_id, 50);
	ret = i2c_start_transaction(port_id, address, i2c_reg, 1, buf, len, 1);
	return !ret;
}

/**
 * read I2C data
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 * @param address: 0x01 to 0x7F
 *  Note that addresses are from 0x01 to 0x7F not
 *  even numbers from 0x02 to 0xFE as given in some I2C device specifications.
 *  They are 7-bit addresses not 8-bit addresses.
 * @param i2c_reg: I2C register e.g. 0x42
 * @param buf: buffer to return data
 * @param len: length of the return data
 * @return: 1(success)/0(failure)
 */
int ecrobot_read_i2c(uint8_t port_id, uint32_t address, int i2c_reg, uint8_t *buf, uint32_t len)
{
	int ret;+

	ecrobot_wait_i2c_ready(port_id, 50);
	ret = i2c_start_transaction(port_id, address, i2c_reg, 1, buf, len, 0);
	return !ret;
}

/**
 * terminate a NXT sensor port used for I2C communication
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 */
void ecrobot_term_i2c(uint8_t port_id)
{
	i2c_disable(port_id);
}





////////////////////////////////////////////////////////////

/*==============================================================================
 * NXT Ultrasonic Sensor API
 *=============================================================================*/
static int32_t distance_state[4] = {-1,-1,-1,-1}; /* -1: sensor is not connected */

//static int32_t getDistance(void)
//{
//	int i;
//
//	/*
// 	 * if multiple Ultrasonic Sensors are connected to a NXT,
// 	 * only the senosr measurement data which is connected to the smallest port id
// 	 * can be monitored in LCD and logging data.
// 	 */
//	for (i = 0; i < 4; i++)
//	{
//		if (distance_state[i] != -1)
//		{
//			return distance_state[i];
//		}
//	}
//	return -1;
//}



/**
 * init a NXT sensor port for Ultrasonic Sensor
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 */
void ecrobot_init_sonar_sensor(uint8_t port_id)
{
	ecrobot_init_i2c(port_id, LOWSPEED);
}





/**
 * get Ultrasonic Sensor measurement data in cm
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 * @return: distance in cm (0 to 255), -1 (failure)
 *  NOTE that this API has one cycle delay between data acquisition request
 *  and actual data acquisition.
 */
int32_t ecrobot_get_sonar_sensor(uint8_t port_id)
{
	static uint8_t data[4] = {0};

	if (i2c_busy(port_id) == 0)
	{
		distance_state[port_id] = (int32_t)data[port_id];
	   /* i2c_start_transaction just triggers an I2C transaction,
		* actual data transaction between ARM7 and a Ultrasonic
		* Sensor is done by an ISR after this, so there is one cycle
		* delay for consistent data acquistion
		*/
		i2c_start_transaction(port_id,1,0x42,1,&data[port_id],1,0);
	}

	return distance_state[port_id];
}





/**
 * terminate I2C used for for Ultrasonic Sensor
 *
 * @param port_id: NXT_PORT_S1/NXT_PORT_S2/NXT_PORT_S3/NXT_PORT_S4
 */
void ecrobot_term_sonar_sensor(uint8_t port_id)
{
	distance_state[0] = -1;
	distance_state[1] = -1;
	distance_state[2] = -1;
	distance_state[3] = -1;
	i2c_disable(port_id);
}





