/**
 * @file    color.h
 * @brief   Color and brightness scaling (pure functions, SRS LMP-003).
 *
 * duty = round(C x B x 1024 / (255 x 100)) with integer round half up, limited to DUTY_FULL_SCALE_COUNTS.
 * Brightness values above 100 are treated as 100 (the validator rejects them before they get here).
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

#include "lamp_types.h"

/** @brief Duty in counts for one channel value (0 to 255) at a brightness in percent (0 to 100). */
uint16_t Color_ScaleChannel(uint8_t channel, uint8_t brightnessPercent);

/** @brief Duty for all three channels. */
Duty_t Color_ToDuty(Rgb_t color, uint8_t brightnessPercent);

/**
 * @brief  Scale a duty by a factor in permille (1000 = unchanged), rounding half up.
 * Used by the breathing waveform. Result never exceeds DUTY_FULL_SCALE_COUNTS.
 */
Duty_t Color_ApplyScale(Duty_t duty, uint16_t scalePermille);

#endif /* COLOR_H */
