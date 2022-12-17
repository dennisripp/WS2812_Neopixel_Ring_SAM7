#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <stdint.h>

void display_init(void);

void display_update(void);

void display_force_update(void);

void display_clear(uint32_t updateToo);

void display_goto_xy(int x, int y);

void display_char(int c);

void display_string(const char *str);

void display_int(int val, uint32_t places);
void display_hex(uint32_t val, uint32_t places);

void display_unsigned(uint32_t val, uint32_t places);

void display_bitmap_copy(const uint8_t *data, uint32_t width, uint32_t depth, uint32_t x, uint32_t y);

void display_test(void);

uint8_t *display_get_buffer(void);

void display_set_auto_update(int);

extern int display_tick;
extern int display_auto_update;

#endif
