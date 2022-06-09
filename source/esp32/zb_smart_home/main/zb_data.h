#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

#define UNKNOWN_CLUSTER_ID              0x0000
#define CLUSTER_ID_RELAY_CTRL           0x0001
#define CLUSTER_ID_BUTTON               0x0002
#define CLUSTER_ID_SENSOR               0x0003
#define CLUSTER_ID_COMPTEUR             0x0004

#define ENDPOINT_PROVISION              0x01
#define ENDPOINT_TELEMETRY              0x02
#define ENDPOINT_COMMAND                0x03
#define ENDPOINT_KEEPALIVE              0x04

#define DEFAULT_ZB_KEEPALIVE_TIMEOUT_MS 30000
#define ZB_KEEPALIVE_TIMEOUT            DEFAULT_ZB_KEEPALIVE_TIMEOUT_MS / 1000.0

typedef struct {
    uint8_t relay_channel_num;
    uint8_t relay_bit_mask;
} zb_relay_t;

typedef struct {
    uint8_t  RH1;
    uint8_t  RH2;
    uint8_t  T1;
    uint8_t  T2;
    uint16_t ppm_adc;
} zb_sensor_t;

typedef struct {
    float   irms;
    float   vrms;
    float   power;
    uint8_t relay;
} zb_compteur_t;

typedef union {
    struct {
        uint8_t relay_channel_num;
        uint8_t relay_bit_mask;
    };

    struct {
        float temperature;
        float humidity;
        float air_quality;
    };

    struct {
        float   irms;
        float   vrms;
        float   power;
        uint8_t relay;
    };
} zb_dev_data_t;

typedef struct zb_dev_t {
    struct zb_dev_t* next;
    uint16_t         cluster_id;
    uint16_t         short_addr;
    uint64_t         ieee_addr;
    time_t           last_seen;
    bool             is_alive;
    bool             new_data_updated;
    zb_dev_data_t*   data;
} zb_dev_t;

void zb_device_init(void);

void zb_print_dev_list(void);
void zb_add_device(uint16_t short_addr, uint64_t ieee_addr);

void zb_remove_device(uint64_t ieee_addr);

void zb_update_device_data(uint64_t ieee_addr, uint16_t cluster_id,
                           zb_dev_data_t* data);

void zb_update_device_last_seen(uint64_t ieee_addr);

void zb_update_device_last_seen_with_time(uint64_t ieee_addr, time_t last_seen);

void zb_set_device_cluster_id(uint64_t ieee_addr, uint16_t cluster_id);

uint8_t zb_get_device_count(void);

zb_dev_t* zb_get_device_list(void);

zb_dev_t* zb_get_device_with_ieee_addr(uint64_t ieee_addr);

zb_dev_t* zb_get_device_with_short_addr(uint16_t short_addr);

void zb_remove_all_device(void);

/*
 * NOTE: zb_get_device_*_msg() use cJSON_PrintUnformatted() to return a json
 * buffer. cJSON_PrintUnformatted() allocates memory for the string buffer so
 * use free() manually after using it to prevent memory leak.
 *
 * Follow this issue:
 * https://github.com/DaveGamble/cJSON/issues/297
 */

char* zb_get_device_provision_msg(zb_dev_t* device);

char* zb_get_device_telemetry_msg(zb_dev_t* device);
