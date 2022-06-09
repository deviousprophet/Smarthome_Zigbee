/* 
 * File:   usart.h
 * Author: deviousprophet
 *
 * Created on March 3, 2022, 1:39 PM
 */

#ifndef USART_H
#define	USART_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

    void usart_init(void);
    void usart_send(uint8_t c);

#ifdef	__cplusplus
}
#endif

#endif	/* USART_H */

