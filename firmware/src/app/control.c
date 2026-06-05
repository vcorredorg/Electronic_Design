#include "zephyr_compat.h"
#include "control.h"
#include "serial_protocol.h"
#include "lockin_core.h"
#include "adc_backend.h"
#include "gpio_backend.h"
#include "pwm_backend.h"
#include "uart_backend.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SAMPLE_STACK_SIZE   4096
#define REPORT_STACK_SIZE   4096
#define SERIAL_STACK_SIZE   2048
#define CONTROL_STACK_SIZE  2048

#define SAMPLE_PRIORITY     0
#define CONTROL_PRIORITY    2
#define SERIAL_PRIORITY     3
#define REPORT_PRIORITY     4

K_THREAD_STACK_DEFINE(sample_stack, SAMPLE_STACK_SIZE);
K_THREAD_STACK_DEFINE(report_stack, REPORT_STACK_SIZE);
K_THREAD_STACK_DEFINE(serial_stack, SERIAL_STACK_SIZE);
K_THREAD_STACK_DEFINE(control_stack, CONTROL_STACK_SIZE);

static struct k_thread sample_thread;
static struct k_thread report_thread;
static struct k_thread serial_thread;
static struct k_thread control_thread;

static struct k_sem sample_sem;
static struct k_sem report_sem;
static struct k_sem control_sem;

static struct k_timer sample_timer;
static struct k_timer report_timer;
static struct k_timer external_gate_timer;

static atomic_t external_measure_done;
static atomic_t external_measure_active;
static atomic_t started;

static void start_internal_sampling_from_config(void)
{
    lockin_config_t cfg;
    lockin_core_get_config(&cfg);
    uint32_t dt = cfg.sample_dt_us;
    if (dt < 1u) {
        dt = 1u;
    }
    k_timer_start(&sample_timer, K_USEC(dt), K_USEC(dt));
}

static void sample_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    k_sem_give(&sample_sem);
}

static void report_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    k_sem_give(&report_sem);
}

static void external_gate_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    atomic_set(&external_measure_done, 1);
    atomic_set(&external_measure_active, 0);
    k_sem_give(&control_sem);
}

static void external_sample_callback(void)
{
    if (!atomic_get(&external_measure_active)) {
        k_sem_give(&sample_sem);
    }
}

static void sample_entry(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    while (1) {
        k_sem_take(&sample_sem, K_FOREVER);
        uint16_t sample = 0;
        if (adc_backend_read(&sample) == 0) {
            lockin_core_process_sample(sample, lockin_now_us32());

            gpio_backend_set_reference(lockin_core_reference_high());
        }
    }
}

static void report_entry(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    char line[384];
    while (1) {
        k_sem_take(&report_sem, K_FOREVER);
        lockin_snapshot_t snapshot;
        lockin_core_snapshot(&snapshot);
        int n = serial_protocol_format_report(&snapshot, line, sizeof(line));
        if (n > 0) {
            uart_backend_write(line, (size_t)n);
        }
        uint16_t code = lockin_core_pwm_code_from_snapshot(&snapshot, 1023u);
        pwm_backend_write_code(code, 1023u);
    }
}

static void serial_entry(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    char line[80];
    size_t pos = 0;

    while (1) {
        uint8_t ch = 0;
        if (uart_backend_get_char(&ch) != 0) {
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            if (pos > 0u) {
                line[pos] = '\0';
                serial_protocol_handle_line(line);
                pos = 0u;
            }
            continue;
        }

        if (pos < (sizeof(line) - 1u)) {
            line[pos++] = (char)ch;
        } else {
            pos = 0u;
        }
    }
}

static void apply_external_measurement_result(void)
{
    uint32_t edges = gpio_backend_get_and_clear_edge_count();
    float trigger_hz = (float)edges / 10.0f;

    uint32_t undersampling = 1u;
    uint32_t oversampling = 1u;
    uint32_t samples_per_period = LOCKIN_REFERENCE_MULTIPLIER;
    bool both_edges = false;

    if (trigger_hz <= 100000.0f) {
        undersampling = 1u;
        oversampling = 2u;
        samples_per_period = LOCKIN_REFERENCE_MULTIPLIER * 2u;
        both_edges = true;
    } else if (trigger_hz <= 200000.0f) {
        undersampling = 1u;
        oversampling = 1u;
        samples_per_period = LOCKIN_REFERENCE_MULTIPLIER;
    } else if (trigger_hz <= 400000.0f) {
        undersampling = 2u;
        oversampling = 1u;
        samples_per_period = LOCKIN_REFERENCE_MULTIPLIER / 2u;
    } else if (trigger_hz <= 800000.0f) {
        undersampling = 4u;
        oversampling = 1u;
        samples_per_period = LOCKIN_REFERENCE_MULTIPLIER / 4u;
    } else {
        undersampling = 8u;
        oversampling = 1u;
        samples_per_period = LOCKIN_REFERENCE_MULTIPLIER / 8u;
    }

    lockin_core_set_external_timing(trigger_hz, undersampling, oversampling, samples_per_period);
    gpio_backend_enable_external_irq(both_edges);
}

static void control_entry(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    while (1) {
        k_sem_take(&control_sem, K_FOREVER);
        if (atomic_cas(&external_measure_done, 1, 0)) {
            apply_external_measurement_result();
        }
    }
}

int control_init(void)
{
    k_sem_init(&sample_sem, 0, 1);
    k_sem_init(&report_sem, 0, 1);
    k_sem_init(&control_sem, 0, 1);

    k_timer_init(&sample_timer, sample_timer_handler, NULL);
    k_timer_init(&report_timer, report_timer_handler, NULL);
    k_timer_init(&external_gate_timer, external_gate_timer_handler, NULL);

    lockin_core_init();

    int ret = adc_backend_init();
    if (ret != 0) {
        printk("adc init failed: %d\n", ret);
    }

    ret = gpio_backend_init();
    if (ret != 0) {
        printk("gpio init failed: %d\n", ret);
    }

    gpio_backend_set_external_sample_callback(external_sample_callback);
    gpio_backend_set_gain_bits(LOCKIN_DEFAULT_GAIN);

    ret = pwm_backend_init();
    if (ret != 0) {
        printk("pwm init failed: %d\n", ret);
    }

    ret = uart_backend_init();
    if (ret != 0) {
        printk("uart init failed: %d\n", ret);
    }

    return 0;
}

void control_start(void)
{
    if (atomic_cas(&started, 0, 1)) {
        k_thread_create(&sample_thread, sample_stack, SAMPLE_STACK_SIZE,
                        sample_entry, NULL, NULL, NULL,
                        SAMPLE_PRIORITY, K_FP_REGS, K_NO_WAIT);
        k_thread_create(&control_thread, control_stack, CONTROL_STACK_SIZE,
                        control_entry, NULL, NULL, NULL,
                        CONTROL_PRIORITY, K_FP_REGS, K_NO_WAIT);
        k_thread_create(&serial_thread, serial_stack, SERIAL_STACK_SIZE,
                        serial_entry, NULL, NULL, NULL,
                        SERIAL_PRIORITY, K_FP_REGS, K_NO_WAIT);
        k_thread_create(&report_thread, report_stack, REPORT_STACK_SIZE,
                        report_entry, NULL, NULL, NULL,
                        REPORT_PRIORITY, K_FP_REGS, K_NO_WAIT);

        start_internal_sampling_from_config();
        k_timer_start(&report_timer, K_MSEC(LOCKIN_DEFAULT_OUTPUT_MS), K_MSEC(LOCKIN_DEFAULT_OUTPUT_MS));
    }
}

void control_command_set_frequency(float hz)
{
    gpio_backend_disable_external_irq();
    k_timer_stop(&sample_timer);
    lockin_core_set_frequency(hz);
    lockin_core_clear_sync_buffer();
    start_internal_sampling_from_config();
}

void control_command_set_gain(uint32_t gain)
{
    lockin_core_set_gain(gain);
    gpio_backend_set_gain_bits(gain);
}

void control_command_set_time_constant(float tau_s)
{
    lockin_core_set_time_constant(tau_s);
}

void control_command_set_output_scale(float scale)
{
    lockin_core_set_output_scale(scale);
}

void control_command_set_first_harmonic(uint32_t first_harmonic)
{
    lockin_core_set_first_harmonic(first_harmonic);
}

void control_command_toggle_filter(void)
{
    lockin_config_t cfg;
    lockin_core_get_config(&cfg);
    lockin_core_set_filter(!cfg.sync_filter_enabled);
    lockin_core_clear_sync_buffer();

    lockin_core_get_config(&cfg);
    if (!cfg.external_reference_enabled) {
        k_timer_stop(&sample_timer);
        start_internal_sampling_from_config();
    }
}

void control_command_toggle_reference(void)
{
    lockin_config_t cfg;
    lockin_core_get_config(&cfg);

    if (cfg.external_reference_enabled) {
        gpio_backend_disable_external_irq();
        k_timer_stop(&external_gate_timer);
        atomic_set(&external_measure_active, 0);
        lockin_core_set_reference_mode(false);
        k_timer_stop(&sample_timer);
        start_internal_sampling_from_config();
    } else {
        k_timer_stop(&sample_timer);
        lockin_core_set_reference_mode(true);
        control_command_recalculate_external();
    }
}

void control_command_recalculate_external(void)
{
    k_timer_stop(&sample_timer);
    gpio_backend_get_and_clear_edge_count();
    atomic_set(&external_measure_done, 0);
    atomic_set(&external_measure_active, 1);
    gpio_backend_enable_external_irq(true);
    k_timer_start(&external_gate_timer, K_SECONDS(5), K_NO_WAIT);
}
