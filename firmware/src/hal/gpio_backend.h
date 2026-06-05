#ifndef GPIO_BACKEND_H
#define GPIO_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

int gpio_backend_init(void);
int gpio_backend_set_reference(bool high);
int gpio_backend_set_gain_bits(uint32_t gain);
int gpio_backend_enable_external_irq(bool both_edges);
int gpio_backend_disable_external_irq(void);
void gpio_backend_set_external_sample_callback(void (*callback)(void));
uint32_t gpio_backend_get_and_clear_edge_count(void);

#endif
