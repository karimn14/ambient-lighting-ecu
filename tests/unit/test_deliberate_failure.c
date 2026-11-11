/**
 * @file    test_deliberate_failure.c
 * @brief   Always fails. Built only with -DDELIBERATE_FAILURE=ON to prove that a failing test makes the
 *          whole run return a non-zero exit code (SRS TST-002).
 */
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_this_must_fail(void)
{
    TEST_FAIL_MESSAGE("deliberate failure to check the exit code of the test run");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_this_must_fail);
    return UNITY_END();
}
