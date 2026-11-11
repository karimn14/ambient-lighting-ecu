/**
 * @file    tx_scheduler.h
 * @brief   Cyclic and event-triggered transmission timing for outgoing CAN frames (both ECUs).
 *
 * The scheduler decides WHEN to transmit. A build callback decides WHAT to transmit (including the alive
 * counter and CRC, via Protocol_Pack*). A send callback performs the transmission (CanDrv_Transmit in
 * firmware, a stub in host tests). The scheduler itself has no hardware dependency.
 *
 * Rules:
 *  - TxScheduler_Run is called from the 10 ms task. It never blocks.
 *  - Time comparisons are wrap-safe for uint32_t millisecond counters.
 *  - If the send callback returns STATUS_ERR_BUSY (no free mailbox) the transmission is skipped, the dropped
 *    counter increases and the cyclic entry is retried at its next period (BCM-006). A pending event stays
 *    pending and is retried on the next run.
 *
 * Layer: Communication. Host-testable: yes.
 */
#ifndef TX_SCHEDULER_H
#define TX_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"
#include "status.h"

#define TX_SCHEDULER_MAX_ENTRIES  (4u)

/** Sends one frame. Must return STATUS_OK, STATUS_ERR_BUSY (no mailbox) or STATUS_ERR_HW. */
typedef Status_t (*TxSendFn_t)(const CanFrame_t *frame);

/** Fills the frame to send now. Return false to skip this transmission without counting it as dropped. */
typedef bool (*TxBuildFn_t)(CanFrame_t *frame, void *context);

typedef struct
{
    TxBuildFn_t build;
    void       *context;
    uint32_t    periodMs;
    uint32_t    nextDueMs;
    bool        eventPending;
    bool        inUse;
} TxEntry_t;

typedef struct
{
    TxEntry_t  entries[TX_SCHEDULER_MAX_ENTRIES];
    TxSendFn_t send;
    uint8_t    entryCount;
    uint16_t   droppedCount;
} TxScheduler_t;

Status_t TxScheduler_Init(TxScheduler_t *scheduler, TxSendFn_t send);

/**
 * @brief  Register a cyclic message.
 * @param  firstDueMs   Time of the first transmission.
 * @param  entryIndex   Receives the index used by TxScheduler_RequestEvent. May be NULL.
 * @return STATUS_ERR_FULL if all entries are used.
 */
Status_t TxScheduler_AddCyclic(TxScheduler_t *scheduler, uint32_t periodMs, uint32_t firstDueMs,
                               TxBuildFn_t build, void *context, uint8_t *entryIndex);

/** @brief Ask for one immediate extra transmission of an entry (sent on the next run). */
Status_t TxScheduler_RequestEvent(TxScheduler_t *scheduler, uint8_t entryIndex);

/** @brief Send everything that is due. Call every 10 ms. */
void TxScheduler_Run(TxScheduler_t *scheduler, uint32_t nowMs);

/** @brief Transmissions skipped because the send callback reported BUSY (wraps at 65535). */
uint16_t TxScheduler_GetDroppedCount(const TxScheduler_t *scheduler);

#endif /* TX_SCHEDULER_H */
