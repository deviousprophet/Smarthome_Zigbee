#pragma once

#include <cJSON.h>
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    struct {
        uint8_t am_pm : 1;
        uint8_t hours : 7;
    };
    uint8_t minutes;
    uint8_t seconds;
} time_data_t;

typedef union {
    uint8_t raw;
    struct {
        uint8_t monday    : 1;
        uint8_t tuesday   : 1;
        uint8_t wednesday : 1;
        uint8_t thursday  : 1;
        uint8_t friday    : 1;
        uint8_t saturday  : 1;
        uint8_t sunday    : 1;
        uint8_t           : 1;
    };
} weekday_bits_t;

typedef struct {
    time_data_t    time_on;
    time_data_t    time_off;
    weekday_bits_t weekday;

    /* Channel mask for Node Relay */
    bool channel_1;
    bool channel_2;
    bool channel_3;
} timeslot_t;

typedef struct zb_dev_timeslot_t {
    struct zb_dev_timeslot_t* next;
    uint16_t                  cluster_id;
    uint64_t                  ieee_addr;
    timeslot_t                timeslot;
} zb_dev_timeslot_t;

esp_err_t json_parse_timeslot(cJSON* data, timeslot_t* timeslot);

void set_timeslot_to_dev(uint64_t ieee_addr, uint16_t cluster_id,
                         timeslot_t* timeslot);

void remove_timeslot_from_dev(uint64_t ieee_addr);

zb_dev_timeslot_t* get_timeslot_list(void);

void print_timeslot_list(void);