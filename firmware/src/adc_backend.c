#include "zephyr_compat.h"
#include "adc_backend.h"
#include "lockin_hw_map.h"

#include <errno.h>
#include <stdint.h>

static const struct device *adc_dev;
static int16_t adc_raw;

static const struct adc_channel_cfg adc_channel_cfg_data = {
    .gain = ADC_GAIN_1,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = LOCKIN_ADC_CHANNEL,
};

int adc_backend_init(void)
{
    adc_dev = device_get_binding(LOCKIN_ADC_DEV_NAME);
    if (adc_dev == NULL) {
        return -ENODEV;
    }

    int ret = adc_channel_setup(adc_dev, &adc_channel_cfg_data);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

int adc_backend_read(uint16_t *sample)
{
    if (sample == NULL) {
        return -EINVAL;
    }

    if (adc_dev == NULL) {
        *sample = 2048u;
        return -ENODEV;
    }

    const struct adc_sequence sequence = {
        .channels = BIT(LOCKIN_ADC_CHANNEL),
        .buffer = &adc_raw,
        .buffer_size = sizeof(adc_raw),
        .resolution = 12,
    };

    int ret = adc_read(adc_dev, &sequence);
    if (ret != 0) {
        return ret;
    }

    if (adc_raw < 0) {
        adc_raw = 0;
    }
    if (adc_raw > 4095) {
        adc_raw = 4095;
    }

    *sample = (uint16_t)adc_raw;
    return 0;
}
