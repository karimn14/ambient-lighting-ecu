/**
 * @file    test_sanity.c
 * @brief   Proves that the host test chain works (compiler, CMake, Unity, CTest).
 *
 * This is the dummy test of Phase 2. It tests no firmware behaviour.
 */
#include <stdint.h>

#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_unity_is_working(void)
{
    TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

static void test_fixed_width_types_have_expected_size(void)
{
    TEST_ASSERT_EQUAL_UINT(1u, (unsigned)sizeof(uint8_t));
    TEST_ASSERT_EQUAL_UINT(2u, (unsigned)sizeof(uint16_t));
    TEST_ASSERT_EQUAL_UINT(4u, (unsigned)sizeof(uint32_t));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_unity_is_working);
    RUN_TEST(test_fixed_width_types_have_expected_size);
    return UNITY_END();
}
