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

#define DEVICE_STATE_EEPROM_ADDR        0
#define RELAY_NODE_LSB_ADDR_EEPROM_ADDR 2
#define RELAY_NODE_MSB_ADDR_EEPROM_ADDR 3

#define CLUSTER_ID_RELAY_CTRL       0x0001
#define CLUSTER_ID_BUTTON           0x0002
#define CLUSTER_ID_SENSOR           0x0003
#define CLUSTER_ID_COMPTEUR         0x0004

#define ZB_CONNECTED_BIT            BIT0
#define SEND_TELEMETRY_TO_COORD_BIT BIT1
#define LED_STATE_CHANGED_BIT       BIT2
#define NEW_RELAY_NODE_ADDR         BIT3

#define DEVICE_CLUSTER_ID           CLUSTER_ID_BUTTON

#define LED_1                       PORTAbits.RA7
#define LED_2                       PORTAbits.RA0
#define LED_3                       PORTAbits.RA1
#define LED_STATE(num)              LED_##num
#define LED_ON(num)                 LED_##num = 1
#define LED_OFF(num)                LED_##num = 0
#define LED_TOGGLE(num)             LED_##num ^= 1
#define LED_SET_STATE(num, state)   LED_##num = ((state) != 0)

EventBits_t uxBits;
uint16_t relay_node_addr = 0;

union {
    uint8_t raw[2];

    struct {
        uint8_t channels_num;

        struct {
            uint8_t button1 : 1;
            uint8_t button2 : 1;
            uint8_t button3 : 1;
            uint8_t reserved : 5;
        };
    };
} button_data;

void led_button_init(void) {
    button_data.channels_num = 3;

    TRISAbits.TRISA7 = 0;
    TRISAbits.TRISA0 = 0;
    TRISAbits.TRISA1 = 0;

    LED_OFF(1);
    LED_OFF(2);
    LED_OFF(3);
    xEventGroupClearBits(LED_STATE_CHANGED_BIT);
}

void button1_handler(void) {
    LED_TOGGLE(1);
    xEventGroupSetBits(LED_STATE_CHANGED_BIT);
}

void button2_handler(void) {
    if (!PORTBbits.RB4) {
        LED_TOGGLE(2);
        xEventGroupSetBits(LED_STATE_CHANGED_BIT);
    }
}

void button3_handler(void) {
    if (!PORTBbits.RB5) {
        LED_TOGGLE(3);
        xEventGroupSetBits(LED_STATE_CHANGED_BIT);
    }
}

void button_init(void) {
    OPTION_REGbits.INTEDG = 0; // Interrupt on falling edge of RB0/INT pin
    ioc_rb5_add_isr_handler(button3_handler);
    ioc_rb4_add_isr_handler(button2_handler);
    ex_int_add_isr_handler(button1_handler);
}

void znp_msg_handler(uint16_t cmd, uint8_t* data, uint8_t len) {
    switch (cmd) {
        case AF_INCOMING_MSG:
        {
            // data6 = data7 = endpoint
            // data17 = command option

            // if data17 == 0
            // data18 = led control bits
            // else if data17 == 1
            // data18:data19 = node relay address

            uint8_t endpoint = data[6];

            if (endpoint == ENDPOINT_COMMAND) {
                if (data[17] == 0) {
                    if (data[18] & BIT5) LED_SET_STATE(1, data[18] & BIT0);
                    if (data[18] & BIT6) LED_SET_STATE(2, data[18] & BIT1);
                    if (data[18] & BIT7) LED_SET_STATE(3, data[18] & BIT2);
                } else if (data[17] == 1) {
                    relay_node_addr = BUILD_UINT16(data[18], data[19]);
                    xEventGroupSetBits(NEW_RELAY_NODE_ADDR);
                }
            } else if (endpoint == ENDPOINT_TELEMETRY) {
                LED_SET_STATE(1, data[18] & BIT0);
                LED_SET_STATE(2, data[18] & BIT1);
                LED_SET_STATE(3, data[18] & BIT2);
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
        button_data.button1 = LED_STATE(1);
        button_data.button2 = LED_STATE(2);
        button_data.button3 = LED_STATE(3);
        data = button_data.raw;
        len = sizeof (button_data.raw);
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

    button_data.raw[1] |= 0xE0; // bit mask is always set to 1

    uint8_t relay_node_lsb_addr
            = (uint8_t) eeprom_read(RELAY_NODE_LSB_ADDR_EEPROM_ADDR);
    uint8_t relay_node_msb_addr
            = (uint8_t) eeprom_read(RELAY_NODE_MSB_ADDR_EEPROM_ADDR);
    relay_node_addr
            = BUILD_UINT16(relay_node_lsb_addr, relay_node_msb_addr);

    sys_tick_init();
    xEventGroupCreate();
    __delay_ms(100);

    led_ind_init();
    led_button_init();
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

        if (uxBits & LED_STATE_CHANGED_BIT) {
            if (relay_node_addr != 0)
                znp_send_msg(relay_node_addr, ENDPOINT_TELEMETRY, true);
            xEventGroupClearBits(LED_STATE_CHANGED_BIT);
        }

        if (uxBits & NEW_RELAY_NODE_ADDR) {
            eeprom_write(RELAY_NODE_LSB_ADDR_EEPROM_ADDR, LSB(relay_node_addr));
            eeprom_write(RELAY_NODE_MSB_ADDR_EEPROM_ADDR, MSB(relay_node_addr));
            xEventGroupClearBits(NEW_RELAY_NODE_ADDR);
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
