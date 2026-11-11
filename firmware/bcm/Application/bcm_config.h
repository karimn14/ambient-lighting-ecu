/**
 * @file    bcm_config.h
 * @brief   BCM configuration constants (SRS BCM-001, BCM-002, BCM-004, BCM-005). All values are [D].
 */
#ifndef BCM_CONFIG_H
#define BCM_CONFIG_H

#include <stdint.h>

/* TH-07: sample every 10 ms, accept a change after 3 equal samples (30 ms). */
#define BCM_CFG_TASK_PERIOD_MS            (10u)
#define BCM_CFG_DEBOUNCE_SAMPLES          (3u)

/* TH-19: an event VehicleStatus is sent on the next 10 ms run, within 20 ms of the debounced change. */
#define BCM_CFG_EVENT_TX_LATENCY_MS       (20u)

/* BCM-002: sequences. */
#define BCM_CFG_COLOR_COUNT               (5u)   /**< white, red, green, blue, amber. */
#define BCM_CFG_BRIGHTNESS_STEP_COUNT     (4u)   /**< 25, 50, 75, 100 percent. */
#define BCM_CFG_MODE_COUNT                (2u)   /**< STATIC, BREATHING. */

/* BCM-005: startup defaults. */
#define BCM_CFG_DEFAULT_COLOR_INDEX       (0u)   /**< White. */
#define BCM_CFG_DEFAULT_BRIGHTNESS_INDEX  (1u)   /**< 50 percent. */
#define BCM_CFG_DEFAULT_MODE              (0u)   /**< STATIC. */
#define BCM_CFG_DEFAULT_FADE_TIME         (50u)  /**< Unit 10 ms, so 500 ms. */

#endif /* BCM_CONFIG_H */
