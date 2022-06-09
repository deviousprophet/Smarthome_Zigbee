#include "dht11.h"
#include "it_handle.h"

#define DHT_PIN_DIR	TRISAbits.TRISA6
#define DHT_PIN_DATA	PORTAbits.RA6

bool dht_timeout = false;

void dht11_timeout_handle(void) {
    dht_timeout = true;
}

void dht11_start_signal(void) {
    DHT_PIN_DIR = 0;
    DHT_PIN_DATA = 0;
    __delay_ms(20);
    DHT_PIN_DATA = 1;
    __delay_us(20);
    DHT_PIN_DIR = 1;
}

dht11_status_t dht11_check_response(void) {
    dht_timeout = false;
    TMR2 = 0;
    T2CONbits.TMR2ON = 1;
    while (!DHT_PIN_DATA && !dht_timeout);
    if (dht_timeout) return DHT_TIMEOUT;
    else {
	TMR2 = 0;
	while (DHT_PIN_DATA && !dht_timeout);
	if (dht_timeout) return DHT_TIMEOUT;
	else {
	    T2CONbits.TMR2ON = 0;
	    return DHT_OK;
	}
    }
}

void dht11_init(void) {
    T2CONbits.TOUTPS = 0; // 1:1 Post-scale Value
    T2CONbits.TMR2ON = 0; // Timer2 is off
    T2CONbits.T2CKPS = 0b00; // 1:1 Pre-scaler Value
    TMR2 = 0;
    timer2_add_isr_handler(dht11_timeout_handle);
}

uint8_t dht11_read_byte(void) {
    uint8_t result = 0;
    DHT_PIN_DIR = 1;
    for (uint8_t i = 0; i < 8; i++) {
	while (!DHT_PIN_DATA);
	TMR2 = 0;
	T2CONbits.TMR2ON = 1;
	while (DHT_PIN_DATA);
	if (TMR2 > 40) result |= 1 << (7 - i);
    }
    return result;
}

dht11_status_t dht11_read(dht11_data_t* pData) {
    dht11_start_signal();
    dht11_status_t dht_status = dht11_check_response();
    if (dht_status != DHT_OK) return dht_status;
    for (uint8_t i = 0; i < 4; i++)
	pData->raw[i] = dht11_read_byte();
    uint8_t checksum = dht11_read_byte();
    if (checksum != ((pData->raw[0] + pData->raw[1] + pData->raw[2] + pData->raw[3]) & 0xFF))
	return DHT_ERROR;
    return DHT_OK;
}

