/**
 * @file    bcm_app.h
 * @brief   BCM composition module: wires inputs, command generation, scheduling and logging together.
 *
 * BcmApp_Run10ms executes, in this fixed order:
 *   1. Sample the inputs, debounce, update InputState, request an event VehicleStatus on a door or lock change.
 *   2. Read UART console bytes (fault injection commands when enabled).
 *   3. Run the TxScheduler (LightingCommand and VehicleStatus every 100 ms).
 *   4. Log events and flush the log.
 *
 * Layer: Application (composition).
 */
#ifndef BCM_APP_H
#define BCM_APP_H

#include <stdint.h>

#include "status.h"

Status_t BcmApp_Init(uint32_t nowMs);

/** @brief The 10 ms task (TH-06 equivalent for the BCM). */
void BcmApp_Run10ms(uint32_t nowMs);

#endif /* BCM_APP_H */
