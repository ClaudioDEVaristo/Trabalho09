#ifndef BUZZER_H
#define BUZZER_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define BUZZER_PIN_0 10
#define BUZZER_PIN 21

void buzzer_setup_pwm(uint pin, uint freq_hz);
void buzzer_play(uint pin, uint times, uint freq_hz, uint duration_ms);

#endif