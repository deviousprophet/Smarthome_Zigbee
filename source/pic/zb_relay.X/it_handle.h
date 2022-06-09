/* 
 * File:   it_handle.h
 * Author: deviousprophet
 *
 * Created on March 2, 2022, 4:55 PM
 */

#ifndef IT_HANDLE_H
#define	IT_HANDLE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

    typedef enum {
        USART_RX_DATA,
        USART_RX_CMPLT,
    } usart_event_t;

    typedef void (*usart_rx_isr_t)(usart_event_t rx_event, void* event_data);
    typedef void (*timer1_isr_t)(void);
    typedef void (*timer2_isr_t)(void);
    typedef void (*ex_int_isr_t)(void);
    typedef void (*ioc_rb4_isr_t)(void);
    typedef void (*ioc_rb5_isr_t)(void);

    void usart_add_isr_handler(usart_rx_isr_t handler);
    void timer1_add_isr_handler(timer1_isr_t handler);
    void timer2_add_isr_handler(timer2_isr_t handler);
    void ex_int_add_isr_handler(ex_int_isr_t handler);
    void ioc_rb4_add_isr_handler(ioc_rb4_isr_t handler);
    void ioc_rb5_add_isr_handler(ioc_rb5_isr_t handler);


#ifdef	__cplusplus
}
#endif

#endif	/* IT_HANDLE_H */

