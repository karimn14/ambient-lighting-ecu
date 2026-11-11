/**
 * @file    scheduler.h
 * @brief   Cooperative periodic task scheduler driven by a 1 ms tick (decision D-01, SRS SYS-005).
 *
 * The caller passes the current time (HAL_GetTick in firmware, simulated time in host tests).
 * Tasks run to completion and must not block. Due times advance by the period, so the long-term rate has no drift.
 *
 * Layer: Services. Host-testable: yes.
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#include "status.h"

#define SCHEDULER_MAX_TASKS  (8u)

typedef void (*SchedulerTaskFn_t)(uint32_t nowMs);

typedef struct
{
    SchedulerTaskFn_t task;
    uint32_t          periodMs;
    uint32_t          nextDueMs;
} SchedulerEntry_t;

typedef struct
{
    SchedulerEntry_t entries[SCHEDULER_MAX_TASKS];
    uint8_t          count;
    uint32_t         maxLatenessMs;  /**< Largest delay between a task's due time and its start (diagnostic). */
} Scheduler_t;

Status_t Scheduler_Init(Scheduler_t *scheduler);

/** @brief Register a periodic task. periodMs must be at least 1. Returns STATUS_ERR_FULL when out of entries. */
Status_t Scheduler_AddTask(Scheduler_t *scheduler, SchedulerTaskFn_t task, uint32_t periodMs, uint32_t firstDueMs);

/** @brief Run every task whose due time has passed, in registration order. Call on every main-loop pass. */
void Scheduler_Run(Scheduler_t *scheduler, uint32_t nowMs);

uint32_t Scheduler_GetMaxLatenessMs(const Scheduler_t *scheduler);

#endif /* SCHEDULER_H */
