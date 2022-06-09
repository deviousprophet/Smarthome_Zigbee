/*
 * File:   main.c
 * Author: deviousprophet
 *
 * Created on January 17, 2022, 11:07 PM
 */

#include "main.h"
#include "it_handle.h"
#include "sys_tick.h"
#include "zb_znp.h"
#include "dht11.h"
#include "mq135.h"
#include "led_ind.h"
#include "event_groups.h"

#define DEVICE_STATE_EEPROM_ADDR    0

#define CLUSTER_ID_RELAY_CTRL       0x0001
#define CLUSTER_ID_BUTTON           0x0002
#define CLUSTER_ID_SENSOR           0x0003
#define CLUSTER_ID_COMPTEUR         0x0004

#define ZB_CONNECTED_BIT            BIT0

#define DEVICE_CLUSTER_ID           CLUSTER_ID_SENSOR

EventBits_t uxBits;

union {
    uint8_t raw[6];

    struct {
        dht11_data_t dht11_data;
        uint16_t ppm_adc;
    };
} sensor_data;

void znp_msg_handler(uint16_t cmd, uint8_t* data, uint8_t len) {
    switch (cmd) {
        case AF_INCOMING_MSG:
            break;
        case ZDO_STATE_CHANGE_IND:
            if (data[0] == 0x07) xEventGroupSetBits(ZB_CONNECTED_BIT);
            break;
        default:
            break;
    }
}

void znp_send_msg(uint8_t endpoint, bool wait_for_rsps) {
    uint8_t* data = NULL;
    uint8_t len = 0;
    if (endpoint == ENDPOINT_TELEMETRY) {
        if (dht11_read(&sensor_data.dht11_data) != DHT_OK)
            return;
        sensor_data.ppm_adc = mq135_read();
        data = sensor_data.raw;
        len = sizeof (sensor_data.raw);
    }

    znp_af_data_request(endpoint,
                        DEVICE_CLUSTER_ID,
                        data,
                        len,
                        wait_for_rsps);
}

void main(void) {
    CMCONbits.CM = 0b111; // Comparators Off
    INTCONbits.GIE = 1; // Enable global interrupt
    INTCONbits.PEIE = 1; // Enable peripheral interrupt
    OPTION_REGbits.nRBPU = 0;

    if (!PORTBbits.RB7) eeprom_write(DEVICE_STATE_EEPROM_ADDR, 0x00);

    sys_tick_init();
    xEventGroupCreate();

    dht11_init();
    mq135_init();
    led_ind_init();

    led_ind_blink_start();
    znp_init(znp_msg_handler);
    do {
        if (eeprom_read(DEVICE_STATE_EEPROM_ADDR) == 0xFE)
            znp_router_start(ROUTER_START_REJOIN);
        else znp_router_start(ROUTER_START_NEW_NETWORK);

        uxBits = xEventGroupWaitBits(ZB_CONNECTED_BIT, 100);
    } while (!(uxBits & ZB_CONNECTED_BIT));

    eeprom_write(DEVICE_STATE_EEPROM_ADDR, 0xFE);
    znp_send_msg(ENDPOINT_PROVISION, true);
    led_ind_blink_stop();

    sys_tick_t keepalive_tick, sensor_data_tick;
    keepalive_tick = sensor_data_tick = sys_tick_get_tick();

    while (1) {
        if (sys_tick_get_tick() - sensor_data_tick >= 50) {
            sensor_data_tick = sys_tick_get_tick();
            znp_send_msg(ENDPOINT_TELEMETRY, false);
        }
        if (sys_tick_get_tick() - keepalive_tick >= 20) {
            keepalive_tick = sys_tick_get_tick();
            znp_send_msg(ENDPOINT_KEEPALIVE, false);
            led_ind_on();
            __delay_ms(10);
            led_ind_off();
        }
    }
}
