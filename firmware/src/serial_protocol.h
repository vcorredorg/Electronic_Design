#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <stddef.h>
#include "dsp/lockin_core.h"

void serial_protocol_handle_line(const char *line);
int serial_protocol_format_report(const lockin_snapshot_t *s, char *out, size_t out_len);

#endif
