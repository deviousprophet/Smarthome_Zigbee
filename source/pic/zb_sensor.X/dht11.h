/* 
 * File:   dht11.h
 * Author: deviousprophet
 *
 * Created on March 18, 2022, 11:31 AM
 */

#ifndef DHT11_H
#define	DHT11_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

    typedef enum {
        DHT_OK,
        DHT_TIMEOUT,
        DHT_ERROR,
    } dht11_status_t;

    typedef union {
        uint8_t raw[4];

        struct {
            uint8_t RH1;
            uint8_t RH2;
            uint8_t T1;
            uint8_t T2;
        };
    } dht11_data_t;

    void dht11_init(void);
    dht11_status_t dht11_read(dht11_data_t* pData);

#ifdef	__cplusplus
}
#endif

#endif	/* DHT11_H */

