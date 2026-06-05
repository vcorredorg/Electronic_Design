#ifndef PWM_BACKEND_H
#define PWM_BACKEND_H

#include <stdint.h>

int pwm_backend_init(void);
int pwm_backend_write_code(uint16_t code, uint16_t max_code);

#endif
