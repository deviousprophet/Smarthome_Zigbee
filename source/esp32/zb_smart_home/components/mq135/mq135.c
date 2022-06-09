#include "mq135.h"

#include <math.h>

float get_correction_factor(float t, float h) {
    return CORA * t * t - CORB * t + CORC - (h - 33.) * CORD;
}

float get_resistance(uint16_t adc_val) {
    return ((1023. / (float)adc_val) * 5. - 1.) * RLOAD;
}

float get_corrected_resistance(uint16_t adc_val, float t, float h) {
    return get_resistance(adc_val) / get_correction_factor(t, h);
}

float mq135_calc_ppm(uint16_t adc_val) {
    return PARA * pow((get_resistance(adc_val) / RZERO), -PARB);
}

float mq135_calc_corrected_ppm(uint16_t adc_val, float t, float h) {
    return PARA * pow((get_corrected_resistance(adc_val, t, h) / RZERO), -PARB);
}

float mq135_get_rzero(uint16_t adc_val) {
    return get_resistance(adc_val) * pow((ATMOCO2 / PARA), (1. / PARB));
}

float mq135_get_corrected_rzero(uint16_t adc_val, float t, float h) {
    return get_corrected_resistance(adc_val, t, h) *
           pow((ATMOCO2 / PARA), (1. / PARB));
}
