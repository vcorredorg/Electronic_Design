cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lockin_teensy40_zephyr)

target_include_directories(app PRIVATE
    include
    src
    src/app
    src/dsp
    src/hal
)

target_sources(app PRIVATE
    src/main.c
    src/app/control.c
    src/app/serial_protocol.c
    src/dsp/lockin_core.c
    src/hal/adc_backend.c
    src/hal/gpio_backend.c
    src/hal/pwm_backend.c
    src/hal/uart_backend.c
)
