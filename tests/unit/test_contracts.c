/**
 * @file    test_contracts.c
 * @brief   Interface contract tests (Phase 2).
 *
 * These tests do NOT test behaviour, because no behaviour exists yet. They protect the agreements that the
 * interface headers make with the SRS and with each other:
 *   - message identifiers, DLCs, periods, signal ranges, fault bits, state encodings
 *   - configuration constants against the thresholds in SRS section 4
 *   - structural limits (queue capacity)
 *
 * The expected numbers below are copied from the SRS on purpose. If you change an SRS threshold, change the
 * header AND this file in the same commit. A test that fails here means the code and the SRS disagree.
 *
 * Including every header in one translation unit also detects macro and type name collisions between modules.
 */
#include <stdint.h>

#include "unity.h"

#include "bcm_app.h"
#include "bcm_cmd.h"
#include "bcm_config.h"
#include "can_drv.h"
#include "can_frame.h"
#include "color.h"
#include "command_validator.h"
#include "crc8.h"
#include "debounce.h"
#include "diagnostics.h"
#include "fade_engine.h"
#include "fault_injection.h"
#include "fault_manager.h"
#include "gpio_in.h"
#include "input_state.h"
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

/* ---- Compile-time contracts ------------------------------------------------------------------------- */
_Static_assert((RX_QUEUE_CAPACITY & (RX_QUEUE_CAPACITY - 1u)) == 0u,
               "RX_QUEUE_CAPACITY must be a power of two (free-running index scheme)");
_Static_assert(RX_QUEUE_CAPACITY <= 128u,
               "RX_QUEUE_CAPACITY must be at most 128 (8-bit free-running counters)");
_Static_assert(PROTOCOL_ID_LIGHTING_COMMAND <= CAN_FRAME_MAX_STD_ID, "identifier must fit 11 bits");
_Static_assert(PROTOCOL_ID_DIAGNOSTIC_STATUS <= CAN_FRAME_MAX_STD_ID, "identifier must fit 11 bits");
_Static_assert(PROTOCOL_DLC_LIGHTING_COMMAND <= CAN_FRAME_MAX_DLC, "DLC must fit a classic CAN frame");
_Static_assert(PROTOCOL_DLC_DIAGNOSTIC_STATUS <= CAN_FRAME_MAX_DLC, "DLC must fit a classic CAN frame");

void setUp(void) {}
void tearDown(void) {}

/* ---- CAN message set (SRS 4.1, handbook section 4) -------------------------------------------------- */
static void test_message_identifiers(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x200u, PROTOCOL_ID_LIGHTING_COMMAND);
    TEST_ASSERT_EQUAL_HEX32(0x210u, PROTOCOL_ID_VEHICLE_STATUS);
    TEST_ASSERT_EQUAL_HEX32(0x300u, PROTOCOL_ID_LAMP_STATUS);
    TEST_ASSERT_EQUAL_HEX32(0x310u, PROTOCOL_ID_DIAGNOSTIC_STATUS);
}

static void test_arbitration_priority_order(void)
{
    /* Lower identifier wins arbitration: commands outrank status, status outranks diagnostics. */
    TEST_ASSERT_TRUE(PROTOCOL_ID_LIGHTING_COMMAND < PROTOCOL_ID_VEHICLE_STATUS);
    TEST_ASSERT_TRUE(PROTOCOL_ID_VEHICLE_STATUS < PROTOCOL_ID_LAMP_STATUS);
    TEST_ASSERT_TRUE(PROTOCOL_ID_LAMP_STATUS < PROTOCOL_ID_DIAGNOSTIC_STATUS);
}

static void test_message_dlcs(void)
{
    TEST_ASSERT_EQUAL_UINT8(8u, PROTOCOL_DLC_LIGHTING_COMMAND);
    TEST_ASSERT_EQUAL_UINT8(3u, PROTOCOL_DLC_VEHICLE_STATUS);
    TEST_ASSERT_EQUAL_UINT8(4u, PROTOCOL_DLC_LAMP_STATUS);
    TEST_ASSERT_EQUAL_UINT8(8u, PROTOCOL_DLC_DIAGNOSTIC_STATUS);
}

static void test_message_periods(void)
{
    TEST_ASSERT_EQUAL_UINT32(100u, PROTOCOL_PERIOD_LIGHTING_COMMAND_MS);   /* TH-01 */
    TEST_ASSERT_EQUAL_UINT32(100u, PROTOCOL_PERIOD_VEHICLE_STATUS_MS);     /* TH-01 */
    TEST_ASSERT_EQUAL_UINT32(100u, PROTOCOL_PERIOD_LAMP_STATUS_MS);        /* TH-03 */
    TEST_ASSERT_EQUAL_UINT32(500u, PROTOCOL_PERIOD_DIAGNOSTIC_STATUS_MS);  /* TH-03 */
}

static void test_signal_ranges(void)
{
    TEST_ASSERT_EQUAL_UINT(16u, PROTOCOL_ALIVE_COUNTER_MODULUS);
    TEST_ASSERT_EQUAL_UINT(0u, PROTOCOL_MODE_STATIC);
    TEST_ASSERT_EQUAL_UINT(1u, PROTOCOL_MODE_BREATHING);
    TEST_ASSERT_EQUAL_UINT(1u, PROTOCOL_MODE_MAX);          /* A1: only STATIC and BREATHING */
    TEST_ASSERT_EQUAL_UINT(100u, PROTOCOL_BRIGHTNESS_MAX_PERCENT);
    TEST_ASSERT_EQUAL_UINT(0u, PROTOCOL_LOCK_LOCKED);
    TEST_ASSERT_EQUAL_UINT(1u, PROTOCOL_LOCK_UNLOCKED);
    TEST_ASSERT_EQUAL_UINT(1u, PROTOCOL_LOCK_MAX);
}

/* ---- Fault bits and state encoding (SRS DIA-002, LampStatus layout) --------------------------------- */
static void test_fault_bit_positions(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x01u, FAULT_BIT_CMD_TIMEOUT);
    TEST_ASSERT_EQUAL_HEX8(0x02u, FAULT_BIT_INVALID_VALUE);
    TEST_ASSERT_EQUAL_HEX8(0x04u, FAULT_BIT_CRC_FAIL);
    TEST_ASSERT_EQUAL_HEX8(0x08u, FAULT_BIT_ALIVE_FAIL);
    TEST_ASSERT_EQUAL_HEX8(0x10u, FAULT_BIT_BUSOFF);
    TEST_ASSERT_EQUAL_HEX8(0x20u, FAULT_BIT_VEH_TIMEOUT);
    TEST_ASSERT_EQUAL_HEX8(0x40u, FAULT_BIT_DLC_FAIL);
    TEST_ASSERT_EQUAL_HEX8(0x80u, FAULT_BIT_RX_OVERFLOW);
}

static void test_fault_bits_are_distinct_and_fill_the_byte(void)
{
    const uint8_t all = (uint8_t)(FAULT_BIT_CMD_TIMEOUT | FAULT_BIT_INVALID_VALUE | FAULT_BIT_CRC_FAIL |
                                  FAULT_BIT_ALIVE_FAIL | FAULT_BIT_BUSOFF | FAULT_BIT_VEH_TIMEOUT |
                                  FAULT_BIT_DLC_FAIL | FAULT_BIT_RX_OVERFLOW);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, all);   /* eight distinct single bits cover the whole byte */
}

static void test_lamp_state_encoding(void)
{
    TEST_ASSERT_EQUAL_INT(0, (int)LAMP_STATE_INIT);
    TEST_ASSERT_EQUAL_INT(1, (int)LAMP_STATE_OFF);
    TEST_ASSERT_EQUAL_INT(2, (int)LAMP_STATE_ON);
    TEST_ASSERT_EQUAL_INT(3, (int)LAMP_STATE_WELCOME);
    TEST_ASSERT_EQUAL_INT(4, (int)LAMP_STATE_COURTESY);
    TEST_ASSERT_EQUAL_INT(5, (int)LAMP_STATE_COURTESY_FADEOUT);
    TEST_ASSERT_EQUAL_INT(6, (int)LAMP_STATE_COMM_LOST);
}

static void test_status_ok_is_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, (int)STATUS_OK);
}

/* ---- Thresholds (SRS section 4) ------------------------------------------------------------------------ */
static void test_lamp_thresholds_match_srs(void)
{
    TEST_ASSERT_EQUAL_UINT32(10u,   LAMP_CFG_TASK_PERIOD_MS);          /* TH-06 */
    TEST_ASSERT_EQUAL_UINT32(300u,  LAMP_CFG_RX_TIMEOUT_MS);           /* TH-04 */
    TEST_ASSERT_EQUAL_UINT32(2u,    LAMP_CFG_RECOVERY_VALID_FRAMES);   /* TH-05 */
    TEST_ASSERT_EQUAL_UINT32(20u,   LAMP_CFG_RESPONSE_LATENCY_MS);     /* TH-08 */
    TEST_ASSERT_EQUAL_UINT32(300u,  LAMP_CFG_COURTESY_FADE_IN_MS);     /* TH-10 */
    TEST_ASSERT_EQUAL_UINT32(5000u, LAMP_CFG_COURTESY_HOLD_MS);        /* TH-10 */
    TEST_ASSERT_EQUAL_UINT32(1000u, LAMP_CFG_COURTESY_FADE_OUT_MS);    /* TH-10 */
    TEST_ASSERT_EQUAL_UINT32(80u,   LAMP_CFG_COURTESY_BRIGHTNESS_PCT); /* TH-10 */
    TEST_ASSERT_EQUAL_UINT32(1000u, LAMP_CFG_WELCOME_FADE_IN_MS);      /* TH-11 */
    TEST_ASSERT_EQUAL_UINT32(2000u, LAMP_CFG_WELCOME_HOLD_MS);         /* TH-11 */
    TEST_ASSERT_EQUAL_UINT32(1000u, LAMP_CFG_WELCOME_FADE_OUT_MS);     /* TH-11 */
    TEST_ASSERT_EQUAL_UINT32(60u,   LAMP_CFG_WELCOME_DEFAULT_BRIGHTNESS_PCT); /* TH-11 */
    TEST_ASSERT_EQUAL_UINT32(4000u, LAMP_CFG_BREATHING_PERIOD_MS);     /* TH-12 */
    TEST_ASSERT_EQUAL_UINT32(200u,  LAMP_CFG_BREATHING_MIN_PERMILLE);  /* TH-12: 20 percent */
    TEST_ASSERT_EQUAL_UINT32(1000u, LAMP_CFG_BREATHING_MAX_PERMILLE);  /* TH-12: 100 percent */
    TEST_ASSERT_EQUAL_UINT32(1000u, LAMP_CFG_COMM_LOSS_FADE_MS);       /* TH-13 */
    TEST_ASSERT_EQUAL_UINT32(1000u, LAMP_CFG_VEH_LOSS_FADE_MS);        /* TH-13 */
    TEST_ASSERT_EQUAL_UINT32(1000u, LAMP_CFG_BUSOFF_RESTART_MS);       /* TH-14 */
    TEST_ASSERT_EQUAL_UINT32(10u,   LAMP_CFG_FADE_TIME_UNIT_MS);       /* LightingCommand FadeTime unit */
}

static void test_courtesy_and_welcome_colors_match_srs(void)
{
    TEST_ASSERT_EQUAL_UINT32(255u, LAMP_CFG_COURTESY_RED);
    TEST_ASSERT_EQUAL_UINT32(255u, LAMP_CFG_COURTESY_GREEN);
    TEST_ASSERT_EQUAL_UINT32(255u, LAMP_CFG_COURTESY_BLUE);
    TEST_ASSERT_EQUAL_UINT32(0u,   LAMP_CFG_WELCOME_DEFAULT_RED);
    TEST_ASSERT_EQUAL_UINT32(100u, LAMP_CFG_WELCOME_DEFAULT_GREEN);
    TEST_ASSERT_EQUAL_UINT32(255u, LAMP_CFG_WELCOME_DEFAULT_BLUE);
}

static void test_bcm_thresholds_match_srs(void)
{
    TEST_ASSERT_EQUAL_UINT32(10u, BCM_CFG_TASK_PERIOD_MS);       /* TH-07 sample period */
    TEST_ASSERT_EQUAL_UINT32(3u,  BCM_CFG_DEBOUNCE_SAMPLES);     /* TH-07: 30 ms */
    TEST_ASSERT_EQUAL_UINT32(20u, BCM_CFG_EVENT_TX_LATENCY_MS);  /* TH-19 */
    TEST_ASSERT_EQUAL_UINT32(5u,  BCM_CFG_COLOR_COUNT);          /* BCM-002 palette */
    TEST_ASSERT_EQUAL_UINT32(4u,  BCM_CFG_BRIGHTNESS_STEP_COUNT);/* BCM-002 steps */
    TEST_ASSERT_EQUAL_UINT32(2u,  BCM_CFG_MODE_COUNT);           /* BCM-002 modes */
    TEST_ASSERT_EQUAL_UINT32(50u, BCM_CFG_DEFAULT_FADE_TIME);    /* BCM-005: 500 ms */
    TEST_ASSERT_EQUAL_UINT32(1u,  BCM_CFG_DEFAULT_BRIGHTNESS_INDEX); /* BCM-005: 50 percent */
}

static void test_derived_timing_relations(void)
{
    /* The receive timeout is three command periods (TH-04). */
    TEST_ASSERT_EQUAL_UINT32(3u * PROTOCOL_PERIOD_LIGHTING_COMMAND_MS, LAMP_CFG_RX_TIMEOUT_MS);
    /* The event latency budget is two task periods (TH-08, TH-19). */
    TEST_ASSERT_EQUAL_UINT32(2u * LAMP_CFG_TASK_PERIOD_MS, LAMP_CFG_RESPONSE_LATENCY_MS);
    TEST_ASSERT_EQUAL_UINT32(2u * BCM_CFG_TASK_PERIOD_MS, BCM_CFG_EVENT_TX_LATENCY_MS);
    /* Cyclic periods are whole multiples of the task period, so the 10 ms task can serve them. */
    TEST_ASSERT_EQUAL_UINT32(0u, PROTOCOL_PERIOD_LIGHTING_COMMAND_MS % LAMP_CFG_TASK_PERIOD_MS);
    TEST_ASSERT_EQUAL_UINT32(0u, PROTOCOL_PERIOD_DIAGNOSTIC_STATUS_MS % LAMP_CFG_TASK_PERIOD_MS);
}

/* ---- Limits ------------------------------------------------------------------------------------------- */
static void test_pwm_resolution_and_queue_limits(void)
{
    TEST_ASSERT_EQUAL_UINT32(1024u, DUTY_FULL_SCALE_COUNTS);   /* TH-09: 1024 counts */
    TEST_ASSERT_EQUAL_UINT32(8u, RX_QUEUE_CAPACITY);           /* TH-15 */
    TEST_ASSERT_EQUAL_UINT32(7u, RX_QUEUE_INDEX_MASK);
}

static void test_crc_parameters(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x1Du, CRC8_POLYNOMIAL);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, CRC8_INITIAL_VALUE);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, CRC8_FINAL_XOR);
    /* CRC8_CHECK_VALUE_123456789 is [B]: not asserted until it is confirmed with an independent tool. */
}

static void test_scheduler_and_tx_capacity_hold_the_planned_entries(void)
{
    /* Lamp: 3 tasks planned at most today; BCM: 2 cyclic messages. Leave room to grow. */
    TEST_ASSERT_TRUE(SCHEDULER_MAX_TASKS >= 4u);
    TEST_ASSERT_TRUE(TX_SCHEDULER_MAX_ENTRIES >= 2u);
}

static void test_fault_injection_disabled_by_default(void)
{
    /* Without BCM_ENABLE_FAULT_INJECTION the stub reports no injection (AD-06). */
    TEST_ASSERT_EQUAL_INT((int)FAULT_INJ_NONE, (int)FaultInjection_GetMode());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_message_identifiers);
    RUN_TEST(test_arbitration_priority_order);
    RUN_TEST(test_message_dlcs);
    RUN_TEST(test_message_periods);
    RUN_TEST(test_signal_ranges);
    RUN_TEST(test_fault_bit_positions);
    RUN_TEST(test_fault_bits_are_distinct_and_fill_the_byte);
    RUN_TEST(test_lamp_state_encoding);
    RUN_TEST(test_status_ok_is_zero);
    RUN_TEST(test_lamp_thresholds_match_srs);
    RUN_TEST(test_courtesy_and_welcome_colors_match_srs);
    RUN_TEST(test_bcm_thresholds_match_srs);
    RUN_TEST(test_derived_timing_relations);
    RUN_TEST(test_pwm_resolution_and_queue_limits);
    RUN_TEST(test_crc_parameters);
    RUN_TEST(test_scheduler_and_tx_capacity_hold_the_planned_entries);
    RUN_TEST(test_fault_injection_disabled_by_default);
    return UNITY_END();
}
