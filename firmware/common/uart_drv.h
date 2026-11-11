/**
 * @file    uart_drv.h
 * @brief   Interface of the UART driver. Implemented per project in Drivers_App (HAL based).
 *
 * Non-blocking: a write never waits for the line. Implementations use an interrupt-driven transmit buffer
 * (or DMA). Host tests provide a stub.
 *
 * Layer: Drivers (interface header, contains no HAL types).
 */
#ifndef UART_DRV_H
#define UART_DRV_H

#include <stdbool.h>
#include <stdint.h>

#include "status.h"

Status_t UartDrv_Init(void);

/** @brief Queue bytes for transmission. Returns how many bytes were accepted (0 to length). */
uint16_t UartDrv_Write(const uint8_t *data, uint16_t length);

/** @brief Fetch one received byte if available. Returns false if none. */
bool UartDrv_ReadByte(uint8_t *byte);

#endif /* UART_DRV_H */
