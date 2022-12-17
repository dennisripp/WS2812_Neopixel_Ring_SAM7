#ifndef __NXT_MOTORS_H__
#define __NXT_MOTORS_H__

#include <stdint.h>

#define NXT_N_MOTORS 3

typedef enum {           /* Verhalten des Motors beim Stoppen */
	NXT_MOTOR_BRAKE_FREILAUF, /* frei auslaufen lassen            */
	NXT_MOTOR_BREKE_BRAKE     /* Durch Kurzschluss der Anschlüsse */
} NXT_MOTOR_BRAKE;            /*    schnell auf 0 runterbremsen   */

int nxt_motor_init(uint32_t zyklus);
int nxt_motor_set(uint32_t n, int speed_percent,NXT_MOTOR_BRAKE brake);
int nxt_motor_get(uint32_t port,uint32_t *pos,int16_t *speed);
#endif
