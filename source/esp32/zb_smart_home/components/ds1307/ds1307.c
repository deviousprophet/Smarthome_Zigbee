#include "ds1307.h"

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#define SETMINMAX(data, min, max)                                              \
    ((data > max) ? max : ((data < min) ? min : data))

#define BCD_2_U8(bcd)                                                          \
    ((uint8_t)((bcd > 0x99) ? 255 : (bcd >> 4) * 10 + (bcd & 0x0F)))

#define U8_2_BCD(integer)                                                      \
    ((uint8_t)((integer > 99) ? 255 : ((integer / 10) << 4) + (integer % 10)))

sq_output_freq_cb_t output_freq_cb;
SemaphoreHandle_t   sq_semphr;

static const char *TAG = "ds1307";

static void IRAM_ATTR ds_sq_handler(void *arg) {
    xSemaphoreGiveFromISR(sq_semphr, NULL);
}

static void ds_output_freq_task(void *arg) {
    while (1) {
        if (xSemaphoreTake(sq_semphr, portMAX_DELAY)) output_freq_cb();
    }
}

static esp_err_t ds1307_write(uint8_t ds_reg, uint8_t data) {
    uint8_t buffer[2] = {ds_reg, data};
    return i2c_master_write_to_device(I2C_NUM_0, DS1307_I2C_ADDR, buffer, 2,
                                      1000 / portTICK_PERIOD_MS);
}

static esp_err_t ds1307_write_multi(uint8_t ds_reg_offset, uint8_t *data,
                                    uint8_t length) {
    esp_err_t ret;
    for (uint8_t i = 0; i < length; i++) {
        ret = ds1307_write(ds_reg_offset + i, data[i]);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

static esp_err_t ds1307_read(uint8_t ds_reg, uint8_t *data) {
    return i2c_master_write_read_device(I2C_NUM_0, DS1307_I2C_ADDR, &ds_reg, 1,
                                        data, 1, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t ds1307_read_multi(uint8_t ds_reg_offset, uint8_t *data,
                                   uint8_t length) {
    esp_err_t ret;
    for (uint8_t i = 0; i < length; i++) {
        ret = ds1307_read(ds_reg_offset + i, data + i);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t ds1307_init(ds1307_config_t *ds_cfg) {
    ESP_LOGI(TAG, "DS1307 init");
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = ds_cfg->sda_io_num,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_io_num       = ds_cfg->scl_io_num,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,  // 100kHz
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    if (ds_cfg->sq_io_num != -1) {
        output_freq_cb = ds_cfg->sq_output_freq_cb;
        gpio_set_direction(ds_cfg->sq_io_num, GPIO_MODE_INPUT);
        gpio_set_intr_type(ds_cfg->sq_io_num, GPIO_INTR_POSEDGE);
        gpio_set_pull_mode(ds_cfg->sq_io_num, GPIO_PULLUP_ONLY);

        sq_semphr = xSemaphoreCreateBinary();
        xTaskCreate(ds_output_freq_task, "ds_output_freq_task", 2048, NULL, 10,
                    NULL);

        gpio_install_isr_service(0);
        gpio_isr_handler_add(ds_cfg->sq_io_num, ds_sq_handler, NULL);
    }

    while (ds1307_check() != ESP_OK) vTaskDelay(2000 / portTICK_PERIOD_MS);

    return ds1307_set_output_freq(DS1307_OUTPUT_FREQ_HIGH);
}

esp_err_t ds1307_check(void) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, DS1307_I2C_ADDR << 1, true);
    i2c_master_stop(cmd);
    esp_err_t ret =
        i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) ESP_LOGW(TAG, "Not connected!");

    vTaskDelay(10 / portTICK_PERIOD_MS);
    return ret;
}

esp_err_t ds1307_get_datetime(struct tm *time) {
    uint8_t   data[7];
    esp_err_t ret = ds1307_read_multi(DS1307_TIME_ADDR_OFFSET, data, 7);
    if (ret != ESP_OK) return ret;

    time->tm_sec   = BCD_2_U8(data[0]);
    time->tm_min   = BCD_2_U8(data[1]);
    time->tm_hour  = BCD_2_U8(data[2]); /* 24H */
    time->tm_wday  = BCD_2_U8(data[3]) - 1;
    time->tm_mday  = BCD_2_U8(data[4]);
    time->tm_mon   = BCD_2_U8(data[5]) - 1;
    time->tm_year  = BCD_2_U8(data[6]) + 100;
    time->tm_isdst = 0;

    return ESP_OK;
}

esp_err_t ds1307_set_datetime(struct tm *time) {
    uint8_t data[7];
    data[0] = U8_2_BCD(SETMINMAX((uint8_t)time->tm_sec, 0, 59));
    data[1] = U8_2_BCD(SETMINMAX((uint8_t)time->tm_min, 0, 59));
    data[2] = U8_2_BCD(SETMINMAX((uint8_t)time->tm_hour, 0, 23));
    data[3] = U8_2_BCD(SETMINMAX((uint8_t)(time->tm_wday + 1), 1, 7));
    data[4] = U8_2_BCD(SETMINMAX((uint8_t)time->tm_mday, 1, 31));
    data[5] = U8_2_BCD(SETMINMAX((uint8_t)(time->tm_mon + 1), 1, 12));
    data[6] = U8_2_BCD(SETMINMAX((uint8_t)(time->tm_year - 100), 0, 99));

    return ds1307_write_multi(DS1307_TIME_ADDR_OFFSET, data, 7);
}

esp_err_t ds1307_set_output_freq(ds1307_output_freq_t frequency) {
    return ds1307_write(DS1307_CONTROL, frequency);
}
