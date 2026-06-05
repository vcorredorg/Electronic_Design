#include "zephyr_compat.h"
#include "gpio_backend.h"
#include "lockin_hw_map.h"

#include <errno.h>
#include <stdint.h>

static const struct device *gpio_dev;
static struct gpio_callback ext_ref_callback;
static volatile uint32_t edge_count;
static void (*external_sample_callback)(void);

static void ext_ref_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    edge_count++;
    if (external_sample_callback != NULL) {
        external_sample_callback();
    }
}

int gpio_backend_init(void)
{
    gpio_dev = device_get_binding(LOCKIN_GPIO_DEV_NAME);
    if (gpio_dev == NULL) {
        return -ENODEV;
    }

    int ret = gpio_pin_configure(gpio_dev, LOCKIN_PIN_EXT_REF, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret != 0) {
        return ret;
    }

    ret = gpio_pin_configure(gpio_dev, LOCKIN_PIN_REF_OUT, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        return ret;
    }

    ret = gpio_pin_configure(gpio_dev, LOCKIN_PIN_GAIN0, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        return ret;
    }
    ret = gpio_pin_configure(gpio_dev, LOCKIN_PIN_GAIN1, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        return ret;
    }
    ret = gpio_pin_configure(gpio_dev, LOCKIN_PIN_GAIN2, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        return ret;
    }

    gpio_init_callback(&ext_ref_callback, ext_ref_isr, BIT(LOCKIN_PIN_EXT_REF));
    gpio_add_callback(gpio_dev, &ext_ref_callback);
    return 0;
}

int gpio_backend_set_reference(bool high)
{
    if (gpio_dev == NULL) {
        return -ENODEV;
    }
    return gpio_pin_set(gpio_dev, LOCKIN_PIN_REF_OUT, high ? 1 : 0);
}

int gpio_backend_set_gain_bits(uint32_t gain)
{
    if (gpio_dev == NULL) {
        return -ENODEV;
    }

    uint32_t code = 0u;
    switch (gain) {
    case 0u: code = 0u; break;
    case 1u: code = 1u; break;
    case 2u: code = 2u; break;
    case 4u: code = 3u; break;
    case 8u: code = 4u; break;
    case 16u: code = 5u; break;
    case 32u: code = 6u; break;
    case 64u: code = 7u; break;
    default: return -EINVAL;
    }

    gpio_pin_set(gpio_dev, LOCKIN_PIN_GAIN0, (code & 0x1u) ? 1 : 0);
    gpio_pin_set(gpio_dev, LOCKIN_PIN_GAIN1, (code & 0x2u) ? 1 : 0);
    gpio_pin_set(gpio_dev, LOCKIN_PIN_GAIN2, (code & 0x4u) ? 1 : 0);
    return 0;
}

int gpio_backend_enable_external_irq(bool both_edges)
{
    if (gpio_dev == NULL) {
        return -ENODEV;
    }

    gpio_flags_t flags = both_edges ? GPIO_INT_EDGE_BOTH : GPIO_INT_EDGE_RISING;
    return gpio_pin_interrupt_configure(gpio_dev, LOCKIN_PIN_EXT_REF, flags);
}

int gpio_backend_disable_external_irq(void)
{
    if (gpio_dev == NULL) {
        return -ENODEV;
    }
    return gpio_pin_interrupt_configure(gpio_dev, LOCKIN_PIN_EXT_REF, GPIO_INT_DISABLE);
}

void gpio_backend_set_external_sample_callback(void (*callback)(void))
{
    external_sample_callback = callback;
}

uint32_t gpio_backend_get_and_clear_edge_count(void)
{
    unsigned int key = irq_lock();
    uint32_t count = edge_count;
    edge_count = 0u;
    irq_unlock(key);
    return count;
}
