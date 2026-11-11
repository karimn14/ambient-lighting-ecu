/**
 * @file    lamp_app.h
 * @brief   Lamp ECU composition module: wires the modules together and defines the order of work.
 *
 * Not shown as a box in the layer diagram. It is the top of the Application layer and the only module that
 * knows all the others.
 *
 * LampApp_Run10ms executes, in this fixed order (decision AD-07):
 *   1. Drain the RxQueue. For each frame: validate, update FaultManager, Diagnostics, LampFsm,
 *      TimeoutMonitor, and forget the alive counter when a monitor enters its lost state.
 *   2. Update the timeout monitors, report transitions.
 *   3. Handle new queue overflows and CAN driver status (bus-off).
 *   4. Step the LampFsm, apply its request to the FadeEngine, compute the output, write it with LedPwm.
 *   5. Run the TxScheduler (LampStatus every 100 ms, DiagnosticStatus every 500 ms).
 *   6. Log events.
 *
 * Layer: Application (composition).
 */
#ifndef LAMP_APP_H
#define LAMP_APP_H

#include <stdint.h>

#include "can_frame.h"
#include "status.h"

/** @brief Initialise all modules. The output is 0 before the PWM outputs are enabled (SYS-003). */
Status_t LampApp_Init(uint32_t nowMs);

/**
 * @brief  CAN receive callback. Interrupt context. Only copies the frame into the RxQueue.
 * Register it with CanDrv_SetRxCallback.
 */
void LampApp_RxIsr(const CanFrame_t *frame);

/** @brief The 10 ms task (TH-06). */
void LampApp_Run10ms(uint32_t nowMs);

#endif /* LAMP_APP_H */
