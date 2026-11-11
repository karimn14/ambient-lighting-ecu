/**
 * @file    bcm_cmd.h
 * @brief   Builds the content of LightingCommand and VehicleStatus from the input state (SRS BCM-003).
 *
 * Pure functions. No HAL, no CAN, no hidden state. The alive counter is passed in, owned by the caller.
 * Frame encoding (including CRC-8) is done afterwards by Protocol_Pack*.
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef BCM_CMD_H
#define BCM_CMD_H

#include <stdint.h>

#include "input_state.h"
#include "protocol.h"

void BcmCmd_BuildLighting(const InputState_t *state, uint8_t aliveCounter, LightingCommand_t *out);
void BcmCmd_BuildVehicle(const InputState_t *state, uint8_t aliveCounter, VehicleStatus_t *out);

#endif /* BCM_CMD_H */
