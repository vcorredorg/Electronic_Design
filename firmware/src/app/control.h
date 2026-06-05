#ifndef CONTROL_H
#define CONTROL_H

#include <stdbool.h>
#include <stdint.h>

int control_init(void);
void control_start(void);
void control_command_set_frequency(float hz);
void control_command_set_gain(uint32_t gain);
void control_command_set_time_constant(float tau_s);
void control_command_set_output_scale(float scale);
void control_command_set_first_harmonic(uint32_t first_harmonic);
void control_command_toggle_filter(void);
void control_command_toggle_reference(void);
void control_command_recalculate_external(void);

#endif
