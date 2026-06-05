#ifndef UART_BACKEND_H
#define UART_BACKEND_H

#include <stddef.h>
#include <stdint.h>

int uart_backend_init(void);
int uart_backend_get_char(uint8_t *ch);
void uart_backend_write(const char *s, size_t len);

#endif
