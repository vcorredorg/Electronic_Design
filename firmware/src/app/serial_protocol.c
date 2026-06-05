#include "serial_protocol.h"
#include "control.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static bool parse_uint_text(const char *s, uint32_t *out)
{
    uint32_t v = 0u;
    bool any = false;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        any = true;
        v = (v * 10u) + (uint32_t)(*s - '0');
        s++;
    }
    if (!any) {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_float_text(const char *s, float *out)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }

    float sign = 1.0f;
    if (*s == '-') {
        sign = -1.0f;
        s++;
    } else if (*s == '+') {
        s++;
    }

    float value = 0.0f;
    bool any = false;
    while (*s >= '0' && *s <= '9') {
        any = true;
        value = value * 10.0f + (float)(*s - '0');
        s++;
    }

    if (*s == '.') {
        s++;
        float scale = 0.1f;
        while (*s >= '0' && *s <= '9') {
            any = true;
            value += (float)(*s - '0') * scale;
            scale *= 0.1f;
            s++;
        }
    }

    if (!any) {
        return false;
    }

    if (*s == 'e' || *s == 'E') {
        s++;
        float exp_sign = 1.0f;
        if (*s == '-') {
            exp_sign = -1.0f;
            s++;
        } else if (*s == '+') {
            s++;
        }
        uint32_t exp_value = 0u;
        bool exp_any = false;
        while (*s >= '0' && *s <= '9') {
            exp_any = true;
            exp_value = exp_value * 10u + (uint32_t)(*s - '0');
            s++;
        }
        if (exp_any) {
            float factor = 1.0f;
            for (uint32_t i = 0; i < exp_value && i < 38u; i++) {
                factor *= 10.0f;
            }
            value = (exp_sign > 0.0f) ? (value * factor) : (value / factor);
        }
    }

    *out = sign * value;
    return true;
}

void serial_protocol_handle_line(const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return;
    }

    switch (line[0]) {
    case 't':
    case 'T':
        control_command_toggle_filter();
        break;
    case 'r':
    case 'R':
        control_command_toggle_reference();
        break;
    case 'c':
    case 'C':
        control_command_recalculate_external();
        break;
    case 'g':
    case 'G': {
        uint32_t gain = 0u;
        if (parse_uint_text(line + 1, &gain)) {
            control_command_set_gain(gain);
        }
        break;
    }
    case 'e':
    case 'E': {
        float tau = 0.0f;
        if (parse_float_text(line + 1, &tau)) {
            control_command_set_time_constant(tau);
        }
        break;
    }
    case 's':
    case 'S': {
        float scale = 0.0f;
        if (parse_float_text(line + 1, &scale)) {
            control_command_set_output_scale(scale);
        }
        break;
    }
    case 'h':
    case 'H': {
        uint32_t first_harmonic = 0u;
        if (parse_uint_text(line + 1, &first_harmonic)) {
            control_command_set_first_harmonic(first_harmonic);
        }
        break;
    }
    default: {
        float hz = 0.0f;
        if (parse_float_text(line, &hz)) {
            control_command_set_frequency(hz);
        }
        break;
    }
    }
}

int serial_protocol_format_report(const lockin_snapshot_t *s, char *out, size_t out_len)
{
    if (s == NULL || out == NULL || out_len == 0u) {
        return -1;
    }

    const lockin_config_t *c = &s->config;
    float undersampling_field = c->external_reference_enabled ? (float)c->undersampling : 0.0f;
    if (c->external_reference_enabled && c->oversampling == 2u) {
        undersampling_field = 0.5f;
    }

    return snprintf(out, out_len,
                    "%d %.6f %u %u %u %u %.3f %.3f %.6f %.3f "
                    "%.5f %.5f %.5f %.5f %.5f "
                    "%.5f %.5f %.5f %.5f %.5f %.5f %u\r\n",
                    s->error,
                    (double)c->output_scale,
                    c->gain,
                    c->sync_filter_enabled ? 1u : 0u,
                    c->external_reference_enabled ? 1u : 0u,
                    c->samples_per_period,
                    (double)c->sample_hz_true,
                    (double)c->signal_hz_true,
                    (double)c->time_constant_s,
                    (double)undersampling_field,
                    (double)s->r,
                    (double)s->phase_rad,
                    (double)s->noise,
                    (double)s->x1,
                    (double)s->y1,
                    (double)s->harmonic_x[0],
                    (double)s->harmonic_x[1],
                    (double)s->harmonic_x[2],
                    (double)s->harmonic_y[0],
                    (double)s->harmonic_y[1],
                    (double)s->harmonic_y[2],
                    s->first_harmonic);
}
