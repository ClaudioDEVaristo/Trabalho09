#ifndef ws2812
#define ws2812

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

static inline void put_pixel(uint32_t pixel_grb);
static inline uint32_t urgb_u32(char *color_name);
void set_led(uint8_t x, uint8_t y, char *color_name);
void update_matrix(PIO pio, uint sm);
void set_pattern(PIO pio, uint sm, uint8_t pattern, char *color_name);
int get_color_index(char *color);
void clear_matrix(PIO pio, uint sm);

#endif
