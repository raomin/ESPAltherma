#include <unity.h>

#include "haremoval.h"

static const HaComponent TANK = {"dhw_tank_temp_r5t", "sensor"};
static const HaComponent ROOM = {"indoor_ambient_temp_r1t", "sensor"};
static const HaComponent VALVE = {"3way_valveondhwoffspace", "binary_sensor"};

void test_first_discovery_removes_nothing(void)
{
    HaAnnounced a;
    TEST_ASSERT_EQUAL_size_t(0, a.removed({TANK, ROOM}).size());
}

void test_deselected_values_are_removed_once(void)
{
    HaAnnounced a;
    a.sent({TANK, ROOM, VALVE});
    std::vector<HaComponent> gone = a.removed({TANK});
    TEST_ASSERT_EQUAL_size_t(2, gone.size());
    TEST_ASSERT_EQUAL_STRING("indoor_ambient_temp_r1t", gone[0].key.c_str());
    TEST_ASSERT_EQUAL_STRING(",\"indoor_ambient_temp_r1t\":{\"p\":\"sensor\"}", haRemovalJson(gone[0]).c_str());
    TEST_ASSERT_EQUAL_STRING(",\"3way_valveondhwoffspace\":{\"p\":\"binary_sensor\"}", haRemovalJson(gone[1]).c_str());
    // sent: the next discovery leaves them out
    a.sent({TANK});
    TEST_ASSERT_EQUAL_size_t(0, a.removed({TANK}).size());
}

void test_unsent_discovery_keeps_the_removals(void)
{
    HaAnnounced a;
    a.sent({TANK, ROOM});
    TEST_ASSERT_EQUAL_size_t(1, a.removed({TANK}).size());
    // not sent (broker gone): the next try removes it again
    TEST_ASSERT_EQUAL_size_t(1, a.removed({TANK}).size());
}

void test_added_values_remove_nothing(void)
{
    HaAnnounced a;
    a.sent({TANK});
    TEST_ASSERT_EQUAL_size_t(0, a.removed({TANK, ROOM, VALVE}).size());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_discovery_removes_nothing);
    RUN_TEST(test_deselected_values_are_removed_once);
    RUN_TEST(test_unsent_discovery_keeps_the_removals);
    RUN_TEST(test_added_values_remove_nothing);
    return UNITY_END();
}
