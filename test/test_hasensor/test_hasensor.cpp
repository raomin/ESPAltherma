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
        {"Boiler water pressure (bar)", ESP_SENSOR_BAR},
    };
    for (const HaSensor &sensor : sensors)
    {
        LabelDef original(-1, -1, sensor.convid, -1, sensor.dataType, sensor.label);
        std::string expected = makeSensorJson(original, false);
        TEST_ASSERT_EQUAL_STRING(expected.c_str(), makeSensorJson(sensor.definition(), false).c_str());
    }
    TEST_ASSERT_TRUE(sizeof(HaSensor) < sizeof(LabelDef));
    // the boiler's water pressure: a measurement in bar, "None" while the board has no value
    std::string bar = makeSensorJson(HaSensor("Boiler water pressure (bar)", ESP_SENSOR_BAR).definition(), false);
    TEST_ASSERT_NOT_NULL(strstr(bar.c_str(), "\"dev_cla\":\"pressure\""));
    TEST_ASSERT_NOT_NULL(strstr(bar.c_str(), "\"unit_of_meas\":\"bar\""));
    TEST_ASSERT_NOT_NULL(strstr(bar.c_str(), "\"stat_cla\":\"measurement\""));
    TEST_ASSERT_NOT_NULL(strstr(bar.c_str(), "is not none else None"));
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
