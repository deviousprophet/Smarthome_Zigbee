#include "usart.h"

void usart_init(void) {
    TRISBbits.TRISB1 = 1; // RX
    TRISBbits.TRISB2 = 1; // TX

    TXSTAbits.BRGH = 1;
    TXSTAbits.SYNC = 0;
    TXSTAbits.TXEN = 1;
    SPBRG = 12; // 19200

    RCSTAbits.SPEN = 1;
    RCSTAbits.CREN = 1;
}

void usart_send(uint8_t c) {
    while (!TXSTAbits.TRMT);
    TXREG = c;
}