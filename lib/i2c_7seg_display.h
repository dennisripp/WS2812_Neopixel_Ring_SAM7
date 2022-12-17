/*
 * i2c_7seg_display.h
 *
 *  Created on: 25.09.2012
 *      Author: Bormann
 */

#ifndef I2C_7SEG_DISPLAY_H_
#define I2C_7SEG_DISPLAY_H_


void segment_display_init(int port);
void display_segment(int port,int state);

#endif /* I2C_7SEG_DISPLAY_H_ */
