/**
 * @file    command_validator.h
 * @brief   Frame validation in the order defined in SRS 3.3 (SYS-002, COM-002, COM-003, COM-004).
 *
 * Order: identifier, DLC, CRC-8, alive counter, value ranges and reserved bits.
 * The validator decodes the frame only if every check passed. It remembers the last accepted alive counter
 * of each message and updates it only on acceptance. It does NOT touch application state, counters or
 * timeouts. The caller reacts to the result.
 *
 * Known limitation (to be stated in the README): only a repeated alive counter is rejected. A jump of more
 * than one is accepted. A 4-bit counter and a CRC-8 do not protect against replay of an older frame or
 * against a deliberate attacker.
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef COMMAND_VALIDATOR_H
#define COMMAND_VALIDATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"
#include "protocol.h"

typedef enum
{
    VALIDATION_OK = 0,
    VALIDATION_IGNORED_ID,     /**< Identifier is not 0x200 or 0x210. No counter changes. */
    VALIDATION_REJECT_DLC,     /**< Maps to DLC_FAIL. */
    VALIDATION_REJECT_CRC,     /**< Maps to CRC_FAIL. */
    VALIDATION_REJECT_ALIVE,   /**< Maps to ALIVE_FAIL. */
    VALIDATION_REJECT_RANGE    /**< Maps to INVALID_VALUE. */
} ValidationResult_t;

typedef struct
{
    bool    lightingHasPrevious;
    uint8_t lightingPreviousAlive;
    bool    vehicleHasPrevious;
    uint8_t vehiclePreviousAlive;
} CommandValidator_t;

void CommandValidator_Init(CommandValidator_t *validator);

/** @brief Forget the previous LightingCommand counter. The next frame passes the alive check (startup, recovery). */
void CommandValidator_ForgetLighting(CommandValidator_t *validator);

/** @brief Forget the previous VehicleStatus counter. */
void CommandValidator_ForgetVehicle(CommandValidator_t *validator);

/**
 * @brief  Validate one received frame.
 * @param  frame      Received frame.
 * @param  lighting   Receives the decoded command when the result is VALIDATION_OK for id 0x200. May be NULL.
 * @param  vehicle    Receives the decoded status when the result is VALIDATION_OK for id 0x210. May be NULL.
 * @return VALIDATION_OK or the first failing check. A NULL frame is reported as VALIDATION_IGNORED_ID.
 */
ValidationResult_t CommandValidator_Check(CommandValidator_t *validator, const CanFrame_t *frame,
                                          LightingCommand_t *lighting, VehicleStatus_t *vehicle);

/** @brief Fault bit set by a rejection result (0 for VALIDATION_OK and VALIDATION_IGNORED_ID). */
uint8_t CommandValidator_FaultBitFor(ValidationResult_t result);

#endif /* COMMAND_VALIDATOR_H */
