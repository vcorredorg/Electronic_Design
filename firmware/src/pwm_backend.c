#include "zephyr_compat.h"
#include "pwm_backend.h"
#include "lockin_hw_map.h"

#include <errno.h>
#include <stdint.h>

static const struct device *pwm_dev;

int pwm_backend_init(void)
{
    pwm_dev = device_get_binding(LOCKIN_PWM_DEV_NAME);
    if (pwm_dev == NULL) {
        return -ENODEV;
    }

    return pwm_backend_write_code(0u, 1023u);
}

int pwm_backend_write_code(uint16_t code, uint16_t max_code)
{
    if (pwm_dev == NULL) {
        return -ENODEV;
    }
    if (max_code == 0u) {
        return -EINVAL;
    }
    if (code > max_code) {
        code = max_code;
    }

    uint32_t pulse_us = ((uint32_t)code * LOCKIN_PWM_MAX_PULSE_US) / (uint32_t)max_code;
    return pwm_pin_set_usec(pwm_dev, LOCKIN_PWM_CHANNEL, LOCKIN_PWM_PERIOD_US, pulse_us, 0);
}
