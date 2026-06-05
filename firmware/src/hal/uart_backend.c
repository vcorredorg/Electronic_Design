#include "zephyr_compat.h"
#include "uart_backend.h"
#include "lockin_hw_map.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

K_MSGQ_DEFINE(uart_rx_msgq, sizeof(uint8_t), 256, 4);

static const struct device *uart_dev;

static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t buffer[16];
            int len = uart_fifo_read(dev, buffer, sizeof(buffer));
            for (int i = 0; i < len; i++) {
                (void)k_msgq_put(&uart_rx_msgq, &buffer[i], K_NO_WAIT);
            }
        }
    }
}

int uart_backend_init(void)
{
    uart_dev = device_get_binding(LOCKIN_UART_DEV_NAME);
    if (uart_dev == NULL) {
        return -ENODEV;
    }

    uart_irq_callback_set(uart_dev, uart_isr);
    uart_irq_rx_enable(uart_dev);
    return 0;
}

int uart_backend_get_char(uint8_t *ch)
{
    if (ch == NULL) {
        return -EINVAL;
    }
    return k_msgq_get(&uart_rx_msgq, ch, K_FOREVER);
}

void uart_backend_write(const char *s, size_t len)
{
    if (s == NULL || uart_dev == NULL) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart_dev, (unsigned char)s[i]);
    }
}
