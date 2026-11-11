/**
 * @file    lamp_types.h
 * @brief   Small value types shared by application and driver code.
 *
 * Types only. This header contains no behaviour, so every layer may include it.
 */
#ifndef LAMP_TYPES_H
#define LAMP_TYPES_H

#include <stdint.h>

/** Duty value that means "constantly on". Compare values run from 0 to this value (SRS 3.4, LMP-002). */
#define DUTY_FULL_SCALE_COUNTS   (1024u)

/** Color in 8-bit channel values, as carried on the bus. */
typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Rgb_t;

/** PWM duty in counts, 0 to DUTY_FULL_SCALE_COUNTS. */
typedef struct
{
    uint16_t red;
    uint16_t green;
    uint16_t blue;
} Duty_t;

#endif /* LAMP_TYPES_H */
