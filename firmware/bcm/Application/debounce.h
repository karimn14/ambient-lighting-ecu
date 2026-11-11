/**
 * @file    debounce.h
 * @brief   Counter-based debouncing of one digital input (SRS BCM-001).
 *
 * The debounced state changes only after BCM_CFG_DEBOUNCE_SAMPLES consecutive equal raw samples that differ
 * from the current state. A shorter glitch is ignored.
 *
 * Layer: Application. Host-testable: yes.
 */
#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool    state;      /**< Debounced state. */
    bool    candidate;  /**< Raw value currently being counted. */
    uint8_t count;      /**< Consecutive samples equal to candidate. */
} Debounce_t;

void Debounce_Init(Debounce_t *debounce, bool initialState);

/** @brief Feed one raw sample (every 10 ms). Returns true if the debounced state changed. */
bool Debounce_Update(Debounce_t *debounce, bool rawSample);

bool Debounce_GetState(const Debounce_t *debounce);

#endif /* DEBOUNCE_H */
