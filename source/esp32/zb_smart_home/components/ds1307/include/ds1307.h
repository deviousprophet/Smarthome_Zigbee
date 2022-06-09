#pragma once

#include <esp_bit_defs.h>
#include <esp_err.h>
#include <stdint.h>
#include <sys/time.h>

#define DATE_STR_FORMAT         "%a %d/%m/%Y"
#define TIME_STR_FORMAT         "%X"

/* I2C slave address for DS1307 */
#define DS1307_I2C_ADDR         0x68

/* Registers location */
#define DS1307_SECONDS          0x00
#define DS1307_MINUTES          0x01
#define DS1307_HOURS            0x02
#define DS1307_DAY              0x03
#define DS1307_DATE             0x04
#define DS1307_MONTH            0x05
#define DS1307_YEAR             0x06
#define DS1307_CONTROL          0x07

#define DS1307_TIME_ADDR_OFFSET DS1307_SECONDS

/* Bits in control register */
#define DS1307_CONTROL_OUT      BIT7
#define DS1307_CONTROL_SQWE     BIT4
#define DS1307_CONTROL_RS1      BIT1
#define DS1307_CONTROL_RS0      BIT0

typedef enum {
    DS1307_OUTPUT_FREQ_LOW  = 0,
    DS1307_OUTPUT_FREQ_HIGH = DS1307_CONTROL_OUT,
    DS1307_OUTPUT_FREQ_1HZ  = DS1307_CONTROL_OUT | DS1307_CONTROL_SQWE,
    DS1307_OUTPUT_FREQ_4096Hz =
        DS1307_CONTROL_OUT | DS1307_CONTROL_SQWE | DS1307_CONTROL_RS0,
    DS1307_OUTPUT_FREQ_8192Hz =
        DS1307_CONTROL_OUT | DS1307_CONTROL_SQWE | DS1307_CONTROL_RS1,
    DS1307_OUTPUT_FREQ_32768Hz = DS1307_CONTROL_OUT | DS1307_CONTROL_SQWE |
                                 DS1307_CONTROL_RS1 | DS1307_CONTROL_RS0,
} ds1307_output_freq_t;

typedef void (*sq_output_freq_cb_t)(void);

typedef struct {
    int                 scl_io_num;
    int                 sda_io_num;
    int                 sq_io_num;
    sq_output_freq_cb_t sq_output_freq_cb;
} ds1307_config_t;

esp_err_t ds1307_init(ds1307_config_t *ds_cfg);
esp_err_t ds1307_check(void);

esp_err_t ds1307_get_datetime(struct tm *time);
esp_err_t ds1307_set_datetime(struct tm *time);

esp_err_t ds1307_set_output_freq(ds1307_output_freq_t frequency);