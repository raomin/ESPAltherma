#include <unity.h>
#include "../../src/homeassistant.cpp"

void test_physical_sensors_require_fresh_present_values_and_lwt()
{
    haValueExpirySeconds = 90;
    const LabelDef label(0x20,0,105,2,1,"Outdoor temperature");
    std::string json = makeSensorJson(label, false);
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"avty_mode\":\"all\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "espaltherma/LWT"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "'Outdoor temperature' in value_json"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"exp_aft\":90"));
}

void test_computed_and_diagnostic_sensors_do_not_use_register_availability()
{
    const LabelDef label(-1,-1,-1,-1,ESP_SENSOR_POWER_KW,"Stats: Heat output (kW)");
    std::string json = makeSensorJson(label, false);
    TEST_ASSERT_NULL(strstr(json.c_str(), "\"exp_aft\""));
    TEST_ASSERT_NULL(strstr(json.c_str(), "\"avty\""));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_physical_sensors_require_fresh_present_values_and_lwt);
    RUN_TEST(test_computed_and_diagnostic_sensors_do_not_use_register_availability);
    return UNITY_END();
}
