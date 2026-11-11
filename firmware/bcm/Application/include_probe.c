/**
 * @file    include_probe.c
 * @brief   Build probe for the BCM CubeIDE project (Phase 2).
 *
 * Purpose: if this file compiles in CubeIDE, then the source folders and the include paths of the project
 * are configured correctly. It defines one constant and no behaviour. Delete it in Phase 4 when the first
 * real Application source file exists.
 */
#include <stdint.h>

#include "bcm_app.h"
#include "bcm_cmd.h"
#include "bcm_config.h"
#include "can_drv.h"
#include "can_frame.h"
#include "crc8.h"
#include "debounce.h"
#include "fault_injection.h"
#include "gpio_in.h"
#include "input_state.h"
#include "lamp_types.h"
#include "log.h"
#include "protocol.h"
#include "scheduler.h"
#include "status.h"
#include "tx_scheduler.h"
#include "uart_drv.h"

/** Non-zero value so the translation unit is not empty. */
const uint8_t g_bcmIncludeProbe = 1u;
