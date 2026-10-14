#include <string.h>
#include <unity.h>

#include "topics.h"

void setUp(void) {}
void tearDown(void) {}

// The default base gives exactly the strings earlier versions published: nothing moves for existing users
void test_default_base_keeps_the_legacy_strings(void)
{
    MqttTopics t;
    t.init("espaltherma");
    TEST_ASSERT_TRUE(t.legacy);
    TEST_ASSERT_EQUAL_STRING("espaltherma", t.id);
    TEST_ASSERT_EQUAL_STRING("espaltherma/ATTR", t.attr);
    TEST_ASSERT_EQUAL_STRING("espaltherma/LWT", t.lwt);
    TEST_ASSERT_EQUAL_STRING("espaltherma/log", t.log);
    TEST_ASSERT_EQUAL_STRING("espaltherma/detect/run", t.detectRun);
    TEST_ASSERT_EQUAL_STRING("espaltherma/sg/set", t.sgSet);
    TEST_ASSERT_EQUAL_STRING("espaltherma/SAFETY_STATE", t.safetyState);
    TEST_ASSERT_EQUAL_STRING("espaltherma/serialTX", t.serialTx);
    TEST_ASSERT_EQUAL_STRING("espaltherma/dhw_target/set", t.ctlSet[0]);
    TEST_ASSERT_EQUAL_STRING("espaltherma/lwt_target/attributes", t.ctlAttrs[1]);
    TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/espAltherma/config", t.cfgSensor);
    TEST_ASSERT_EQUAL_STRING("homeassistant/switch/espAltherma/config", t.cfgSwitch);
    TEST_ASSERT_EQUAL_STRING("homeassistant/select/espAltherma/sg/config", t.cfgSg);
    TEST_ASSERT_EQUAL_STRING("homeassistant/switch/espAltherma/safety/config", t.cfgSafety);
    TEST_ASSERT_EQUAL_STRING("homeassistant/number/espAltherma/dhw_target/config", t.cfgCtl[0]);
    TEST_ASSERT_EQUAL_STRING("homeassistant/device/espaltherma-mqtt-discovery/config", t.cfgDevice);
    TEST_ASSERT_EQUAL_STRING("espaltherma-mqtt-discovery", t.deviceId);
    TEST_ASSERT_EQUAL_STRING("Daikin Altherma via ESPAltherma", t.deviceName);
    TEST_ASSERT_EQUAL_STRING("ESPAltherma", t.relayDeviceId);
    TEST_ASSERT_EQUAL_STRING("espaltherma_dhw_target", t.ctlUniq[0]);
    TEST_ASSERT_EQUAL_STRING("number.espaltherma_hot_water_target", t.ctlEntity[0]);
    TEST_ASSERT_EQUAL_STRING("number.espaltherma_leaving_water_target", t.ctlEntity[1]);
}

// A second board on the same broker: its own topics, its own device and entity ids in Home Assistant
void test_other_base_gives_its_own_names(void)
{
    MqttTopics t;
    t.init("espaltherma-odu");
    TEST_ASSERT_FALSE(t.legacy);
    TEST_ASSERT_EQUAL_STRING("espaltherma_odu", t.id);
    TEST_ASSERT_EQUAL_STRING("espaltherma-odu/ATTR", t.attr);
    TEST_ASSERT_EQUAL_STRING("espaltherma-odu/LWT", t.lwt);
    TEST_ASSERT_EQUAL_STRING("espaltherma-odu/dhw_target/set", t.ctlSet[0]);
    TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/espaltherma_odu/config", t.cfgSensor);
    TEST_ASSERT_EQUAL_STRING("homeassistant/device/espaltherma_odu-mqtt-discovery/config", t.cfgDevice);
    TEST_ASSERT_EQUAL_STRING("espaltherma_odu-mqtt-discovery", t.deviceId);
    TEST_ASSERT_EQUAL_STRING("Daikin Altherma via ESPAltherma (espaltherma-odu)", t.deviceName);
    TEST_ASSERT_EQUAL_STRING("ESPAltherma-espaltherma_odu", t.relayDeviceId);
    TEST_ASSERT_EQUAL_STRING("espaltherma_odu_lwt_target", t.ctlUniq[1]);
    TEST_ASSERT_EQUAL_STRING("number.espaltherma_odu_hot_water_target", t.ctlEntity[0]);
}

// Identifiers: Home Assistant wants lower case letters, digits and underscores
void test_identifier_from_any_base(void)
{
    MqttTopics t;
    t.init("ESPAlthermaIDU");
    TEST_ASSERT_EQUAL_STRING("espalthermaidu", t.id);
    TEST_ASSERT_EQUAL_STRING("ESPAlthermaIDU/ATTR", t.attr);
    t.init("home/heat.pump/2");
    TEST_ASSERT_EQUAL_STRING("home_heat_pump_2", t.id);
    TEST_ASSERT_EQUAL_STRING("home/heat.pump/2/LWT", t.lwt);
    t.init("");
    TEST_ASSERT_TRUE(t.legacy);
}

// What the user types is cleaned: no spaces, no wildcards, no slashes at the ends, empty is the default
void test_sanitise_base(void)
{
    char b[33];
    strcpy(b, " espaltherma odu/ ");
    TEST_ASSERT_TRUE(topicsSanitizeBase(b, sizeof(b)));
    TEST_ASSERT_EQUAL_STRING("espalthermaodu", b);
    strcpy(b, "/a/#/+b/");
    topicsSanitizeBase(b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("a//b", b);
    strcpy(b, "///");
    topicsSanitizeBase(b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("espaltherma", b);
    strcpy(b, "espaltherma-idu");
    TEST_ASSERT_FALSE(topicsSanitizeBase(b, sizeof(b)));
    TEST_ASSERT_EQUAL_STRING("espaltherma-idu", b);
}

// Two boards left on the default client id would drop each other's connection: it follows the base, unless chosen
void test_client_id(void)
{
    MqttTopics t;
    t.init("espaltherma", "ESPAltherma-dev");
    TEST_ASSERT_EQUAL_STRING("ESPAltherma-dev", t.clientId);
    t.init("espaltherma-odu", "ESPAltherma-dev");
    TEST_ASSERT_EQUAL_STRING("ESPAltherma-espaltherma_odu", t.clientId);
    t.init("espaltherma-odu", "");
    TEST_ASSERT_EQUAL_STRING("ESPAltherma-espaltherma_odu", t.clientId);
    t.init("espaltherma-odu", "my-odu");
    TEST_ASSERT_EQUAL_STRING("my-odu", t.clientId);
    t.init("espaltherma", "my-board");
    TEST_ASSERT_EQUAL_STRING("my-board", t.clientId);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_base_keeps_the_legacy_strings);
    RUN_TEST(test_other_base_gives_its_own_names);
    RUN_TEST(test_identifier_from_any_base);
    RUN_TEST(test_sanitise_base);
    RUN_TEST(test_client_id);
    return UNITY_END();
}
