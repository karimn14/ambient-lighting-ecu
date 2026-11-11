/**
 * @file    fade_engine.h
 * @brief   Time-based linear fades and the breathing waveform (SRS LMP-005, LMP-008).
 *
 * The fade interpolates each channel from the value it has NOW to the new target over durationMs, so a new
 * target during a fade causes no jump (retarget continuity). A duration of 0 reaches the target at the
 * next update. All time arguments are in ms and wrap-safe.
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef FADE_ENGINE_H
#define FADE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "lamp_types.h"
#include "status.h"

typedef struct
{
    Duty_t   current;
    Duty_t   start;
    Duty_t   target;
    uint32_t startMs;
    uint32_t durationMs;
    bool     active;
} FadeEngine_t;

Status_t FadeEngine_Init(FadeEngine_t *engine, Duty_t initial);

/** @brief Begin a fade from the current value to target. */
Status_t FadeEngine_SetTarget(FadeEngine_t *engine, Duty_t target, uint32_t durationMs, uint32_t nowMs);

/** @brief Compute and store the value for time nowMs, and return it. Call every 10 ms. */
Duty_t FadeEngine_Update(FadeEngine_t *engine, uint32_t nowMs);

Duty_t FadeEngine_GetCurrent(const FadeEngine_t *engine);
bool   FadeEngine_IsActive(const FadeEngine_t *engine);

/**
 * @brief  Breathing scale in permille for a time since the mode started.
 * Triangle wave with period LAMP_CFG_BREATHING_PERIOD_MS between LAMP_CFG_BREATHING_MIN_PERMILLE and
 * LAMP_CFG_BREATHING_MAX_PERMILLE, starting at the minimum.
 */
uint16_t FadeEngine_BreathingScalePermille(uint32_t elapsedMs);

#endif /* FADE_ENGINE_H */
