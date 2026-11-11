/**
 * @file    can_drv.h
 * @brief   Interface of the CAN driver. Implemented per project in Drivers_App on top of the STM32 HAL (bxCAN).
 *
 * Only this interface is visible to the upper layers. It uses no HAL types.
 * The HAL is not an AUTOSAR MCAL, and this wrapper is not an AUTOSAR CAN Driver.
 *
 * Layer: Drivers (interface header).
 *
 * [B] Verify against the installed STM32Cube packages in Phase 5: HAL_CAN_ConfigFilter, HAL_CAN_Start,
 * HAL_CAN_AddTxMessage, HAL_CAN_GetRxMessage, HAL_CAN_ActivateNotification and the RX callback names.
 */
#ifndef CAN_DRV_H
#define CAN_DRV_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"
#include "status.h"

typedef enum
{
    CAN_DRV_MODE_NORMAL = 0,
    CAN_DRV_MODE_LOOPBACK,         /**< Frames are looped back internally. Used for single-node driver tests. */
    CAN_DRV_MODE_SILENT,           /**< Receive only, never drives the bus. */
    CAN_DRV_MODE_SILENT_LOOPBACK
} CanDrvMode_t;

/** Acceptance filter: a frame passes if (frame.id & mask) == (id & mask). 11-bit identifiers. */
typedef struct
{
    uint32_t id;
    uint32_t mask;
} CanDrvFilter_t;

typedef struct
{
    uint8_t  txErrorCounter;
    uint8_t  rxErrorCounter;
    bool     errorWarning;
    bool     errorPassive;
    bool     busOff;
    uint32_t lastErrorFlags;  /**< Raw driver error flags for logging. */
} CanDrvStatus_t;

/** Called from interrupt context for every accepted frame. Must only copy the frame (see RxQueue). */
typedef void (*CanDrvRxCallback_t)(const CanFrame_t *frame);

Status_t CanDrv_Init(CanDrvMode_t mode, const CanDrvFilter_t *filters, uint8_t filterCount);
void     CanDrv_SetRxCallback(CanDrvRxCallback_t callback);
Status_t CanDrv_Start(void);
Status_t CanDrv_Stop(void);

/** @brief Transmit one frame. Returns STATUS_ERR_BUSY if no mailbox is free. Never blocks. */
Status_t CanDrv_Transmit(const CanFrame_t *frame);

void CanDrv_GetStatus(CanDrvStatus_t *status);

/** @brief Stop and start the peripheral again, for bus-off recovery (COM-007). */
Status_t CanDrv_Restart(void);

#endif /* CAN_DRV_H */
