#include "zephyr_compat.h"
#include "lockin_core.h"

#include <string.h>

/*
 * Zephyr 2.7.1 in PlatformIO/Teensy can fail during the pre-link stage when
 * libm is pulled in. Keep this firmware self-contained: no sinf/cosf/sqrtf/
 * atan2f/fabsf symbols are required from libm. Accuracy is sufficient for a
 * first hardware bring-up; later this can be replaced by a LUT or CMSIS-DSP.
 */
static float lk_absf(float x)
{
    return (x < 0.0f) ? -x : x;
}

static float lk_wrap_pi(float x)
{
    const float two_pi = 2.0f * LOCKIN_PI;
    while (x > LOCKIN_PI) {
        x -= two_pi;
    }
    while (x < -LOCKIN_PI) {
        x += two_pi;
    }
    return x;
}

static float lk_sinf(float x)
{
    x = lk_wrap_pi(x);
    float x2 = x * x;
    /* 7th-order odd polynomial around 0 after wrapping to [-pi, pi]. */
    return x * (1.0f - (x2 / 6.0f) + ((x2 * x2) / 120.0f) - ((x2 * x2 * x2) / 5040.0f));
}

static float lk_cosf(float x)
{
    return lk_sinf(x + (0.5f * LOCKIN_PI));
}

static float lk_sqrtf(float x)
{
    if (x <= 0.0f) {
        return 0.0f;
    }
    float g = (x >= 1.0f) ? x : 1.0f;
    for (int i = 0; i < 8; ++i) {
        g = 0.5f * (g + (x / g));
    }
    return g;
}

static float lk_atan2f(float y, float x)
{
    if (x == 0.0f) {
        if (y > 0.0f) { return 0.5f * LOCKIN_PI; }
        if (y < 0.0f) { return -0.5f * LOCKIN_PI; }
        return 0.0f;
    }

    float abs_y = lk_absf(y) + 1.0e-12f;
    float angle;

    if (x >= 0.0f) {
        float r = (x - abs_y) / (x + abs_y);
        angle = (0.25f * LOCKIN_PI) - (0.25f * LOCKIN_PI * r);
    } else {
        float r = (x + abs_y) / (abs_y - x);
        angle = (0.75f * LOCKIN_PI) - (0.25f * LOCKIN_PI * r);
    }

    return (y < 0.0f) ? -angle : angle;
}


static lockin_config_t cfg;

static float x_in_f1[LOCKIN_MAX_HARMONICS];
static float x_in_f2[LOCKIN_MAX_HARMONICS];
static float y_qu_f1[LOCKIN_MAX_HARMONICS];
static float y_qu_f2[LOCKIN_MAX_HARMONICS];
static float x_mean[LOCKIN_MAX_HARMONICS];
static float y_mean[LOCKIN_MAX_HARMONICS];
static float x_var[LOCKIN_MAX_HARMONICS];
static float y_var[LOCKIN_MAX_HARMONICS];
static uint16_t sync_buffer[LOCKIN_SYNC_BUFFER_LEN];

static uint32_t sample_index;
static uint32_t time_to_sample;
static uint32_t newest_index;
static uint32_t oldest_index;
static uint32_t last_sample_us;
static uint32_t last_loop_us;
static float clip_value;
static float unlock_value;
static float alpha;
static float alpha_min;

static void recalc_internal_timing(void)
{
    if (cfg.signal_hz < 0.1f) {
        cfg.signal_hz = 0.1f;
    }
    if (cfg.sample_hz < 1.0f) {
        cfg.sample_hz = 1.0f;
    }

    cfg.samples_per_period = (uint32_t)(cfg.sample_hz / cfg.signal_hz);
    if (cfg.samples_per_period < 1u) {
        cfg.samples_per_period = 1u;
    }

    cfg.sample_dt_us = (uint32_t)(1000000.0f / cfg.sample_hz);
    if (cfg.sample_dt_us < 1u) {
        cfg.sample_dt_us = 1u;
    }

    cfg.sample_hz_true = 1000000.0f / (float)cfg.sample_dt_us;
    cfg.signal_hz_true = cfg.sample_hz_true / (float)cfg.samples_per_period;

    alpha = 1.0f / (cfg.sample_hz_true * cfg.time_constant_s);
    if (alpha < 0.0000001f) {
        alpha = 0.0000001f;
    }
    if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    alpha_min = 1.0f - alpha;
}

static void reset_filters_unlocked(void)
{
    memset(x_in_f1, 0, sizeof(x_in_f1));
    memset(x_in_f2, 0, sizeof(x_in_f2));
    memset(y_qu_f1, 0, sizeof(y_qu_f1));
    memset(y_qu_f2, 0, sizeof(y_qu_f2));
    memset(x_mean, 0, sizeof(x_mean));
    memset(y_mean, 0, sizeof(y_mean));
    memset(x_var, 0, sizeof(x_var));
    memset(y_var, 0, sizeof(y_var));
    sample_index = 0u;
    time_to_sample = 0u;
    newest_index = 0u;
    oldest_index = 1u;
    last_sample_us = 0u;
    last_loop_us = 0u;
    clip_value = 0.0f;
    unlock_value = 0.0f;
}

void lockin_core_init(void)
{
    cfg.signal_hz = LOCKIN_DEFAULT_SIGNAL_HZ;
    cfg.sample_hz = LOCKIN_DEFAULT_SAMPLE_HZ;
    cfg.time_constant_s = LOCKIN_DEFAULT_TAU_S;
    cfg.phase_offset_rad = 0.0f;
    cfg.output_scale = LOCKIN_DEFAULT_OUTPUT_SCALE;
    cfg.calibration_factor = LOCKIN_INPUT_CAL;
    cfg.gain = LOCKIN_DEFAULT_GAIN;
    cfg.first_harmonic = 2u;
    cfg.last_harmonic = cfg.first_harmonic + LOCKIN_HARMONIC_COUNT - 1u;
    cfg.undersampling = 1u;
    cfg.oversampling = 1u;
    cfg.sync_filter_enabled = false;
    cfg.external_reference_enabled = false;
    recalc_internal_timing();
    reset_filters_unlocked();
    memset(sync_buffer, 0, sizeof(sync_buffer));
}

void lockin_core_reset_state(void)
{
    unsigned int key = irq_lock();
    reset_filters_unlocked();
    irq_unlock(key);
}

void lockin_core_clear_sync_buffer(void)
{
    unsigned int key = irq_lock();
    memset(sync_buffer, 0, sizeof(sync_buffer));
    newest_index = 0u;
    oldest_index = 1u;
    irq_unlock(key);
}

void lockin_core_get_config(lockin_config_t *out)
{
    unsigned int key = irq_lock();
    *out = cfg;
    irq_unlock(key);
}

void lockin_core_set_config(const lockin_config_t *new_cfg)
{
    unsigned int key = irq_lock();
    cfg = *new_cfg;
    recalc_internal_timing();
    reset_filters_unlocked();
    irq_unlock(key);
}

void lockin_core_set_gain(uint32_t gain)
{
    if (!(gain == 0u || gain == 1u || gain == 2u || gain == 4u || gain == 8u ||
          gain == 16u || gain == 32u || gain == 64u)) {
        return;
    }

    unsigned int key = irq_lock();
    cfg.gain = gain;
    cfg.calibration_factor = (gain == 0u) ? LOCKIN_INPUT_CAL : (LOCKIN_INPUT_CAL / (float)gain);
    irq_unlock(key);
}

void lockin_core_set_time_constant(float tau_s)
{
    if (tau_s <= 0.0001f) {
        tau_s = 0.0001f;
    }
    unsigned int key = irq_lock();
    cfg.time_constant_s = tau_s;
    recalc_internal_timing();
    irq_unlock(key);
}

void lockin_core_set_output_scale(float scale)
{
    if (scale < 0.0f) {
        scale = 0.0f;
    }
    unsigned int key = irq_lock();
    cfg.output_scale = scale;
    irq_unlock(key);
}

void lockin_core_set_first_harmonic(uint32_t first_harmonic)
{
    if (first_harmonic < 2u) {
        first_harmonic = 2u;
    }
    if ((first_harmonic + LOCKIN_HARMONIC_COUNT - 1u) > LOCKIN_MAX_HARMONICS) {
        first_harmonic = LOCKIN_MAX_HARMONICS - LOCKIN_HARMONIC_COUNT + 1u;
    }
    unsigned int key = irq_lock();
    cfg.first_harmonic = first_harmonic;
    cfg.last_harmonic = first_harmonic + LOCKIN_HARMONIC_COUNT - 1u;
    reset_filters_unlocked();
    irq_unlock(key);
}

void lockin_core_set_frequency(float signal_hz)
{
    if (signal_hz <= 0.1f) {
        signal_hz = 0.1f;
    }
    unsigned int key = irq_lock();
    cfg.signal_hz = signal_hz;
    cfg.external_reference_enabled = false;
    cfg.sample_hz = cfg.sync_filter_enabled ? LOCKIN_SYNC_SAMPLE_HZ : LOCKIN_DEFAULT_SAMPLE_HZ;
    cfg.undersampling = 1u;
    cfg.oversampling = 1u;
    recalc_internal_timing();
    reset_filters_unlocked();
    irq_unlock(key);
}

void lockin_core_set_filter(bool sync_enabled)
{
    unsigned int key = irq_lock();
    cfg.sync_filter_enabled = sync_enabled;
    if (!cfg.external_reference_enabled) {
        cfg.sample_hz = sync_enabled ? LOCKIN_SYNC_SAMPLE_HZ : LOCKIN_DEFAULT_SAMPLE_HZ;
        recalc_internal_timing();
    }
    reset_filters_unlocked();
    memset(sync_buffer, 0, sizeof(sync_buffer));
    irq_unlock(key);
}

void lockin_core_set_reference_mode(bool external_enabled)
{
    unsigned int key = irq_lock();
    cfg.external_reference_enabled = external_enabled;
    if (!external_enabled) {
        cfg.sample_hz = cfg.sync_filter_enabled ? LOCKIN_SYNC_SAMPLE_HZ : LOCKIN_DEFAULT_SAMPLE_HZ;
        cfg.undersampling = 1u;
        cfg.oversampling = 1u;
        recalc_internal_timing();
    }
    reset_filters_unlocked();
    irq_unlock(key);
}

void lockin_core_set_external_timing(float trigger_hz, uint32_t undersampling, uint32_t oversampling, uint32_t samples_per_period)
{
    if (trigger_hz < 1.0f) {
        trigger_hz = 1.0f;
    }
    if (undersampling < 1u) {
        undersampling = 1u;
    }
    if (oversampling < 1u) {
        oversampling = 1u;
    }
    if (samples_per_period < 1u) {
        samples_per_period = 1u;
    }

    unsigned int key = irq_lock();
    cfg.external_reference_enabled = true;
    cfg.sample_hz = trigger_hz;
    cfg.signal_hz = trigger_hz / (float)LOCKIN_REFERENCE_MULTIPLIER;
    cfg.undersampling = undersampling;
    cfg.oversampling = oversampling;
    cfg.samples_per_period = samples_per_period;
    cfg.sample_hz_true = ((float)oversampling * trigger_hz) / (float)undersampling;
    cfg.signal_hz_true = cfg.signal_hz;
    cfg.sample_dt_us = (uint32_t)(1000000.0f / cfg.sample_hz_true);
    if (cfg.sample_dt_us < 1u) {
        cfg.sample_dt_us = 1u;
    }
    alpha = 1.0f / (cfg.sample_hz_true * cfg.time_constant_s);
    if (alpha < 0.0000001f) {
        alpha = 0.0000001f;
    }
    if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    alpha_min = 1.0f - alpha;
    reset_filters_unlocked();
    irq_unlock(key);
}

static bool harmonic_is_enabled(uint32_t harmonic)
{
    return harmonic == 1u || (harmonic >= cfg.first_harmonic && harmonic <= cfg.last_harmonic);
}

void lockin_core_process_sample(uint16_t adc_counts, uint32_t now_us)
{
    unsigned int key = irq_lock();

    time_to_sample++;
    if (time_to_sample < cfg.undersampling) {
        irq_unlock(key);
        return;
    }
    time_to_sample = 0u;

    float x = (float)adc_counts;
    float clip = (x < LOCKIN_ADC_CLIP_LOW || x > LOCKIN_ADC_CLIP_HIGH) ? 1.0f : 0.0f;
    clip_value = clip_value * 0.9995f + clip * 0.0005f;

    if (last_sample_us != 0u) {
        uint32_t loop_us = now_us - last_sample_us;
        if (last_loop_us != 0u) {
            float ratio = (float)loop_us / (float)last_loop_us;
            float unlocked = (ratio < 0.95f || ratio > 1.05f) ? 1.0f : 0.0f;
            unlock_value = unlock_value * 0.9995f + unlocked * 0.0005f;
        }
        last_loop_us = loop_us;
    }
    last_sample_us = now_us;

    uint32_t max_h = cfg.last_harmonic;
    if (max_h > LOCKIN_MAX_HARMONICS) {
        max_h = LOCKIN_MAX_HARMONICS;
    }

    if (!cfg.sync_filter_enabled) {
        for (uint32_t h = 1u; h <= max_h; h++) {
            if (!harmonic_is_enabled(h)) {
                continue;
            }
            uint32_t idx = h - 1u;
            float theta = ((2.0f * (float)h * LOCKIN_PI * (float)sample_index) /
                           (float)cfg.samples_per_period) - ((float)h * cfg.phase_offset_rad);
            float phas = -lk_sinf(theta);
            float quad = -lk_cosf(theta);
            float xi0 = x * phas;
            float yq0 = x * quad;

            float delta = xi0 - x_in_f1[idx];
            x_in_f1[idx] += alpha * delta;
            delta = x_in_f1[idx] - x_in_f2[idx];
            x_in_f2[idx] += alpha * delta;

            delta = yq0 - y_qu_f1[idx];
            y_qu_f1[idx] += alpha * delta;
            delta = y_qu_f1[idx] - y_qu_f2[idx];
            y_qu_f2[idx] += alpha * delta;
        }

        float delta_y = y_qu_f2[0] - y_mean[0];
        float incr_y = alpha * delta_y;
        y_mean[0] += incr_y;
        y_var[0] = alpha_min * (y_var[0] + delta_y * incr_y);

        float delta_x = x_in_f2[0] - x_mean[0];
        float incr_x = alpha * delta_x;
        x_mean[0] += incr_x;
        x_var[0] = alpha_min * (x_var[0] + delta_x * incr_x);
    } else {
        uint16_t oldest = sync_buffer[oldest_index];

        for (uint32_t h = 1u; h <= max_h; h++) {
            if (!harmonic_is_enabled(h)) {
                continue;
            }
            uint32_t idx = h - 1u;
            float theta = ((2.0f * (float)h * LOCKIN_PI * (float)sample_index) /
                           (float)cfg.samples_per_period) - ((float)h * cfg.phase_offset_rad);
            float phas = -lk_sinf(theta);
            float quad = -lk_cosf(theta);

            float xi0 = x * phas;
            float xio0 = (float)oldest * phas;
            x_in_f1[idx] += (xi0 - xio0) / (float)cfg.samples_per_period;
            float delta = x_in_f1[idx] - x_in_f2[idx];
            x_in_f2[idx] += alpha * delta;

            float yq0 = x * quad;
            float yqo0 = (float)oldest * quad;
            y_qu_f1[idx] += (yq0 - yqo0) / (float)cfg.samples_per_period;
            delta = y_qu_f1[idx] - y_qu_f2[idx];
            y_qu_f2[idx] += alpha * delta;
        }

        newest_index++;
        if (newest_index >= cfg.samples_per_period || newest_index >= LOCKIN_SYNC_BUFFER_LEN) {
            newest_index = 0u;
        }
        oldest_index = newest_index + 1u;
        if (oldest_index >= cfg.samples_per_period || oldest_index >= LOCKIN_SYNC_BUFFER_LEN) {
            oldest_index = 0u;
        }
        sync_buffer[newest_index] = adc_counts;
    }

    sample_index++;
    if (sample_index >= cfg.samples_per_period) {
        sample_index = 0u;
    }

    irq_unlock(key);
}

void lockin_core_snapshot(lockin_snapshot_t *out)
{
    unsigned int key = irq_lock();
    memset(out, 0, sizeof(*out));
    out->config = cfg;
    out->first_harmonic = cfg.first_harmonic;
    out->x1 = x_in_f2[0] * cfg.calibration_factor;
    out->y1 = y_qu_f2[0] * cfg.calibration_factor;
    for (uint32_t n = 0; n < LOCKIN_HARMONIC_COUNT; n++) {
        uint32_t idx = cfg.first_harmonic - 1u + n;
        if (idx < LOCKIN_MAX_HARMONICS) {
            out->harmonic_x[n] = x_in_f2[idx] * cfg.calibration_factor;
            out->harmonic_y[n] = y_qu_f2[idx] * cfg.calibration_factor;
        }
    }
    float local_clip = clip_value;
    float local_unlock = unlock_value;
    float grab_var = lk_sqrtf(y_var[0] * y_var[0] + x_var[0] * x_var[0]);
    irq_unlock(key);

    out->r = lk_sqrtf(out->x1 * out->x1 + out->y1 * out->y1);
    out->phase_rad = lk_atan2f(out->y1, out->x1);
    out->noise = lk_sqrtf(lk_absf(grab_var)) * out->config.calibration_factor;
    out->clipping = local_clip > 0.05f;
    out->reference_unlocked = local_unlock > 0.2f;
    out->error = (out->clipping ? 1 : 0) | (out->reference_unlocked ? 2 : 0);
}

bool lockin_core_reference_high(void)
{
    unsigned int key = irq_lock();
    bool high = sample_index < (cfg.samples_per_period / 2u);
    irq_unlock(key);
    return high;
}

uint16_t lockin_core_pwm_code_from_snapshot(const lockin_snapshot_t *s, uint16_t max_code)
{
    float value = s->config.output_scale * s->r * LOCKIN_OUTPUT_SCALE_CAL;
    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > (float)max_code) {
        value = (float)max_code;
    }
    return (uint16_t)value;
}
