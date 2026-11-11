/**
 * @file    fault_injection.h
 * @brief   Test-only fault injection on outgoing frames (SRS TST-005). Selected by UART command.
 *
 * Gating: the module is compiled in only when BCM_ENABLE_FAULT_INJECTION is defined. Without it the header
 * provides empty inline replacements, so a normal build carries no injection code path (decision AD-06).
 *
 * Layer: Application. Host-testable: yes (when enabled).
 */
#ifndef FAULT_INJECTION_H
#define FAULT_INJECTION_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"
#include "status.h"

typedef enum
{
    FAULT_INJ_NONE = 0,
    FAULT_INJ_WRONG_ID,            /**< Send LightingCommand with an unexpected identifier. */
    FAULT_INJ_WRONG_DLC,           /**< Send LightingCommand with DLC 7. */
    FAULT_INJ_BAD_CRC,             /**< Corrupt the CRC byte. */
    FAULT_INJ_FROZEN_ALIVE,        /**< Repeat the previous alive counter. */
    FAULT_INJ_MODE_OUT_OF_RANGE,   /**< LightingMode above the valid range, with a correct CRC. */
    FAULT_INJ_BRIGHTNESS_OUT_OF_RANGE,
    FAULT_INJ_RESERVED_BITS,       /**< Reserved bits set, with a correct CRC. */
    FAULT_INJ_STOP_ALL,            /**< Transmit nothing. */
    FAULT_INJ_STOP_VEHICLE         /**< Stop VehicleStatus only. */
} FaultInjectionMode_t;

#ifdef BCM_ENABLE_FAULT_INJECTION

void FaultInjection_Init(void);

/** @brief Parse a UART command line such as "inject crc" or "inject none". Returns STATUS_ERR_INVALID_ARG if unknown. */
Status_t FaultInjection_ParseCommand(const char *line, FaultInjectionMode_t *mode);

void FaultInjection_SetMode(FaultInjectionMode_t mode);
FaultInjectionMode_t FaultInjection_GetMode(void);

/**
 * @brief  Alter an outgoing frame according to the active mode.
 * @return false if the frame must not be transmitted at all.
 */
bool FaultInjection_ApplyToFrame(CanFrame_t *frame);

#else

static inline void FaultInjection_Init(void) {}
static inline FaultInjectionMode_t FaultInjection_GetMode(void) { return FAULT_INJ_NONE; }
static inline bool FaultInjection_ApplyToFrame(CanFrame_t *frame) { (void)frame; return true; }

#endif /* BCM_ENABLE_FAULT_INJECTION */

#endif /* FAULT_INJECTION_H */
