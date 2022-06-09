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
#include "led_ind.h"
#include "event_groups.h"

#define DEVICE_STATE_EEPROM_ADDR            0
#define BUTTON_NODE_LSB_ADDR_EEPROM_ADDR    2
#define BUTTON_NODE_MSB_ADDR_EEPROM_ADDR    3

#define CLUSTER_ID_RELAY_CTRL       0x0001
#define CLUSTER_ID_BUTTON           0x0002
#define CLUSTER_ID_SENSOR           0x0003
#define CLUSTER_ID_COMPTEUR         0x0004

#define ZB_CONNECTED_BIT            BIT0
#define SEND_TELEMETRY_TO_COORD_BIT BIT1
#define RELAY_STATE_CHANGED_BIT	    BIT2
#define NEW_BUTTON_NODE_ADDR        BIT3

#define DEVICE_CLUSTER_ID           CLUSTER_ID_RELAY_CTRL

#define RELAY_1                     PORTAbits.RA7
#define RELAY_2                     PORTAbits.RA0
#define RELAY_3                     PORTAbits.RA1
#define RELAY_STATE(num)            !RELAY_##num
#define RELAY_ON(num)               RELAY_##num = 0
#define RELAY_OFF(num)              RELAY_##num = 1
#define RELAY_TOGGLE(num)           RELAY_##num ^= 1
#define RELAY_SET_STATE(num, state) RELAY_##num = !(state)

EventBits_t uxBits;
uint16_t button_node_addr = 0;

union {
    uint8_t raw[2];

    struct {
        uint8_t channels_num;

        struct {
            uint8_t relay1 : 1;
            uint8_t relay2 : 1;
            uint8_t relay3 : 1;
            uint8_t reserved : 5;
        };
    };
} relay_data;

void relay_init(void) {
    relay_data.channels_num = 3;

    TRISAbits.TRISA7 = 0;
    TRISAbits.TRISA0 = 0;
    TRISAbits.TRISA1 = 0;

    RELAY_OFF(1);
    RELAY_OFF(2);
    RELAY_OFF(3);
    xEventGroupClearBits(RELAY_STATE_CHANGED_BIT);
}

void button1_handler(void) {
    if (!PORTBbits.RB5) {
        RELAY_TOGGLE(1);
        xEventGroupSetBits(RELAY_STATE_CHANGED_BIT);
    }
}

void button2_handler(void) {
    if (!PORTBbits.RB4) {
        RELAY_TOGGLE(2);
        xEventGroupSetBits(RELAY_STATE_CHANGED_BIT);
    }
}

void button3_handler(void) {
    RELAY_TOGGLE(3);
    xEventGroupSetBits(RELAY_STATE_CHANGED_BIT);
}

void button_init(void) {
    OPTION_REGbits.INTEDG = 0; // Interrupt on falling edge of RB0/INT pin
    ioc_rb5_add_isr_handler(button1_handler);
    ioc_rb4_add_isr_handler(button2_handler);
    ex_int_add_isr_handler(button3_handler);
}

void znp_msg_handler(uint16_t cmd, uint8_t* data, uint8_t len) {
    switch (cmd) {
        case AF_INCOMING_MSG:
        {
            // data6 = data7 = endpoint
            // data17 = command option

            // if data17 == 0
            // data18 = relay control bits
            // else if data17 == 1
            // data18:data19 = node button address

            uint8_t endpoint = data[6];

            if (endpoint == ENDPOINT_COMMAND) {
                if (data[17] == 0) {
                    if (data[18] & BIT5) RELAY_SET_STATE(1, data[18] & BIT0);
                    if (data[18] & BIT6) RELAY_SET_STATE(2, data[18] & BIT1);
                    if (data[18] & BIT7) RELAY_SET_STATE(3, data[18] & BIT2);
                    xEventGroupSetBits(RELAY_STATE_CHANGED_BIT);
                } else if (data[17] == 1) {
                    button_node_addr = BUILD_UINT16(data[18], data[19]);
                    xEventGroupSetBits(NEW_BUTTON_NODE_ADDR);
                }
            } else if (endpoint == ENDPOINT_TELEMETRY) {
                RELAY_SET_STATE(1, data[18] & BIT0);
                RELAY_SET_STATE(2, data[18] & BIT1);
                RELAY_SET_STATE(3, data[18] & BIT2);
                xEventGroupSetBits(SEND_TELEMETRY_TO_COORD_BIT);
            }
        }
            break;
        case ZDO_STATE_CHANGE_IND:
            if (data[0] == 0x07) xEventGroupSetBits(ZB_CONNECTED_BIT);
            break;
        default:
            break;
    }
}

void znp_send_msg(uint16_t dst_addr, uint8_t endpoint, bool wait_for_rsps) {
    uint8_t* data = NULL;
    uint8_t len = 0;
    if (endpoint == ENDPOINT_TELEMETRY) {
        relay_data.relay1 = RELAY_STATE(1);
        relay_data.relay2 = RELAY_STATE(2);
        relay_data.relay3 = RELAY_STATE(3);
        data = relay_data.raw;
        len = sizeof (relay_data.raw);
    }

    znp_af_data_request(dst_addr,
                        endpoint,
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

    relay_data.raw[1] |= 0xE0; // bit mask is always set to 1

    uint8_t button_node_lsb_addr
            = (uint8_t) eeprom_read(BUTTON_NODE_LSB_ADDR_EEPROM_ADDR);
    uint8_t button_node_msb_addr
            = (uint8_t) eeprom_read(BUTTON_NODE_MSB_ADDR_EEPROM_ADDR);
    button_node_addr
            = BUILD_UINT16(button_node_lsb_addr, button_node_msb_addr);

    sys_tick_init();
    xEventGroupCreate();
    __delay_ms(100);

    led_ind_init();
    relay_init();
    button_init();

    led_ind_blink_start();
    znp_init(znp_msg_handler);
    do {
        if (eeprom_read(DEVICE_STATE_EEPROM_ADDR) == 0xFE)
            znp_router_start(ROUTER_START_REJOIN);
        else znp_router_start(ROUTER_START_NEW_NETWORK);

        uxBits = xEventGroupWaitBits(ZB_CONNECTED_BIT, 100);
    } while (!(uxBits & ZB_CONNECTED_BIT));

    eeprom_write(DEVICE_STATE_EEPROM_ADDR, 0xFE);
    znp_send_msg(COORDINATOR_ADDR, ENDPOINT_PROVISION, true);
    led_ind_blink_stop();

    sys_tick_t keepalive_tick = sys_tick_get_tick();

    while (1) {
        uxBits = xEventGroupGetBits();
        if (uxBits & SEND_TELEMETRY_TO_COORD_BIT) {
            znp_send_msg(COORDINATOR_ADDR, ENDPOINT_TELEMETRY, true);
            xEventGroupClearBits(SEND_TELEMETRY_TO_COORD_BIT);
        }

        if (uxBits & RELAY_STATE_CHANGED_BIT) {
            znp_send_msg(COORDINATOR_ADDR, ENDPOINT_TELEMETRY, true);
            if (button_node_addr != 0)
                znp_send_msg(button_node_addr, ENDPOINT_TELEMETRY, true);
            xEventGroupClearBits(RELAY_STATE_CHANGED_BIT);
        }

        if (uxBits & NEW_BUTTON_NODE_ADDR) {
            eeprom_write(BUTTON_NODE_LSB_ADDR_EEPROM_ADDR, LSB(button_node_addr));
            eeprom_write(BUTTON_NODE_LSB_ADDR_EEPROM_ADDR, MSB(button_node_addr));
            xEventGroupClearBits(NEW_BUTTON_NODE_ADDR);
        }

        if (sys_tick_get_tick() - keepalive_tick >= 20) {
            keepalive_tick = sys_tick_get_tick();
            znp_send_msg(COORDINATOR_ADDR, ENDPOINT_KEEPALIVE, false);
            led_ind_on();
            __delay_ms(10);
            led_ind_off();
        }
    }
}
