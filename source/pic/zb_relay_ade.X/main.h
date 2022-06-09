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

    // PIC16F887 Configuration Bit Settings
    // 'C' source line config statements
    // CONFIG1
#pragma config FOSC = INTRC_NOCLKOUT// Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled and can be enabled by SWDTEN bit of the WDTCON register)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = ON       // RE3/MCLR pin function select bit (RE3/MCLR pin function is MCLR)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = OFF      // Brown Out Reset Selection bits (BOR disabled)
#pragma config IESO = ON        // Internal External Switchover bit (Internal/External Switchover mode is enabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is disabled)
#pragma config LVP = OFF        // Low Voltage Programming Enable bit (RB3 pin has digital I/O, HV on MCLR must be used for programming)

    // CONFIG2
#pragma config BOR4V = BOR40V   // Brown-out Reset Selection bit (Brown-out Reset set to 4.0V)
#pragma config WRT = OFF        // Flash Program Memory Self Write Enable bits (Write protection off)

    // #pragma config statements should precede project file includes.
    // Use project enums instead of #define for ON and OFF.

#include <xc.h>
#define _XTAL_FREQ      8000000
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

