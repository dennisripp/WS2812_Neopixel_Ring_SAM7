#ifndef SOUND_H_
#define SOUND_H_

#include <stdint.h>

void sound_init(void);
void sound_interrupt_enable();
void sound_interrupt_disable();
void sound_enable();
void sound_disable();
void sound_isr_C();

void sound_freq       (uint32_t freq, uint32_t ms);
void sound_freq_vol   (uint32_t freq, uint32_t ms, int vol);
void sound_play_sample(uint8_t *data, uint32_t length, uint32_t freq, int vol);
void sound_set_volume (int vol);
int sound_get_volume  (void);
int sound_get_time    (void);

#define MAXVOL 100

#endif /*SOUND_H_*/
