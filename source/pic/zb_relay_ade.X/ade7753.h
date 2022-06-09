/* 
 * File:   ade7753.h
 * Author: deviousprophet
 *
 * Created on March 2, 2022, 4:28 PM
 */

#ifndef ADE7753_H
#define	ADE7753_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"

//#define ADE_CALIBRATION

#ifndef ADE_CALIBRATION
#define AMPS_LSB_CONST              (float) (1 / 52405.96386)
#define AMPS_LSB_OFFSET             (float) (-0.006677591)
#define VOLTS_LSB_CONST_1           (float) (1 / 2911.207692)
#define VOLTS_LSB_OFFSET_1          (float) (-0.733192211)
#define VOLTS_LSB_CONST_2           (float) (1 / 2645.024689)
#define VOLTS_LSB_OFFSET_2          (float) (-20.25007624)
#define ACTIVE_POWER_CONST          (float) (0.859830416)
#define ACTIVE_POWER_OFFSET         (float) (0.257707374)
#else // ADE_CALIBRATION
#define AMPS_LSB_CONST              (float) (1.0)
#define AMPS_LSB_OFFSET             (float) (0)
#define VOLTS_LSB_CONST_1           (float) (1.0)
#define VOLTS_LSB_OFFSET_1          (float) (0)
#define VOLTS_LSB_CONST_2           (float) (1.0)
#define VOLTS_LSB_OFFSET_2          (float) (0)
#define ACTIVE_POWER_CONST          (float) (1.0)
#define ACTIVE_POWER_OFFSET         (float) (0)
#endif // ADE_CALIBRATION

#define MAX_VRMS_INPUT_SAMPLE   0x2518
#define MAX_IRMS_INPUT_SAMPLE   0x2851EC

#define ADE_CLKIN_FREQ              3579545 // 3.579545 MHz

    typedef struct {
        float irms;
        float vrms;
        float active_power;
    } ade_power_meter_data_t;

    void ade_get_power_meter_data(ade_power_meter_data_t* pData);

    /*
     * ADE REGISTERS
     */
#define WAVEFORM   	0x01
#define AENERGY     0x02
#define RAENERGY    0x03
#define LAENERGY    0x04
#define VAENERGY    0x05
#define RVAENERGY   0x06
#define LVAENERGY   0x07
#define LVARENERGY  0x08

#define MODE        0x09
#define IRQEN       0x0A
#define STATUS_ADE  0x0B
#define RSTSTATUS   0x0C

#define CH1OS       0x0D
#define CH2OS       0x0E
#define GAIN        0x0F
#define PHCAL       0x10

#define APOS        0x11
#define WGAIN       0x12
#define WDIV        0x13
#define CFNUM       0x14
#define CFDEN       0x15
#define IRMS        0x16
#define VRMS        0x17
#define IRMSOS      0x18
#define VRMSOS      0x19
#define VAGAIN      0x1A
#define VADIV       0x1B
#define LINECYC     0x1C
#define ZXTOUT      0x1D
#define SAGCYC      0x1E
#define SAGLVL      0x1F
#define IPKLVL      0x20
#define VPKLVL      0x21
#define IPEAK       0x22
#define RSTIPEAK    0x23
#define VPEAK       0x24
#define RSTVPEAK    0x25
#define TEMP        0x26
#define PERIOD      0x27

#define TMODE       0x3D
#define CHKSUM      0x3E
#define DIEREV      0x3F

    // MODE Register

    typedef union {
        uint16_t reg_val;

        struct {
            uint16_t DISHPF : 1;
            uint16_t DISLPF2 : 1;
            uint16_t DISCF : 1;
            uint16_t DISSAG : 1;
            uint16_t ASUSPEND : 1;
            uint16_t TEMPSEL : 1;
            uint16_t SWRST : 1;
            uint16_t CYCMODE : 1;
            uint16_t DISCH1 : 1;
            uint16_t DISCH2 : 1;
            uint16_t SWAP : 1;
            uint16_t DTRT0 : 1;
            uint16_t DTRT1 : 1;
            uint16_t WAVSEL0 : 1;
            uint16_t WAVSEL1 : 1;
            uint16_t POAM : 1;
        };
    } ade_MODE_reg_t;

    // IRQ Register

    typedef union {
        uint16_t reg_val;

        struct {
            uint16_t AEHF : 1;
            uint16_t SAG : 1;
            uint16_t CYCEND : 1;
            uint16_t WSMP : 1;
            uint16_t ZX : 1;
            uint16_t TEMP_IRQ : 1;
            uint16_t RESET : 1;
            uint16_t AEOF : 1;
            uint16_t PKV : 1;
            uint16_t PKI : 1;
            uint16_t VAEHF : 1;
            uint16_t VAEOF : 1;
            uint16_t ZXTO : 1;
            uint16_t PPOS : 1;
            uint16_t PNEG : 1;
        };
    } ade_IRQ_reg_t;

#define GAIN_1          0
#define GAIN_2          BIT0
#define GAIN_4          BIT1
#define GAIN_8          BIT2
#define GAIN_16         BIT3

#define FULL_SCALE_05   0
#define FULL_SCALE_025  BIT0
#define FULL_SCALE_0125 BIT1

    typedef union {
        uint8_t reg_val;

        struct {
            uint8_t magnitude : 5;
            uint8_t sign : 1;
            uint8_t not_used : 1;
            uint8_t integrator : 1;
        };
    } ade_CHxOS_reg_t;

    typedef union {
        uint8_t reg_val;

        struct {
            uint8_t PGA1 : 3;
            uint8_t Full_Scale : 2;
            uint8_t PGA2 : 3;
        };
    } ade_GAIN_reg_t;

#define DEFAULT_LINECYC 150  // 150 line half-cycles

#define ADE_DEFAULT_CONFIG()    {           \
        .gain.reg_val = 0x00,               \
        .mode.reg_val = 0x000C,             \
        .irq_en.reg_val = 0x0040,           \
        .linecyc = DEFAULT_LINECYC,         \
        .ch1os.reg_val = 0x00,              \
        .ch2os.reg_val = 0x00,              \
        .irmsos = 0x000,                    \
        .vrmsos = 0x000,                    \
        .is_reversed = false,               \
        .zx_cb = NULL,                      \
    }

    typedef void (*ade_zx_cb_t)(void);

    typedef struct {
        ade_GAIN_reg_t gain;
        ade_MODE_reg_t mode;
        ade_IRQ_reg_t irq_en;
        uint16_t linecyc;
        ade_CHxOS_reg_t ch1os;
        ade_CHxOS_reg_t ch2os;
        uint16_t irmsos;
        uint16_t vrmsos;
        bool is_reversed;
        ade_zx_cb_t zx_cb;
    } ade_config_t;

    void ade_init(void);
    void ade_config(ade_config_t* config);
    void ade_reset(void);
    void ade_write(uint8_t addr, uint32_t write_buf, uint8_t bytes_to_write);
    uint32_t ade_read(uint8_t addr, uint8_t bytes_to_read);

    void ade_set_GAIN(ade_GAIN_reg_t gain_config);
    void ade_set_LINECYC(uint16_t linecyc);

    uint24_t ade_get_IRMS(void);
    uint24_t ade_get_VRMS(void);
    int ade_get_LAENERGY(void);
    uint16_t ade_get_PERIOD(void);

#ifdef	__cplusplus
}
#endif

#endif	/* ADE7753_H */

