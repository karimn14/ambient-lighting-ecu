/**
 * @file    lamp_config.h
 * @brief   Lamp ECU configuration constants. Every value traces to a threshold in the SRS (section 4).
 *
 * All values are project-defined targets [D]. If a measurement disagrees, report the measurement and change
 * the SRS first, then this file. test_contracts.c compares these values with the SRS numbers so they cannot
 * drift apart silently.
 */
#ifndef LAMP_CONFIG_H
#define LAMP_CONFIG_H

#include <stdint.h>

#include "lamp_types.h"

/* TH-06: control, fade and monitor task period. */
#define LAMP_CFG_TASK_PERIOD_MS            (10u)

/* TH-04, TH-05: receive timeout and recovery. */
#define LAMP_CFG_RX_TIMEOUT_MS             (300u)
#define LAMP_CFG_RECOVERY_VALID_FRAMES     (2u)

/* TH-08: maximum delay from a processed frame to the first output change. */
#define LAMP_CFG_RESPONSE_LATENCY_MS       (20u)

/* TH-10: courtesy light. */
#define LAMP_CFG_COURTESY_RED              (255u)
#define LAMP_CFG_COURTESY_GREEN            (255u)
#define LAMP_CFG_COURTESY_BLUE             (255u)
#define LAMP_CFG_COURTESY_BRIGHTNESS_PCT   (80u)
#define LAMP_CFG_COURTESY_FADE_IN_MS       (300u)
#define LAMP_CFG_COURTESY_HOLD_MS          (5000u)
#define LAMP_CFG_COURTESY_FADE_OUT_MS      (1000u)

/* TH-11: welcome light. */
#define LAMP_CFG_WELCOME_FADE_IN_MS        (1000u)
#define LAMP_CFG_WELCOME_HOLD_MS           (2000u)
#define LAMP_CFG_WELCOME_FADE_OUT_MS       (1000u)
#define LAMP_CFG_WELCOME_DEFAULT_RED       (0u)
#define LAMP_CFG_WELCOME_DEFAULT_GREEN     (100u)
#define LAMP_CFG_WELCOME_DEFAULT_BLUE      (255u)
#define LAMP_CFG_WELCOME_DEFAULT_BRIGHTNESS_PCT (60u)

/* TH-12: breathing. Scale values are permille (1000 = 100 percent). */
#define LAMP_CFG_BREATHING_PERIOD_MS       (4000u)
#define LAMP_CFG_BREATHING_MIN_PERMILLE    (200u)
#define LAMP_CFG_BREATHING_MAX_PERMILLE    (1000u)

/* TH-13: fades after loss of communication. */
#define LAMP_CFG_COMM_LOSS_FADE_MS         (1000u)
#define LAMP_CFG_VEH_LOSS_FADE_MS          (1000u)

/* TH-14: minimum time between bus-off restart attempts (advanced, COM-007). */
#define LAMP_CFG_BUSOFF_RESTART_MS         (1000u)

/** FadeTime signal unit in ms. */
#define LAMP_CFG_FADE_TIME_UNIT_MS         (10u)

#endif /* LAMP_CONFIG_H */
