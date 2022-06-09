/* 
 * File:   spi.h
 * Author: tripr
 *
 * Created on March 2, 2022, 4:01 PM
 */

#ifndef SPI_H
#define	SPI_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"

    typedef enum {
        SPI_MODE_0,
        SPI_MODE_1,
        SPI_MODE_2,
        SPI_MODE_3
    } spi_mode_t;

    void spi_master_init(void);
    uint8_t spi_transfer(uint8_t data);


#ifdef	__cplusplus
}
#endif

#endif	/* SPI_H */

