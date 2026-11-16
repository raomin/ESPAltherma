#include <unity.h>
#include "relays.h"

void setUp(void) {}
void tearDown(void) {}

// Heating and cooling demand are never on together: one on releases the other, off touches nothing else
void test_heating_and_cooling_demand_are_never_on_together(void)
{
    bool heat = false, cool = false;
    TEST_ASSERT_FALSE(demandSwitch(heat, cool, true));
    TEST_ASSERT_TRUE(heat);
    TEST_ASSERT_FALSE(cool);
    TEST_ASSERT_TRUE(demandSwitch(cool, heat, true)); // heating was on: released
    TEST_ASSERT_TRUE(cool);
    TEST_ASSERT_FALSE(heat);
    TEST_ASSERT_FALSE(demandSwitch(cool, heat, true)); // again: nothing to release
    TEST_ASSERT_TRUE(demandSwitch(heat, cool, true));
    TEST_ASSERT_TRUE(heat);
    TEST_ASSERT_FALSE(cool);
    TEST_ASSERT_FALSE(demandSwitch(cool, heat, false)); // cooling off leaves heating on
    TEST_ASSERT_TRUE(heat);
    TEST_ASSERT_FALSE(demandSwitch(heat, cool, false));
    TEST_ASSERT_FALSE(heat);
    TEST_ASSERT_FALSE(cool);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_heating_and_cooling_demand_are_never_on_together);
    return UNITY_END();
}
