/**
 * @file    protocol.h
 * @brief   CAN message definitions, signal structures, pack and unpack functions.
 *
 * DRAFT [D]: identifiers and layouts are project-defined, not OEM or standard assignments.
 * The CAN specification is finalised in Phase 5. Byte order: Intel (little endian), 11-bit identifiers.
 *
 * Layer: Communication. Host-testable: yes. Shared by both ECUs (single source of truth, decision D-08).
 *
 * Pack functions write the whole frame including the alive counter and, where defined, the CRC-8.
 * Unpack functions only decode. They do not validate values. Validation is done by CommandValidator
 * (Lamp ECU) in the order defined in SRS 3.3.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"
#include "status.h"

/* ---- Identifiers (lower value wins arbitration) ---- */
#define PROTOCOL_ID_LIGHTING_COMMAND     (0x200u)  /**< BCM to Lamp, cyclic. */
#define PROTOCOL_ID_VEHICLE_STATUS       (0x210u)  /**< BCM to Lamp, cyclic and on change. */
#define PROTOCOL_ID_LAMP_STATUS          (0x300u)  /**< Lamp to BCM, cyclic. */
#define PROTOCOL_ID_DIAGNOSTIC_STATUS    (0x310u)  /**< Lamp to BCM, cyclic. */

/* ---- Data length codes ---- */
#define PROTOCOL_DLC_LIGHTING_COMMAND    (8u)
#define PROTOCOL_DLC_VEHICLE_STATUS      (3u)
#define PROTOCOL_DLC_LAMP_STATUS         (4u)
#define PROTOCOL_DLC_DIAGNOSTIC_STATUS   (8u)

/* ---- Cycle times in ms (TH-01, TH-03) ---- */
#define PROTOCOL_PERIOD_LIGHTING_COMMAND_MS   (100u)
#define PROTOCOL_PERIOD_VEHICLE_STATUS_MS     (100u)
#define PROTOCOL_PERIOD_LAMP_STATUS_MS        (100u)
#define PROTOCOL_PERIOD_DIAGNOSTIC_STATUS_MS  (500u)

/* ---- Signal ranges ---- */
#define PROTOCOL_ALIVE_COUNTER_MODULUS   (16u)   /**< AliveCounter is 4 bits: 0 to 15. */
#define PROTOCOL_MODE_STATIC             (0u)
#define PROTOCOL_MODE_BREATHING          (1u)
#define PROTOCOL_MODE_MAX                (1u)    /**< Highest valid LightingMode. */
#define PROTOCOL_BRIGHTNESS_MAX_PERCENT  (100u)
#define PROTOCOL_LOCK_LOCKED             (0u)
#define PROTOCOL_LOCK_UNLOCKED           (1u)
#define PROTOCOL_LOCK_MAX                (1u)    /**< Highest valid VehicleLockState. */

/* ---- FaultBits (DiagnosticStatus byte 6, SRS DIA-002) ---- */
#define FAULT_BIT_CMD_TIMEOUT    (0x01u)  /**< bit0, active. */
#define FAULT_BIT_INVALID_VALUE  (0x02u)  /**< bit1, latched. */
#define FAULT_BIT_CRC_FAIL       (0x04u)  /**< bit2, latched. */
#define FAULT_BIT_ALIVE_FAIL     (0x08u)  /**< bit3, latched. */
#define FAULT_BIT_BUSOFF         (0x10u)  /**< bit4, latched. */
#define FAULT_BIT_VEH_TIMEOUT    (0x20u)  /**< bit5, active. */
#define FAULT_BIT_DLC_FAIL       (0x40u)  /**< bit6, latched. */
#define FAULT_BIT_RX_OVERFLOW    (0x80u)  /**< bit7, latched. */

/** Lamp state as reported in LampStatus and DiagnosticStatus. */
typedef enum
{
    LAMP_STATE_INIT = 0,
    LAMP_STATE_OFF = 1,
    LAMP_STATE_ON = 2,
    LAMP_STATE_WELCOME = 3,
    LAMP_STATE_COURTESY = 4,
    LAMP_STATE_COURTESY_FADEOUT = 5,
    LAMP_STATE_COMM_LOST = 6
} LampState_t;

/** LightingCommand (0x200), BCM to Lamp. */
typedef struct
{
    uint8_t aliveCounter;       /**< 0 to 15. */
    bool    lampEnable;
    uint8_t lightingMode;       /**< PROTOCOL_MODE_STATIC or PROTOCOL_MODE_BREATHING. */
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t brightnessPercent;  /**< 0 to PROTOCOL_BRIGHTNESS_MAX_PERCENT. */
    uint8_t fadeTime;           /**< Unit 10 ms. 0 means no fade. */
    uint8_t reservedBits;       /**< Raw value of the reserved bits. Must be 0 to be valid. */
} LightingCommand_t;

/** VehicleStatus (0x210), BCM to Lamp. */
typedef struct
{
    bool    doorOpen;
    uint8_t lockState;          /**< PROTOCOL_LOCK_LOCKED or PROTOCOL_LOCK_UNLOCKED. */
    uint8_t aliveCounter;       /**< 0 to 15. */
    uint8_t reservedBits;       /**< Non-zero if any reserved bit is set. Exact positions are internal. */
} VehicleStatus_t;

/** LampStatus (0x300), Lamp to BCM. */
typedef struct
{
    uint8_t     activeMode;
    LampState_t lampState;
    uint8_t     outputBrightnessPercent;  /**< Actual output, not the requested value. */
    uint8_t     aliveCounter;
} LampStatus_t;

/** DiagnosticStatus (0x310), Lamp to BCM. */
typedef struct
{
    uint16_t rxValidCount;
    uint16_t rxRejectedCount;
    uint16_t timeoutCount;
    uint8_t  faultBits;         /**< Combination of FAULT_BIT_* values. */
    uint8_t  ecuState;          /**< Same encoding as LampState_t. */
} DiagnosticStatus_t;

/**
 * @brief  Expected DLC for a known identifier.
 * @return DLC, or 0 if the identifier is not part of the message set.
 */
uint8_t Protocol_ExpectedDlc(uint32_t id);

/** @brief Next alive counter value (increment modulo PROTOCOL_ALIVE_COUNTER_MODULUS). */
uint8_t Protocol_NextAliveCounter(uint8_t current);

/**
 * @brief  Check the CRC-8 byte of a received 0x200 or 0x210 frame.
 * @return true if the frame has a known CRC-protected identifier, enough bytes, and a matching CRC.
 *         false for any other identifier (LampStatus and DiagnosticStatus carry no CRC), a short frame,
 *         a NULL pointer, or a CRC mismatch.
 */
bool Protocol_CrcIsValid(const CanFrame_t *frame);

Status_t Protocol_PackLightingCommand(const LightingCommand_t *cmd, CanFrame_t *frame);
Status_t Protocol_UnpackLightingCommand(const CanFrame_t *frame, LightingCommand_t *cmd);

Status_t Protocol_PackVehicleStatus(const VehicleStatus_t *status, CanFrame_t *frame);
Status_t Protocol_UnpackVehicleStatus(const CanFrame_t *frame, VehicleStatus_t *status);

Status_t Protocol_PackLampStatus(const LampStatus_t *status, CanFrame_t *frame);
Status_t Protocol_UnpackLampStatus(const CanFrame_t *frame, LampStatus_t *status);

Status_t Protocol_PackDiagnosticStatus(const DiagnosticStatus_t *status, CanFrame_t *frame);
Status_t Protocol_UnpackDiagnosticStatus(const CanFrame_t *frame, DiagnosticStatus_t *status);

/*
 * Unpack functions return STATUS_ERR_INVALID_ARG if the frame identifier or DLC does not match the
 * message, so a caller can never read bytes that were not received. Pack functions return
 * STATUS_ERR_NULL_ARG on NULL arguments and STATUS_ERR_INVALID_ARG if a signal does not fit its bit field
 * (for example aliveCounter above 15), so the encoder never silently truncates a value.
 */

#endif /* PROTOCOL_H */
