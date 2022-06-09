#include "led_ind.h"
#include "sys_tick.h"

#define LED_IND_TRIS	TRISBbits.TRISB4
#define LED_IND_PORT	PORTBbits.RB4

sys_tick_handle_t led_blink_handle;

void _blink_handler(void) {
    led_ind_toggle();
}

void led_ind_init(void) {
    LED_IND_TRIS = 0;
    led_blink_handle = sys_tick_register_callback_handler(_blink_handler);
    led_ind_on();
}

void led_ind_blink_start(void) {
    sys_tick_start_periodic(led_blink_handle, 3);
}

void led_ind_blink_stop(void) {
    sys_tick_stop(led_blink_handle);
    led_ind_off();
}

void led_ind_on(void) {
    LED_IND_PORT = 1;
}

void led_ind_off(void) {
    LED_IND_PORT = 0;
}

void led_ind_toggle(void) {
    LED_IND_PORT ^= 1;
}