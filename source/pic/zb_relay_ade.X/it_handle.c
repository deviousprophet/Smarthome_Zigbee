#include "it_handle.h"

usart_rx_isr_t _usart_rx_isr;
timer1_isr_t _tmr1_isr;
timer2_isr_t _tmr2_isr;
ex_int_isr_t _ex_int_isr;
ioc_rb1_isr_t _ioc_rb1_isr;
ioc_rb2_isr_t _ioc_rb2_isr;
ioc_rb3_isr_t _ioc_rb3_isr;
ioc_rb4_isr_t _ioc_rb4_isr;
ioc_rb5_isr_t _ioc_rb5_isr;
ioc_rb6_isr_t _ioc_rb6_isr;
ioc_rb7_isr_t _ioc_rb7_isr;

void usart_add_isr_handler(usart_rx_isr_t handler) {
    PIE1bits.RCIE = 1;
    OPTION_REGbits.T0CS = 0;
    OPTION_REGbits.PS = 0b011;
    OPTION_REGbits.PSA = 0;
    _usart_rx_isr = handler;
}

void timer1_add_isr_handler(timer1_isr_t handler) {
    PIR1bits.TMR1IF = 0;
    PIE1bits.TMR1IE = 1;
    _tmr1_isr = handler;
}

void timer2_add_isr_handler(timer2_isr_t handler) {
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 1;
    _tmr2_isr = handler;
}

void ex_int_add_isr_handler(ex_int_isr_t handler) {
    INTCONbits.INTF = 0;
    INTCONbits.INTE = 1;
    _ex_int_isr = handler;
}

void ioc_rb1_add_isr_handler(ioc_rb1_isr_t handler) {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    IOCBbits.IOCB1 = 1;
    TRISBbits.TRISB1 = 1;
    _ioc_rb1_isr = handler;
}

void ioc_rb2_add_isr_handler(ioc_rb2_isr_t handler) {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    IOCBbits.IOCB2 = 1;
    TRISBbits.TRISB2 = 1;
    _ioc_rb2_isr = handler;
}

void ioc_rb3_add_isr_handler(ioc_rb3_isr_t handler) {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    IOCBbits.IOCB3 = 1;
    TRISBbits.TRISB3 = 1;
    _ioc_rb3_isr = handler;
}

void ioc_rb4_add_isr_handler(ioc_rb4_isr_t handler) {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    IOCBbits.IOCB4 = 1;
    TRISBbits.TRISB4 = 1;
    _ioc_rb4_isr = handler;
}

void ioc_rb5_add_isr_handler(ioc_rb5_isr_t handler) {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    IOCBbits.IOCB5 = 1;
    TRISBbits.TRISB5 = 1;
    _ioc_rb5_isr = handler;
}

void ioc_rb6_add_isr_handler(ioc_rb6_isr_t handler) {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    IOCBbits.IOCB6 = 1;
    TRISBbits.TRISB6 = 1;
    _ioc_rb6_isr = handler;
}

void ioc_rb7_add_isr_handler(ioc_rb7_isr_t handler) {
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    IOCBbits.IOCB7 = 1;
    TRISBbits.TRISB7 = 1;
    _ioc_rb7_isr = handler;
}

__interrupt() void ISR(void) {
    if (PIR1bits.RCIF) {
        if (RCSTAbits.OERR) {
            RCSTAbits.CREN = 0;
            RCSTAbits.CREN = 1;
        }

        INTCONbits.T0IE = 1;
        INTCONbits.T0IF = 0;
#if (_XTAL_FREQ == 8000000)
        TMR0 = 175;
#elif (_XTAL_FREQ == 4000000)
        TMR0 = 215;
#endif
        uint8_t c = RCREG;
        if (_usart_rx_isr != NULL)
            _usart_rx_isr(USART_RX_DATA, &c);
    }

    if (INTCONbits.T0IF) {
        INTCONbits.T0IF = 0;
        INTCONbits.T0IE = 0;
        if (_usart_rx_isr != NULL)
            _usart_rx_isr(USART_RX_CMPLT, NULL);
    }

    if (PIR1bits.TMR1IF) {
        PIR1bits.TMR1IF = 0;
        if (_tmr1_isr != NULL)
            _tmr1_isr();
    }
    if (PIR1bits.TMR2IF) {
        PIR1bits.TMR2IF = 0;
        if (_tmr2_isr != NULL)
            _tmr2_isr();
    }

    if (INTCONbits.INTF) {
        if (_ex_int_isr != NULL)
            _ex_int_isr();
        INTCONbits.INTF = 0;
    }

    if (INTCONbits.RBIF) {
        static uint8_t tmpPORTB = 0xFF; // PORTB IOC
        uint8_t newPORTB = PORTB;
        uint8_t iocMask = (tmpPORTB ^ newPORTB) & IOCB & TRISB;

        if (iocMask & BIT1) {
            if (_ioc_rb1_isr != NULL)
                _ioc_rb1_isr();
        } else if (iocMask & BIT2) {
            if (_ioc_rb2_isr != NULL)
                _ioc_rb2_isr();
        } else if (iocMask & BIT3) {
            if (_ioc_rb3_isr != NULL)
                _ioc_rb3_isr();
        } else if (iocMask & BIT4) {
            if (_ioc_rb4_isr != NULL)
                _ioc_rb4_isr();
        } else if (iocMask & BIT5) {
            if (_ioc_rb5_isr != NULL)
                _ioc_rb5_isr();
        } else if (iocMask & BIT6) {
            if (_ioc_rb6_isr != NULL)
                _ioc_rb6_isr();
        } else if (iocMask & BIT7) {
            if (_ioc_rb7_isr != NULL)
                _ioc_rb7_isr();
        }
        tmpPORTB = newPORTB;
        INTCONbits.RBIF = 0;
    }
}
