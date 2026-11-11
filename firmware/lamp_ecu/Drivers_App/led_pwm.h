/**
 * @file    led_pwm.h
 * @brief   Interface of the RGB PWM driver (SRS LMP-002, SYS-003). Implemented in Drivers_App with TIM3.
 *
 * Responsibilities: write the three compare values and enforce a safe start. It knows nothing about colors,
 * modes or fades. The polarity of the output stage is handled here, not in the application. With the
 * baseline circuit (common anode, low-side NPN) PWM high turns the LED on.
 *
 * Layer: Drivers (interface header, no HAL types).
 */
#ifndef LED_PWM_H
#define LED_PWM_H

#include "lamp_types.h"
#include "status.h"

/** @brief Configure the timer with all compare values 0. Outputs are not enabled yet. */
Status_t LedPwm_Init(void);

/** @brief Enable the PWM outputs. Compare values must already be 0 (SYS-003). */
Status_t LedPwm_Start(void);

/** @brief Write the duty. Each channel is limited to DUTY_FULL_SCALE_COUNTS. */
void LedPwm_SetDuty(Duty_t duty);

Duty_t LedPwm_GetDuty(void);

#endif /* LED_PWM_H */
