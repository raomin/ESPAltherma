#include <unity.h>
#include "../../src/homeassistant.cpp"

void test_compact_extra_metadata_preserves_discovery()
{
    const HaSensor sensors[] = {
        {"Stats: Heat output (kW)", ESP_SENSOR_POWER_KW},
        {"Unit meter: Heat produced (kWh)", ESP_SENSOR_ENERGY_KWH},
        {"E0: INV current (A)", -1, 105},
        {"Unit model", ESP_SENSOR_TEXT},
        {"Unit meter: Compressor hours (h)", ESP_SENSOR_HOURS},
    };
    for (const HaSensor &sensor : sensors)
    {
        LabelDef original(-1, -1, sensor.convid, -1, sensor.dataType, sensor.label);
        std::string expected = makeSensorJson(original, false);
        TEST_ASSERT_EQUAL_STRING(expected.c_str(), makeSensorJson(sensor.definition(), false).c_str());
    }
    TEST_ASSERT_TRUE(sizeof(HaSensor) < sizeof(LabelDef));
}

void test_extra_components_are_removed_when_disabled()
{
    const HaSensor sensor[] = {{"Stats: Heat output (kW)", ESP_SENSOR_POWER_KW}};
    haExtraLabels = sensor;
    haExtraCount = 1;
    HaAnnounced announced;
    announced.sent(haComponents(nullptr, 0));
    haExtraCount = 0;
    std::vector<HaComponent> removed = announced.removed(haComponents(nullptr, 0));
    TEST_ASSERT_EQUAL(1, removed.size());
    TEST_ASSERT_EQUAL_STRING(makeJsonKey(sensor[0].label).c_str(), removed[0].key.c_str());
    haExtraLabels = nullptr;
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_compact_extra_metadata_preserves_discovery);
    RUN_TEST(test_extra_components_are_removed_when_disabled);
    return UNITY_END();
}
