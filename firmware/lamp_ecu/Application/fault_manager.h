/**
 * @file    fault_manager.h
 * @brief   Communication fault state machines and fault bits (SRS COM-001, COM-006, COM-007, COM-008, DIA-002).
 *
 * Two independent monitors:
 *  - LightingCommand: COMM_STARTUP, COMM_OK, COMM_LOST, COMM_RECOVERING. Owns CMD_TIMEOUT.
 *  - VehicleStatus:   VEH_UNKNOWN, VEH_OK, VEH_LOST. Owns VEH_TIMEOUT.
 *
 * Active bits (CMD_TIMEOUT, VEH_TIMEOUT) follow the current condition. Latched bits stay until reset.
 * The manager records what happened. It does not drive outputs and does not count (Diagnostics counts).
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "command_validator.h"
#include "protocol.h"

typedef enum
{
    COMM_STATE_STARTUP = 0,
    COMM_STATE_OK,
    COMM_STATE_LOST,
    COMM_STATE_RECOVERING
} CommState_t;

typedef enum
{
    VEH_STATE_UNKNOWN = 0,
    VEH_STATE_OK,
    VEH_STATE_LOST
} VehState_t;

typedef struct
{
    CommState_t commState;
    VehState_t  vehState;
    uint8_t     consecutiveValid;   /**< Valid LightingCommand frames counted during recovery. */
    uint8_t     recoveryFrames;     /**< Required count (N). */
    uint8_t     activeBits;
    uint8_t     latchedBits;
} FaultManager_t;

void FaultManager_Init(FaultManager_t *manager, uint8_t recoveryFrames);

/**
 * @brief  A valid LightingCommand was received.
 * @return true when this frame completed a recovery (COMM_RECOVERING to COMM_OK).
 */
bool FaultManager_OnValidLightingCommand(FaultManager_t *manager);

/** @brief A valid VehicleStatus was received. Clears VEH_TIMEOUT. */
void FaultManager_OnValidVehicleStatus(FaultManager_t *manager);

/** @brief A frame with an expected identifier was rejected. Latches its bit and resets a recovery in progress. */
void FaultManager_OnRejected(FaultManager_t *manager, ValidationResult_t result, uint32_t id);

/** @brief The LightingCommand monitor timed out. Moves to COMM_LOST and sets CMD_TIMEOUT. */
void FaultManager_OnCommandTimeout(FaultManager_t *manager);

/** @brief The VehicleStatus monitor timed out. Moves to VEH_LOST and sets VEH_TIMEOUT. */
void FaultManager_OnVehicleTimeout(FaultManager_t *manager);

/** @brief Bus-off detected. Latches BUSOFF and treats it as a command timeout. */
void FaultManager_OnBusOff(FaultManager_t *manager);

/** @brief Receive queue overflow detected. Latches RX_OVERFLOW. */
void FaultManager_OnRxOverflow(FaultManager_t *manager);

CommState_t FaultManager_GetCommState(const FaultManager_t *manager);
VehState_t  FaultManager_GetVehState(const FaultManager_t *manager);

/** @brief Active and latched bits combined, ready for DiagnosticStatus byte 6. */
uint8_t FaultManager_GetFaultBits(const FaultManager_t *manager);

#endif /* FAULT_MANAGER_H */
