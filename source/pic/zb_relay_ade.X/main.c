/*
 * File:   main.c
 * Author: deviousprophet
 *
 * Created on January 17, 2022, 11:07 PM
 */

#include "main.h"
#include "it_handle.h"
#include "event_groups.h"
#include "sys_tick.h"
#include "zb_znp.h"
#include "ade7753.h"
#include "led_ind.h"

#define DEVICE_STATE_EEPROM_ADDR    0

#define CLUSTER_ID_RELAY_CTRL       0x0001
#define CLUSTER_ID_BUTTON           0x0002
#define CLUSTER_ID_SENSOR           0x0003
#define CLUSTER_ID_COMPTEUR         0x0004

#define ZB_CONNECTED_BIT            BIT0
#define ADE_ZX_BIT                  BIT1
#define RELAY_STATE_CHANGED_BIT     BIT2

#define DEVICE_CLUSTER_ID           CLUSTER_ID_COMPTEUR

#define RELAY_PORT                  PORTDbits.RD2
#define RELAY_STATE                 !RELAY_PORT
#define RELAY_ON()                  RELAY_PORT = 0
#define RELAY_OFF()                 RELAY_PORT = 1
#define RELAY_TOGGLE()              RELAY_PORT ^= 1

EventBits_t uxBits;
ade_config_t ade_cfg = ADE_DEFAULT_CONFIG();

union {
    uint8_t raw[sizeof (ade_power_meter_data_t) + sizeof (uint8_t)];

    struct {
        ade_power_meter_data_t meter_data;

        struct {
            uint8_t relay_state : 1;
            uint8_t reserved : 7;
        };
    };
} dev_data;

void relay_init(void) {
    TRISDbits.TRISD2 = 0;
    RELAY_OFF();
}

void button_handler(void) {
    if (!PORTBbits.RB1) {
        RELAY_TOGGLE();
        xEventGroupSetBits(RELAY_STATE_CHANGED_BIT);
    }
}

void button_init(void) {
    ioc_rb1_add_isr_handler(button_handler);
}

void znp_msg_handler(uint16_t cmd, uint8_t* data, uint8_t len) {
    switch (cmd) {
        case AF_INCOMING_MSG:
            if ((data[6] == data[7]) && (data[6] == ENDPOINT_COMMAND)) {
                if (data[17]) RELAY_ON();
                else RELAY_OFF();
                xEventGroupSetBits(RELAY_STATE_CHANGED_BIT);
            }
            break;
        case ZDO_STATE_CHANGE_IND:
            if (data[0] == 0x07) xEventGroupSetBits(ZB_CONNECTED_BIT);
        default:
            break;
    }
}

void znp_send_msg(uint8_t endpoint, bool wait_for_rsps) {
    uint8_t* data = NULL;
    uint8_t len = 0;
    if (endpoint == ENDPOINT_TELEMETRY) {
        dev_data.relay_state = RELAY_STATE;
        data = dev_data.raw;
        len = sizeof (dev_data.raw);
    }

    znp_af_data_request(endpoint,
                        DEVICE_CLUSTER_ID,
                        data,
                        len,
                        wait_for_rsps);
}

void ade_zx_cb_handler(void) {
    xEventGroupSetBits(ADE_ZX_BIT);
}

void main(void) {
    OSCCONbits.IRCF = 0b111; // 8MHz Internal Oscillator Frequency
    ANSEL = ANSELH = 0; // Digital I/O. Pin is assigned to port or special function
    ADCON1 = 0;
    INTCONbits.GIE = 1; // Enable global interrupt
    INTCONbits.PEIE = 1; // Enable peripheral interrupt

    memset(dev_data.raw, 0, sizeof (dev_data.raw));
    OPTION_REGbits.nRBPU = 0;

    if (!PORTBbits.RB7) eeprom_write(DEVICE_STATE_EEPROM_ADDR, 0x00);

    sys_tick_init();
    xEventGroupCreate();
    __delay_ms(100);

    led_ind_init();
    relay_init();
    button_init();

    ade_init();
    ade_cfg.is_reversed = 1;
    ade_cfg.mode.CYCMODE = true;
    ade_cfg.zx_cb = ade_zx_cb_handler;
    ade_config(&ade_cfg);

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

    sys_tick_t keepalive_tick, ade_data_tick;
    keepalive_tick = ade_data_tick = sys_tick_get_tick();

    while (1) {
        uxBits = xEventGroupGetBits();

        if (sys_tick_get_tick() - ade_data_tick >= 20) {
            ade_data_tick = sys_tick_get_tick();
            xEventGroupClearBits(ADE_ZX_BIT);
            xEventGroupWaitBits(ADE_ZX_BIT, 2);
            xEventGroupClearBits(ADE_ZX_BIT);
            ade_get_power_meter_data(&dev_data.meter_data);
            ade_config(&ade_cfg);
            znp_send_msg(ENDPOINT_TELEMETRY, false);
        }

        if (sys_tick_get_tick() - keepalive_tick >= 20) {
            keepalive_tick = sys_tick_get_tick();
            znp_send_msg(ENDPOINT_KEEPALIVE, false);
            led_ind_on();
            __delay_ms(10);
            led_ind_off();
        }

        if (uxBits & RELAY_STATE_CHANGED_BIT) {
            /*
             * @Note: ADE7753 configuration sometimes reset after relay state
             * changed probably because of noise from relay. Delay for a little
             * time and configurate ADE7753 again.
             */
            __delay_ms(100);
            ade_config(&ade_cfg);

            /* Update relay state */
            znp_send_msg(ENDPOINT_TELEMETRY, false);
            xEventGroupClearBits(RELAY_STATE_CHANGED_BIT);
        }
    }
}
