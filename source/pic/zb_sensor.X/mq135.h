/* 
 * File:   mq135.h
 * Author: deviousprophet
 *
 * Created on March 18, 2022, 11:32 AM
 */

#ifndef MQ135_H
#define	MQ135_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

    void mq135_init(void);
    uint16_t mq135_read(void);

#ifdef	__cplusplus
}
#endif

#endif	/* MQ135_H */

