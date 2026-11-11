/**
 * @file    include_probe.c
 * @brief   Build probe for the Lamp ECU CubeIDE project (Phase 2).
 *
 * Purpose: if this file compiles in CubeIDE, then the source folders and the include paths of the project
 * are configured correctly. It defines one constant and no behaviour. Delete it in Phase 3 when the first
 * real Application source file exists.
 */
#include <stdint.h>

#include "can_drv.h"
#include "can_frame.h"
#include "color.h"
#include "command_validator.h"
#include "crc8.h"
#include "diagnostics.h"
#include "fade_engine.h"
#include "fault_manager.h"
#include "lamp_app.h"
#include "lamp_config.h"
#include "lamp_fsm.h"
#include "lamp_types.h"
#include "led_pwm.h"
#include "log.h"
#include "protocol.h"
#include "rx_queue.h"
#include "scheduler.h"
#include "status.h"
#include "timeout_monitor.h"
#include "tx_scheduler.h"
#include "uart_drv.h"

/** Non-zero value so the translation unit is not empty. */
const uint8_t g_lampIncludeProbe = 1u;
