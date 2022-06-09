#include "mq135.h"
#include "sys_tick.h"

uint16_t DeltaSigA2D(void) {
    uint16_t counter = 0, result = 0;
    TRISAbits.TRISA3 = 0;
    CMCONbits.CM = 0b110;
    while (counter < 1024) {
        if (!PORTAbits.RA3) result++;
        counter++;
    }
    CMCONbits.CM = 0b011;
    TRISAbits.TRISA3 = 1;
    return result;
}

void mq135_init(void) {
    CMCONbits.CM = 0b011; // Two Common Reference Comparators
    TRISAbits.TRISA3 = 1;

    VRCONbits.VREN = 1; // VREF circuit powered on
    VRCONbits.VROE = 1; // VREF is output on RA2 pin
    VRCONbits.VRR = 1; // VREF Range Selection: Low range
    VRCONbits.VR = 0b1100; // VREF = (VR<3:0>/ 24) * VDD --> VDD/2
}

uint16_t mq135_read(void) {
    uint16_t data = DeltaSigA2D();
    return data;
}
