#pragma once

#include <stdbool.h>
#include <stdint.h>

// #define USE_PRINTF_INSTEAD_FOR_PUTS

#define LCD5110_WIDTH               84
#define LCD5110_HEIGHT              48

// LCD5110 Commandset
// ------------------
// General commands
#define LCD5110_POWERDOWN           0x04
#define LCD5110_ENTRYMODE           0x02
#define LCD5110_EXTENDEDINSTRUCTION 0x01
#define LCD5110_DISPLAYBLANK        0x00
#define LCD5110_DISPLAYNORMAL       0x04
#define LCD5110_DISPLAYALLON        0x01
#define LCD5110_DISPLAYINVERTED     0x05
// Normal instruction set
#define LCD5110_FUNCTIONSET         0x20
#define LCD5110_DISPLAYCONTROL      0x08
#define LCD5110_SETYADDR            0x40
#define LCD5110_SETXADDR            0x80
// Extended instruction set
#define LCD5110_SETTEMP             0x04
#define LCD5110_SETBIAS             0x10
#define LCD5110_SETVOP              0x80
// Display presets
#define LCD5110_LCD_BIAS            0x03  // Range: 0-7 (0x00-0x07)
#define LCD5110_LCD_TEMP            0x02  // Range: 0-3 (0x00-0x03)
#define LCD5110_LCD_CONTRAST        0x46  // Range: 0-127 (0x00-0x7F)

#define LCD5110_CHAR5x7_WIDTH       6  // 5*8
#define LCD5110_CHAR5x7_HEIGHT      8
#define LCD5110_CHAR3x5_WIDTH       4  // 3*5
#define LCD5110_CHAR3x5_HEIGHT      6

#define LCD5110_BUFFER_SIZE         (LCD5110_WIDTH * LCD5110_HEIGHT) / 8

#define LCD5110_DEFAULT_CONTRAST    0x37

typedef enum {
    LCD5110_COMMAND,
    LCD5110_DATA,
} lcd5110_write_type_t;

typedef enum {
    LCD5110_PIXEL_CLEAR,
    LCD5110_PIXEL_SET,
} lcd5110_pixel_t;

typedef enum {
    LCD5110_FONT_SIZE_3x5,
    LCD5110_FONT_SIZE_5x7,
} lcd5110_font_size_t;

typedef struct {
    uint8_t contrast;
    int     rst_io_num;
    int     ce_io_num;
    int     dc_io_num;
    int     din_io_num;
    int     clk_io_num;
} lcd5110_config_t;

void lcd5110_init(lcd5110_config_t* lcd_config);
void lcd5110_clear(void);
void lcd5110_home(void);
void lcd5110_refresh(void);
void lcd5110_goto_xy(uint8_t x, uint8_t y);
void lcd5110_putc(lcd5110_pixel_t color, lcd5110_font_size_t size, char c);
void lcd5110_puts(lcd5110_pixel_t color, lcd5110_font_size_t size,
                  const char* format, ...)
    __attribute__((format(printf, 3, 4)));

void lcd5110_display_BK_logo(bool inverted);