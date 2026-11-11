/**
 * @file    can_frame.h
 * @brief   Hardware-independent classic CAN frame (11-bit identifier, up to 8 data bytes).
 *
 * Extended identifiers, remote frames and CAN FD are out of scope for this project.
 */
#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdint.h>

#define CAN_FRAME_MAX_DLC        (8u)       /**< Maximum payload length of a classic CAN frame. */
#define CAN_FRAME_MAX_STD_ID     (0x7FFu)   /**< Largest 11-bit identifier. */

typedef struct
{
    uint32_t id;                         /**< 11-bit standard identifier. */
    uint8_t  dlc;                        /**< Number of valid data bytes, 0 to CAN_FRAME_MAX_DLC. */
    uint8_t  data[CAN_FRAME_MAX_DLC];    /**< Payload. Bytes at or beyond dlc are unspecified. */
    uint32_t timestampMs;                /**< Time of reception in ms since reset (set by the driver on RX). */
} CanFrame_t;

#endif /* CAN_FRAME_H */
