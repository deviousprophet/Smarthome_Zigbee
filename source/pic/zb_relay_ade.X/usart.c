#include "usart.h"

void usart_init(void) {
    TRISCbits.TRISC6 = 0; // TX
    TRISCbits.TRISC7 = 1; // RX

    BAUDCTLbits.BRG16 = 0; // 8-bit Baud Rate Generator is used

    TXSTAbits.TX9 = 0;
    TXSTAbits.SYNC = 0;
    TXSTAbits.BRGH = 1;
    TXSTAbits.TXEN = 1;
    SPBRG = 25; // 19200

    RCSTAbits.SPEN = 1;
    RCSTAbits.RX9 = 0;
    RCSTAbits.CREN = 1;
}

void usart_send(uint8_t c) {
    while (!TXSTAbits.TRMT);
    TXREG = c;
}