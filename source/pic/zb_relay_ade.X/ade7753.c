#include "ade7753.h"
#include "it_handle.h"

static ade_zx_cb_t _ade_zx_cb = NULL;
static uint16_t _linecyc = 0;
static float _energy = 0;
static bool _is_reversed = false;

#define ADE_RST_TRIS    TRISDbits.TRISD0
#define ADE_RST_PORT    PORTDbits.RD0

#define ADE_NSS_TRIS    TRISDbits.TRISD3
#define ADE_NSS_PORT    PORTDbits.RD3

int ade_signed_value(uint32_t value, uint8_t bits) {
    if (value & (1 << (bits - 1))) {
        for (uint8_t i = 0; i < bits - 2; i++)
            value ^= (1 << i);
        value++;
        return -((int) value);
    }
    return (int) value;
}

void ade_zx_handler(void) {
    if (_ade_zx_cb != NULL) _ade_zx_cb();
}

void ade_init(void) {
    ADE_NSS_TRIS = 0; // NSS pin
    ADE_NSS_PORT = 1; // Default HIGH
    ADE_RST_TRIS = 0; // RST pin
    ADE_RST_PORT = 0; // Hold in Reset
    spi_master_init();
    ade_reset();
}

void ade_config(ade_config_t* config) {
    if (config == NULL) return;
    _is_reversed = config->is_reversed;
    ade_set_GAIN(config->gain);

    if (_ade_zx_cb == NULL && config->zx_cb != NULL) {
        /* External interrupt for IRQ pin */
        OPTION_REGbits.INTEDG = 0;
        ex_int_add_isr_handler(ade_zx_handler);
        _ade_zx_cb = config->zx_cb;
    }

    ade_set_LINECYC(config->linecyc);
    ade_write(CH1OS, config->ch1os.reg_val, 1);
    ade_write(CH2OS, config->ch2os.reg_val, 1);
    ade_write(IRMSOS, config->irmsos, 2);
    ade_write(VRMSOS, config->vrmsos, 2);
    ade_write(MODE, config->mode.reg_val, 2);
    ade_write(IRQEN, config->irq_en.reg_val, 2);
    ade_read(RSTSTATUS, 2);
}

void ade_reset(void) {
    ADE_RST_PORT = 0;
    __delay_ms(1);
    ADE_RST_PORT = 1;
    __delay_ms(1);
}

void ade_write(uint8_t addr, uint32_t write_buf, uint8_t bytes_to_write) {
    uint8_t data = 0;
    addr |= 0x80;
    ADE_NSS_PORT = 0;
    spi_transfer(addr);
    for (uint8_t i = 0; i < bytes_to_write; i++) {
        data = (uint8_t) (write_buf >> 8 * (bytes_to_write - i - 1));
        spi_transfer(data);
    }
    ADE_NSS_PORT = 1;
}

uint32_t ade_read(uint8_t addr, uint8_t bytes_to_read) {
    uint32_t data = 0;
    ADE_NSS_PORT = 0;
    spi_transfer(addr);
    for (uint8_t i = 0; i < bytes_to_read; i++) {
        data <<= 8;
        data |= spi_transfer(0xFF);
    }
    ADE_NSS_PORT = 1;
    return data;
}

void ade_set_GAIN(ade_GAIN_reg_t gain_config) {
    ade_write(GAIN, gain_config.reg_val, 1);
}

void ade_set_LINECYC(uint16_t linecyc) {
    _linecyc = linecyc;
    ade_write(LINECYC, linecyc, 2);
}

uint24_t ade_get_IRMS(void) {
    return (uint24_t) ade_read(IRMS, 3);
}

uint24_t ade_get_VRMS(void) {
    return (uint24_t) ade_read(VRMS, 3);
}

int ade_get_LAENERGY(void) {
    return ade_signed_value(ade_read(LAENERGY, 3), 24);
}

uint16_t ade_get_PERIOD(void) {
    return (uint16_t) ade_read(PERIOD, 2);
}

void ade_get_power_meter_data(ade_power_meter_data_t* pData) {
    if (pData == NULL) return;
    uint24_t irms = ade_get_IRMS();
    uint24_t vrms = ade_get_VRMS();
    uint16_t period = ade_get_PERIOD();
    int laenergy = ade_get_LAENERGY();

    if (laenergy == 0) irms = 0;

    pData->irms =
            irms * AMPS_LSB_CONST + ((laenergy == 0) ? 0 : AMPS_LSB_OFFSET);

    if (vrms < 564590)
        pData->vrms = vrms * VOLTS_LSB_CONST_1 + VOLTS_LSB_OFFSET_1;
    else
        pData->vrms = vrms * VOLTS_LSB_CONST_2 + VOLTS_LSB_OFFSET_2;

    if (pData->irms < 0) pData->irms = 0;
    if (pData->vrms < 0) pData->vrms = 0;

    float line_periods = ((float) period) * 8 / ADE_CLKIN_FREQ;
    float accumulation_times = line_periods * _linecyc / 2;
    pData->active_power =
            ((_is_reversed ? -1 : 1) * laenergy / accumulation_times)
            * ACTIVE_POWER_CONST + ((laenergy == 0) ? 0 : ACTIVE_POWER_OFFSET);
}
