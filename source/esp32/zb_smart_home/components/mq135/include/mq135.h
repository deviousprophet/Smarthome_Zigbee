#pragma once

#include <stdint.h>

// The load resistance on the board
#define RLOAD   10.0
// Calibration resistance at atmospheric CO2 level
#define RZERO   76.63
// Parameters for calculating ppm of CO2 from sensor resistance
#define PARA    116.6020682
#define PARB    2.769034857

// Parameters to model temperature and humidity dependence
#define CORA    0.00035
#define CORB    0.02718
#define CORC    1.39538
#define CORD    0.0018

// Atmospheric CO2 level for calibration purposes
#define ATMOCO2 397.13

float mq135_calc_ppm(uint16_t adc_val);
float mq135_calc_corrected_ppm(uint16_t adc_val, float t, float h);

// For CO2 calibration
float mq135_get_rzero(uint16_t adc_val);
float mq135_get_corrected_rzero(uint16_t adc_val, float t, float h);