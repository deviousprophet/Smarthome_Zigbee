/* 
 * File:   main.h
 * Author: deviousprophet
 *
 * Created on March 2, 2022, 1:40 PM
 */

#ifndef MAIN_H
#define	MAIN_H

#ifdef	__cplusplus
extern "C" {
#endif

    // PIC16F628A Configuration Bit Settings

    // 'C' source line config statements

    // CONFIG
#pragma config FOSC = INTOSCIO  // Oscillator Selection bits (INTOSC oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = ON       // RA5/MCLR/VPP Pin Function Select bit (RA5/MCLR/VPP pin function is MCLR)
#pragma config BOREN = OFF      // Brown-out Detect Enable bit (BOD disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable bit (RB4/PGM pin has digital I/O function, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EE Memory Code Protection bit (Data memory code protection off)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

    // #pragma config statements should precede project file includes.
    // Use project enums instead of #define for ON and OFF.

#include <xc.h>
#define _XTAL_FREQ 4000000

#define SOFT_RESET()    asm("ljmp 0x0000h")

#define BIT0     (1 << 0)
#define BIT1     (1 << 1)
#define BIT2     (1 << 2)
#define BIT3     (1 << 3)
#define BIT4     (1 << 4)
#define BIT5     (1 << 5)
#define BIT6     (1 << 6)
#define BIT7     (1 << 7)

#ifndef SETMAXMIN
#define SETMAXMIN(data, max, min)                                           \
    ((data > max) ? max : ((data < min) ? min : data))
#endif

#ifndef BCD_2_U8
#define BCD_2_U8(bcd)                                                       \
    ((uint8_t) ((bcd > 0x99) ? 255 : (bcd >> 4) * 10 + (bcd & 0x0F)))
#endif

#ifndef U8_2_BCD
#define U8_2_BCD(integer)                                                   \
    ((uint8_t) ((integer > 99) ? 255 : ((integer / 10) << 4) + (integer % 10)))
#endif

#ifndef MSB
#define MSB(a) (((a) >> 8) & 0xFF)
#endif

#ifndef LSB
#define LSB(a) ((a) & 0xFF)
#endif

#ifndef BUILD_UINT16
#define BUILD_UINT16(loByte, hiByte) \
	((uint16_t)(((loByte) & 0x00FF) + (((hiByte) & 0x00FF) << 8)))
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef	__cplusplus
}
#endif

#endif	/* MAIN_H */

