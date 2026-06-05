#ifndef LOCKIN_CORE_H
#define LOCKIN_CORE_H

#include <stdint.h>
#include <stdbool.h>

#include "lockin_config.h"

typedef struct {
    float signal_hz;
    float sample_hz;
    float sample_hz_true;
    float signal_hz_true;
    float time_constant_s;
    float phase_offset_rad;
    float output_scale;
    float calibration_factor;
    uint32_t samples_per_period;
    uint32_t sample_dt_us;
    uint32_t gain;
    uint32_t first_harmonic;
    uint32_t last_harmonic;
    uint32_t undersampling;
    uint32_t oversampling;
    bool sync_filter_enabled;
    bool external_reference_enabled;
} lockin_config_t;

typedef struct {
    int error;
    bool clipping;
    bool reference_unlocked;
    float r;
    float phase_rad;
    float noise;
    float x1;
    float y1;
    float harmonic_x[LOCKIN_HARMONIC_COUNT];
    float harmonic_y[LOCKIN_HARMONIC_COUNT];
    uint32_t first_harmonic;
    lockin_config_t config;
} lockin_snapshot_t;

void lockin_core_init(void);
void lockin_core_reset_state(void);
void lockin_core_clear_sync_buffer(void);
void lockin_core_get_config(lockin_config_t *out);
void lockin_core_set_config(const lockin_config_t *cfg);
void lockin_core_set_gain(uint32_t gain);
void lockin_core_set_time_constant(float tau_s);
void lockin_core_set_output_scale(float scale);
void lockin_core_set_first_harmonic(uint32_t first_harmonic);
void lockin_core_set_frequency(float signal_hz);
void lockin_core_set_filter(bool sync_enabled);
void lockin_core_set_reference_mode(bool external_enabled);
void lockin_core_set_external_timing(float trigger_hz, uint32_t undersampling, uint32_t oversampling, uint32_t samples_per_period);
void lockin_core_process_sample(uint16_t adc_counts, uint32_t now_us);
void lockin_core_snapshot(lockin_snapshot_t *out);
bool lockin_core_reference_high(void);
uint16_t lockin_core_pwm_code_from_snapshot(const lockin_snapshot_t *s, uint16_t max_code);

#endif
