/**
 * @file    timeout_monitor.h
 * @brief   Time since the last valid frame of one message (SRS COM-001, COM-008).
 *
 * Semantics:
 *  - Not armed until the first TimeoutMonitor_Feed. A node that never received a frame is not "timed out",
 *    it is in its startup state (SYS-003).
 *  - Update reports true exactly once per transition into the timed-out state.
 *  - Feed clears the timed-out state and restarts the timer.
 *  - Timed out when (now - lastValid) >= timeoutMs. Wrap-safe for uint32_t.
 *
 * Detection time with a 10 ms Update period is timeoutMs to timeoutMs + 10 ms (TH-04).
 *
 * Layer: Services. Host-testable: yes.
 */
#ifndef TIMEOUT_MONITOR_H
#define TIMEOUT_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#include "status.h"

typedef struct
{
    uint32_t timeoutMs;
    uint32_t lastValidMs;
    bool     armed;
    bool     timedOut;
} TimeoutMonitor_t;

Status_t TimeoutMonitor_Init(TimeoutMonitor_t *monitor, uint32_t timeoutMs);

/** @brief A valid frame arrived at nowMs. */
void TimeoutMonitor_Feed(TimeoutMonitor_t *monitor, uint32_t nowMs);

/** @brief Check the timer. Returns true only on the transition into the timed-out state. */
bool TimeoutMonitor_Update(TimeoutMonitor_t *monitor, uint32_t nowMs);

bool TimeoutMonitor_IsTimedOut(const TimeoutMonitor_t *monitor);
bool TimeoutMonitor_IsArmed(const TimeoutMonitor_t *monitor);

/** @brief Back to the not-armed state. */
void TimeoutMonitor_Reset(TimeoutMonitor_t *monitor);

#endif /* TIMEOUT_MONITOR_H */
