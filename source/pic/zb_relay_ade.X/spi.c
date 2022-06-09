#include "spi.h"

void spi_master_init(void) {
//    OPTION_REGbits.nRBPU = 0; // PORTB pull-ups are enabled by individual PORT latch values

    TRISCbits.TRISC3 = 0; // SLCK
    TRISCbits.TRISC4 = 1; // MISO
    TRISCbits.TRISC5 = 0; // MOSI
    TRISDbits.TRISD3 = 0; // NSS
    PORTDbits.RD3 = 1;

    SSPSTATbits.SMP = 0; // Input data sampled at middle of data output time
    SSPSTATbits.CKE = 0; // Data transmitted on falling edge of SCK

    SSPCONbits.SSPEN = 1; //  Enables serial port
    SSPCONbits.CKP = 0; // Idle state for clock is a low level
    SSPCONbits.SSPM = 0b0000; // = SPI Master mode, clock = FOSC/4
}

uint8_t spi_transfer(uint8_t data) {
    SSPBUF = data;
    while (!SSPSTATbits.BF);
    return SSPBUF;
}
