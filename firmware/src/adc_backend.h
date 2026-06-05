#ifndef ADC_BACKEND_H
#define ADC_BACKEND_H

#include <stdint.h>

int adc_backend_init(void);
int adc_backend_read(uint16_t *sample);

#endif
