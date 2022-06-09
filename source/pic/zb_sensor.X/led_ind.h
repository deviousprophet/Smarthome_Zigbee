/* 
 * File:   led_ind.h
 * Author: deviousprophet
 *
 * Created on March 4, 2022, 2:31 PM
 */

#ifndef LED_IND_H
#define	LED_IND_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

    void led_ind_init(void);
    void led_ind_blink_start(void);
    void led_ind_blink_stop(void);
    void led_ind_on(void);
    void led_ind_off(void);
    void led_ind_toggle(void);

#ifdef	__cplusplus
}
#endif

#endif	/* LED_IND_H */

