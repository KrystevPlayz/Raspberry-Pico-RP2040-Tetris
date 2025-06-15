#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

// I2C settings for OLED display
#define I2C_PORT i2c0
#define OLED_SDA 8
#define OLED_SCL 9

// GPIO pins for buttons and LED
#define BTN_LEFT 2
#define BTN_RIGHT 3
#define BTN_DOWN 4
#define LED_PIN 15

// Tetris grid dimensions
constexpr int GRID_WIDTH = 16;
constexpr int GRID_HEIGHT = 8;

// The game grid stores placed blocks (1 = occupied, 0 = empty)
uint8_t grid[GRID_HEIGHT][GRID_WIDTH] = {0};

// OLED display object
ssd1306_t display;

// Tetromino shapes encoded as 16-bit patterns, 4 rotations each
const uint16_t tetrominoes[10][4] = {
    {0x0F00,0x2222,0x0F00,0x2222},  // I shape
    {0x8E00,0x6440,0x0E20,0x44C0},  // J shape
    {0x2E00,0x4460,0x0E80,0xC440},  // L shape
    {0x6600,0x6600,0x6600,0x6600},  // O shape (square)
    {0x6C00,0x4620,0x6C00,0x4620},  // S shape
    {0x4E00,0x4640,0x0E40,0x4C40},  // T shape
    {0xC600,0x2640,0xC600,0x2640},  // Z shape
    {0x8000,0x8000,0x8000,0x8000},  // Single block (special)
    {0x4000,0xE000,0x4000,0x0000},  // Plus (+) shape
    {0x8800,0xC000,0x8800,0xC000}   // 2-block vertical line
};

// Current piece properties
int current_shape = 0;
int current_rotation = 0;
int block_x = GRID_WIDTH / 2 - 2;  // Horizontal position of piece (centered)
int block_y = 0;                   // Vertical position of piece (top)

// Game state flags
bool game_over = false;                  // True when no moves possible
bool game_over_led_flashed = false;     // Prevent multiple LED flashes on game over

// Draw a filled rectangle on the OLED display (used to draw blocks)
void ssd1306_draw_rect_fill(ssd1306_t *dev, uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill) {
    for (uint8_t dx = 0; dx < w; dx++) {
        for (uint8_t dy = 0; dy < h; dy++) {
            ssd1306_draw_pixel(dev, x + dx, y + dy, fill);
        }
    }
}

// Check if a particular cell in a tetromino shape/rotation is filled
bool tetromino_cell(int shape, int rotation, int x, int y) {
    uint16_t bits = tetrominoes[shape][rotation];
    // Each bit represents a cell in a 4x4 matrix (bit 15 top-left, bit 0 bottom-right)
    return (bits & (0x8000 >> (y * 4 + x))) != 0;
}

// Check if placing a shape at (x, y) with given rotation collides with existing blocks or walls
bool check_collision_shape(int x, int y, int shape, int rotation) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (tetromino_cell(shape, rotation, px, py)) {
                int gx = x + px;
                int gy = y + py;
                // Check boundaries (left, right, bottom)
                if (gx < 0 || gx >= GRID_WIDTH || gy >= GRID_HEIGHT)
                    return true;
                // Check collision with placed blocks (ignore if above visible grid)
                if (gy >= 0 && grid[gy][gx])
                    return true;
            }
        }
    }
    return false;
}

// Lock the current shape into the grid (mark the blocks as placed)
void lock_shape(int x, int y, int shape, int rotation) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (tetromino_cell(shape, rotation, px, py)) {
                int gx = x + px;
                int gy = y + py;
                // Only write if inside grid bounds
                if (gx >= 0 && gx < GRID_WIDTH && gy >= 0 && gy < GRID_HEIGHT) {
                    grid[gy][gx] = 1;
                }
            }
        }
    }
}

// Flash the LED on the board for a specified duration (milliseconds)
void led_flash(int duration_ms) {
    gpio_put(LED_PIN, 1);  // Turn LED ON
    sleep_ms(duration_ms);
    gpio_put(LED_PIN, 0);  // Turn LED OFF
}

// Check for any completed lines, remove them, and shift above lines down
void clear_lines() {
    bool line_cleared = false;

    for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
        bool full_line = true;
        // Check if the row is completely filled
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (!grid[y][x]) {
                full_line = false;
                break;
            }
        }

        if (full_line) {
            line_cleared = true;

            // Shift all rows above down by one
            for (int row = y; row > 0; row--) {
                for (int col = 0; col < GRID_WIDTH; col++) {
                    grid[row][col] = grid[row-1][col];
                }
            }
            // Clear top row
            for (int col = 0; col < GRID_WIDTH; col++) {
                grid[0][col] = 0;
            }
            y++; // Check this row again after shifting
        }
    }

    // Flash LED if any lines cleared
    if (line_cleared) {
        led_flash(100);
    }
}

// Draw the entire game state (grid + current piece) on the OLED display
void draw_game() {
    ssd1306_clear(&display);

    // Draw all placed blocks
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (grid[y][x]) {
                ssd1306_draw_rect_fill(&display, x*8, y*8, 8, 8, true);
            }
        }
    }

    // Draw the current falling piece
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (tetromino_cell(current_shape, current_rotation, px, py)) {
                int gx = block_x + px;
                int gy = block_y + py;
                // Only draw if inside grid bounds
                if (gx >= 0 && gx < GRID_WIDTH && gy >= 0 && gy < GRID_HEIGHT) {
                    ssd1306_draw_rect_fill(&display, gx*8, gy*8, 8, 8, true);
                }
            }
        }
    }

    ssd1306_show(&display);  // Refresh the OLED screen
}

// Spawn a new random tetromino piece at the top center of the grid
void spawn_piece() {
    current_shape = rand() % 10;  // Random shape from 0 to 10
    current_rotation = 0;
    block_x = GRID_WIDTH / 2 - 2;
    block_y = 0;

    // If new piece immediately collides, game is over
    if (check_collision_shape(block_x, block_y, current_shape, current_rotation)) {
        game_over = true;
    }
}

// Reset the game state to start a new game
void reset_game() {
    // Clear the grid
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            grid[y][x] = 0;
        }
    }
    game_over = false;
    game_over_led_flashed = false; // Reset LED flash flag
    spawn_piece();                  // Spawn the first piece
    draw_game();                   // Draw initial state
}

int main() {
    stdio_init_all();

    // Initialize I2C for OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);

    // Initialize buttons with pull-ups
    gpio_init(BTN_LEFT); gpio_set_dir(BTN_LEFT, GPIO_IN); gpio_pull_up(BTN_LEFT);
    gpio_init(BTN_RIGHT); gpio_set_dir(BTN_RIGHT, GPIO_IN); gpio_pull_up(BTN_RIGHT);
    gpio_init(BTN_DOWN); gpio_set_dir(BTN_DOWN, GPIO_IN); gpio_pull_up(BTN_DOWN);

    // Initialize LED pin as output, LED off initially
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    // Initialize OLED display
    ssd1306_init(&display, 128, 64, I2C_PORT, OLED_SDA, OLED_SCL);

    const int fall_interval_ms = 1500;            // Piece falls every 1.5 seconds
    absolute_time_t last_fall = get_absolute_time();

    srand((unsigned)to_us_since_boot(get_absolute_time())); // Seed random generator with time

    reset_game();                                 // Start fresh game

    bool needs_redraw = true;                      // Flag to track if display redraw needed
    absolute_time_t last_input_time = get_absolute_time();  // For button debounce timing
    bool game_over_displayed = false;              // Flag to clear screen only once on game over

    while (true) {
        if (!game_over) {
            // We are playing, reset game over display flag
            game_over_displayed = false;

            bool moved = false;  // Track if piece moved this frame

            // Simple input debounce of 150ms
            if (absolute_time_diff_us(last_input_time, get_absolute_time()) > 150000) {
                // Move left if button pressed and no collision
                if (!gpio_get(BTN_LEFT) && !check_collision_shape(block_x - 1, block_y, current_shape, current_rotation)) {
                    block_x--;
                    moved = true;
                    last_input_time = get_absolute_time();
                } 
                // Move right if button pressed and no collision
                else if (!gpio_get(BTN_RIGHT) && !check_collision_shape(block_x + 1, block_y, current_shape, current_rotation)) {
                    block_x++;
                    moved = true;
                    last_input_time = get_absolute_time();
                } 
                // Move down if button pressed and no collision
                else if (!gpio_get(BTN_DOWN) && !check_collision_shape(block_x, block_y + 1, current_shape, current_rotation)) {
                    block_y++;
                    moved = true;
                    last_input_time = get_absolute_time();
                }
            }

            if (moved) {
                needs_redraw = true;  // Redraw screen after movement
            }

            // Handle automatic piece falling by timing
            if (absolute_time_diff_us(last_fall, get_absolute_time()) > fall_interval_ms * 1000) {
                if (!check_collision_shape(block_x, block_y + 1, current_shape, current_rotation)) {
                    block_y++;  // Move piece down
                } else {
                    // Piece cannot move down, lock it in place
                    lock_shape(block_x, block_y, current_shape, current_rotation);
                    led_flash(100);  // Flash LED to indicate lock
                    clear_lines();   // Clear any completed lines
                    spawn_piece();   // Spawn a new piece
                }
                needs_redraw = true;
                last_fall = get_absolute_time();
            }

            // Redraw the display if needed
            if (needs_redraw) {
                draw_game();
                needs_redraw = false;
            }
        } else {
            // Game over state

            // Clear the screen only once when game over detected
            if (!game_over_displayed) {
                ssd1306_clear(&display);
                ssd1306_show(&display);
                game_over_displayed = true;
            }

            // Flash LED once on game over
            if (!game_over_led_flashed) {
                led_flash(300);
                game_over_led_flashed = true;
            }

            // Wait for user to press DOWN button to reset the game
            if (!gpio_get(BTN_DOWN)) {
                reset_game();
                sleep_ms(300);  // Debounce delay to avoid multiple immediate resets
            }

            sleep_ms(100);  // Slow down loop during game over to save CPU
        }

        sleep_ms(10);  // Small delay for CPU friendliness in main loop
    }
    return 0;
}
