#include "ssd1306.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>

// Internal helper to send a command byte over I2C
static void ssd1306_send_command(ssd1306_t *dev, uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd}; // Control byte 0x00 means command
    i2c_write_blocking(dev->i2c, dev->address, buf, 2, false);
}

// Internal helper to send data bytes over I2C
static void ssd1306_send_data(ssd1306_t *dev, const uint8_t *data, size_t len) {
    // Data transfer starts with control byte 0x40
    uint8_t buf[len + 1];
    buf[0] = 0x40; // Control byte for data
    memcpy(&buf[1], data, len);
    i2c_write_blocking(dev->i2c, dev->address, buf, len + 1, false);
}

void ssd1306_init(ssd1306_t *dev, uint8_t width, uint8_t height,
                  i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin) {
    dev->width = width;
    dev->height = height;
    dev->i2c = i2c;
    dev->sda_pin = sda_pin;
    dev->scl_pin = scl_pin;
    dev->address = 0x3C; // Common I2C address for SSD1306

    // Initialize I2C pins & bus
    i2c_init(i2c, 400000);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    sleep_ms(100); // wait for display to power up

    // Initialization sequence from datasheet
    ssd1306_send_command(dev, 0xAE);             // Display off
    ssd1306_send_command(dev, 0x20);             // Set Memory Addressing Mode
    ssd1306_send_command(dev, 0x00);             // Horizontal addressing mode
    ssd1306_send_command(dev, 0xB0);             // Set Page Start Address for Page Addressing Mode
    ssd1306_send_command(dev, 0xC8);             // COM Output Scan Direction
    ssd1306_send_command(dev, 0x00);             // Low column address
    ssd1306_send_command(dev, 0x10);             // High column address
    ssd1306_send_command(dev, 0x40);             // Start line address
    ssd1306_send_command(dev, 0x81);             // Set contrast control
    ssd1306_send_command(dev, 0x7F);
    ssd1306_send_command(dev, 0xA1);             // Segment re-map
    ssd1306_send_command(dev, 0xA6);             // Normal display
    ssd1306_send_command(dev, 0xA8);             // Multiplex ratio
    ssd1306_send_command(dev, 0x3F);
    ssd1306_send_command(dev, 0xA4);             // Output RAM to Display
    ssd1306_send_command(dev, 0xD3);             // Display offset
    ssd1306_send_command(dev, 0x00);
    ssd1306_send_command(dev, 0xD5);             // Display clock divide ratio
    ssd1306_send_command(dev, 0x80);
    ssd1306_send_command(dev, 0xD9);             // Pre-charge period
    ssd1306_send_command(dev, 0xF1);
    ssd1306_send_command(dev, 0xDA);             // COM pins hardware configuration
    ssd1306_send_command(dev, 0x12);
    ssd1306_send_command(dev, 0xDB);             // VCOMH deselect level
    ssd1306_send_command(dev, 0x40);
    ssd1306_send_command(dev, 0x8D);             // Charge pump setting
    ssd1306_send_command(dev, 0x14);
    ssd1306_send_command(dev, 0xAF);             // Display ON

    // Clear buffer
    memset(dev->buffer, 0, sizeof(dev->buffer));
}

void ssd1306_clear(ssd1306_t *dev) {
    memset(dev->buffer, 0, sizeof(dev->buffer));
}

void ssd1306_show(ssd1306_t *dev) {
    // Display is organized in 8 pages (rows of 8 pixels)
    for (uint8_t page = 0; page < (dev->height / 8); page++) {
        ssd1306_send_command(dev, 0xB0 + page);  // Set page address
        ssd1306_send_command(dev, 0x00);         // Set lower column start address
        ssd1306_send_command(dev, 0x10);         // Set higher column start address

        ssd1306_send_data(dev, &dev->buffer[page * dev->width], dev->width);
    }
}

void ssd1306_draw_pixel(ssd1306_t *dev, uint8_t x, uint8_t y, bool color) {
    if (x >= dev->width || y >= dev->height) return;

    uint16_t byte_index = (y / 8) * dev->width + x;
    uint8_t bit_mask = 1 << (y % 8);

    if (color)
        dev->buffer[byte_index] |= bit_mask;
    else
        dev->buffer[byte_index] &= ~bit_mask;
}

void ssd1306_draw_rect(ssd1306_t *dev, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill) {
    for (uint8_t dx = 0; dx < w; dx++) {
        for (uint8_t dy = 0; dy < h; dy++) {
            if (fill) {
                ssd1306_draw_pixel(dev, x + dx, y + dy, true);
            } else {
                // Outline only
                if (dx == 0 || dx == w - 1 || dy == 0 || dy == h - 1) {
                    ssd1306_draw_pixel(dev, x + dx, y + dy, true);
                }
            }
        }
    }
}
