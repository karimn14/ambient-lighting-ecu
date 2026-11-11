/**
 * @file    lamp_fsm.h
 * @brief   Lamp state machine and output layer selection (SRS 3.1, LMP-001, LMP-004, LMP-006 to LMP-009,
 *          COM-008, COM-009).
 *
 * The FSM decides WHAT the output should be (a target duty, a fade time, a breathing flag). It does not
 * fade and does not touch hardware. LampApp passes its request to the FadeEngine and LedPwm.
 *
 * Precedence: COMM_LOST, then COURTESY, then WELCOME, then BASE (SRS 3.1).
 * Door state is level-triggered. The lock state is edge-triggered (LOCKED to UNLOCKED starts the welcome
 * sequence). The first VehicleStatus only sets the baseline (COM-009).
 *
 * All fields of LampFsm_t are private. Use the functions.
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef LAMP_FSM_H
#define LAMP_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include "lamp_types.h"
#include "protocol.h"

/** What the FSM wants the output to do. */
typedef struct
{
    bool     retarget;        /**< True when target and fadeMs must be given to the FadeEngine now. */
    Duty_t   target;          /**< Target duty before any breathing scale. */
    uint32_t fadeMs;          /**< Fade duration for the retarget. */
    bool     breathing;       /**< True when the breathing scale must be applied on top of the faded value. */
    uint32_t breathingElapsedMs; /**< Time since the breathing mode started (input of the waveform). */
} LampOutputRequest_t;

typedef enum
{
    LAMP_PHASE_NONE = 0,
    LAMP_PHASE_FADE_IN,
    LAMP_PHASE_HOLD,
    LAMP_PHASE_FADE_OUT
} LampPhase_t;

typedef struct
{
    LampState_t state;
    LampPhase_t phase;
    uint32_t    phaseStartMs;
    bool        baseValid;           /**< A valid LightingCommand has been received. */
    bool        baseEnable;
    uint8_t     baseMode;
    Rgb_t       baseColor;
    uint8_t     baseBrightnessPercent;
    uint8_t     baseFadeTime;
    bool        vehicleBaselineSet;
    bool        doorOpen;
    uint8_t     lockState;
    uint32_t    breathingStartMs;
    bool        retargetPending;
    uint32_t    pendingFadeMs;
} LampFsm_t;

void LampFsm_Init(LampFsm_t *fsm);

/** @brief A valid LightingCommand was accepted. */
void LampFsm_OnLightingCommand(LampFsm_t *fsm, const LightingCommand_t *cmd, uint32_t nowMs);

/** @brief A valid VehicleStatus was accepted. */
void LampFsm_OnVehicleStatus(LampFsm_t *fsm, const VehicleStatus_t *status, uint32_t nowMs);

/** @brief The LightingCommand timeout occurred (enter COMM_LOST, fade all to 0). */
void LampFsm_OnCommLost(LampFsm_t *fsm, uint32_t nowMs);

/** @brief Recovery completed after the required number of valid frames. */
void LampFsm_OnCommRecovered(LampFsm_t *fsm, uint32_t nowMs);

/** @brief The VehicleStatus timeout occurred (door treated as closed, overlays end). */
void LampFsm_OnVehicleLost(LampFsm_t *fsm, uint32_t nowMs);

/** @brief Advance timers and produce the output request. Call every 10 ms after the events of that pass. */
void LampFsm_Step(LampFsm_t *fsm, uint32_t nowMs, LampOutputRequest_t *request);

LampState_t LampFsm_GetState(const LampFsm_t *fsm);

/** @brief LightingMode currently executing (reported in LampStatus). */
uint8_t LampFsm_GetActiveMode(const LampFsm_t *fsm);

#endif /* LAMP_FSM_H */
