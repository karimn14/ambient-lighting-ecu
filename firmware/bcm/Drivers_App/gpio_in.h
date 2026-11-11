/**
 * @file    gpio_in.h
 * @brief   Interface of the BCM digital input driver (door switch and five buttons).
 *
 * Returns the logical state: true means pressed, closed switch or door open as defined below. The driver
 * hides the electrical polarity (active low with internal pull-up on the Blue Pill).
 *
 * Layer: Drivers (interface header, no HAL types).
 */
#ifndef GPIO_IN_H
#define GPIO_IN_H

#include <stdbool.h>

#include "status.h"

typedef enum
{
    GPIO_IN_DOOR = 0,    /**< Switch closed means door open. */
    GPIO_IN_ENABLE,
    GPIO_IN_MODE,
    GPIO_IN_BRIGHTNESS,
    GPIO_IN_COLOR,
    GPIO_IN_UNLOCK,
    GPIO_IN_COUNT
} GpioInput_t;

Status_t GpioIn_Init(void);

/** @brief Raw logical state of one input, not debounced. */
bool GpioIn_Read(GpioInput_t input);

#endif /* GPIO_IN_H */
