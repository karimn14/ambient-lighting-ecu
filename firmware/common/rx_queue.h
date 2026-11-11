/**
 * @file    rx_queue.h
 * @brief   Single-producer single-consumer ring buffer between the CAN receive interrupt and the main loop.
 *
 * SRS COM-010: the interrupt only copies the frame here. Validation and application work run in the main loop.
 *
 * Concurrency contract:
 *  - RxQueue_Push is called from one context only (the CAN RX interrupt).
 *  - RxQueue_Pop and the getters are called from one other context only (the main loop).
 *  - Indices are free-running 8-bit counters, so the capacity must be a power of two and at most 128.
 *  - No lock is used. This relies on 8-bit and 16-bit accesses being atomic on the target core. [B] Review
 *    the generated code and add the required memory barriers if the compiler reorders the index update
 *    (Phase 5 review item).
 *
 * Layer: Communication. Host-testable: yes.
 */
#ifndef RX_QUEUE_H
#define RX_QUEUE_H

#include <stdint.h>

#include "can_frame.h"
#include "status.h"

#define RX_QUEUE_CAPACITY    (8u)   /**< TH-15. Power of two. */
#define RX_QUEUE_INDEX_MASK  (RX_QUEUE_CAPACITY - 1u)

typedef struct
{
    CanFrame_t        slots[RX_QUEUE_CAPACITY];
    volatile uint8_t  writeCount;     /**< Free-running, modified by the producer only. */
    volatile uint8_t  readCount;      /**< Free-running, modified by the consumer only. */
    volatile uint16_t overflowCount;  /**< Frames dropped because the queue was full. Producer only. */
} RxQueue_t;

Status_t RxQueue_Init(RxQueue_t *queue);

/** @brief Producer side. Returns STATUS_ERR_FULL and increments overflowCount if no slot is free. */
Status_t RxQueue_Push(RxQueue_t *queue, const CanFrame_t *frame);

/** @brief Consumer side. Returns STATUS_ERR_EMPTY if there is no frame. */
Status_t RxQueue_Pop(RxQueue_t *queue, CanFrame_t *frame);

/** @brief Number of frames currently stored. */
uint8_t RxQueue_GetCount(const RxQueue_t *queue);

/**
 * @brief  Total frames dropped since init (wraps at 65535).
 * The consumer converts changes of this value into RxRejectedCount and RX_OVERFLOW. It never resets it,
 * because only the producer writes it.
 */
uint16_t RxQueue_GetOverflowCount(const RxQueue_t *queue);

#endif /* RX_QUEUE_H */
