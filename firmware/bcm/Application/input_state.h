/**
 * @file    input_state.h
 * @brief   Simulated vehicle input state and the mapping of button presses (SRS BCM-002, BCM-005).
 *
 * Door: level. Enable, mode, brightness, color, unlock: each press advances or toggles its value.
 * Sequences: mode 0,1 (wrap). Brightness 25, 50, 75, 100 (wrap). Color white, red, green, blue, amber (wrap).
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef INPUT_STATE_H
#define INPUT_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "lamp_types.h"

typedef struct
{
    bool    doorOpen;
    bool    lampEnable;
    uint8_t lightingMode;
    uint8_t colorIndex;
    uint8_t brightnessIndex;
    bool    unlocked;
} InputState_t;

/** @brief Set the BCM-005 defaults, with the door state read from the switch. */
void InputState_Init(InputState_t *state, bool doorOpen);

/** @brief Set the door level. Returns true if it changed. */
bool InputState_SetDoor(InputState_t *state, bool open);

void InputState_PressEnable(InputState_t *state);
void InputState_PressMode(InputState_t *state);
void InputState_PressBrightness(InputState_t *state);
void InputState_PressColor(InputState_t *state);
void InputState_PressUnlock(InputState_t *state);

uint8_t InputState_GetBrightnessPercent(const InputState_t *state);
Rgb_t   InputState_GetColor(const InputState_t *state);

#endif /* INPUT_STATE_H */
