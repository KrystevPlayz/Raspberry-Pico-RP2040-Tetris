#ifndef SSD1306_H
#define SSD1306_H

#include "hardware/i2c.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t width;
    uint8_t height;
    i2c_inst_t *i2c;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t address;
    uint8_t buffer[1024]; // For 128x64 OLED (128 * 64 / 8)
} ssd1306_t;

// Initializes the display
void ssd1306_init(ssd1306_t *dev, uint8_t width, uint8_t height,
                  i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin);

// Clears the internal buffer
void ssd1306_clear(ssd1306_t *dev);

// Pushes the buffer to the OLED display
void ssd1306_show(ssd1306_t *dev);

// Draws a single pixel
void ssd1306_draw_pixel(ssd1306_t *dev, uint8_t x, uint8_t y, bool color);

// Draws a filled or empty rectangle
void ssd1306_draw_rect(ssd1306_t *dev, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H
