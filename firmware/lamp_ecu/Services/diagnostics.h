/**
 * @file    diagnostics.h
 * @brief   Diagnostic counters and DiagnosticStatus assembly (SRS DIA-001).
 *
 * RxValidCount    : valid LightingCommand and VehicleStatus frames.
 * RxRejectedCount : frames with an expected identifier that failed a check, plus receive queue overflows.
 * TimeoutCount    : every transition into a timeout of either message (including a return from
 *                   COMM_RECOVERING to COMM_LOST).
 * All counters are 16 bit and wrap from 65535 to 0.
 *
 * Layer: Services. Host-testable: yes.
 */
#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>

#include "protocol.h"

typedef struct
{
    uint16_t rxValidCount;
    uint16_t rxRejectedCount;
    uint16_t timeoutCount;
} Diagnostics_t;

void Diagnostics_Init(Diagnostics_t *diag);
void Diagnostics_OnValidFrame(Diagnostics_t *diag);

/** @brief Count rejected frames. count lets the caller add several overflow drops at once. */
void Diagnostics_OnRejectedFrames(Diagnostics_t *diag, uint16_t count);
void Diagnostics_OnTimeout(Diagnostics_t *diag);

/** @brief Assemble the DiagnosticStatus content from the counters, the fault bits and the lamp state. */
void Diagnostics_Fill(const Diagnostics_t *diag, uint8_t faultBits, LampState_t ecuState,
                      DiagnosticStatus_t *out);

#endif /* DIAGNOSTICS_H */
