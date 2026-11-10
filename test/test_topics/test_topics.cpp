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
    TEST_ASSERT_EQUAL_STRING("homeassistant/update/espAltherma/firmware/config", t.cfgUpdate);
    TEST_ASSERT_EQUAL_STRING("espaltherma/update", t.updateTopic);
    TEST_ASSERT_EQUAL_STRING("update.espaltherma_firmware", t.updateEntity);
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
    TEST_ASSERT_EQUAL_STRING("homeassistant/update/espaltherma_odu/firmware/config", t.cfgUpdate);
    TEST_ASSERT_EQUAL_STRING("espaltherma-odu/update", t.updateTopic);
    TEST_ASSERT_EQUAL_STRING("espaltherma_odu_firmware", t.updateUniq);
    TEST_ASSERT_EQUAL_STRING("update.espaltherma_odu_firmware", t.updateEntity);
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

// The longest base topic (32 characters): no topic, identifier or discovery topic is cut, and the topics the board
// listens to differ from the ones it publishes on (cut at 48 characters, power_limit/on/set and .../on/state were
// the same topic: the board answered its own state)
#define NOT_CUT(field) TEST_ASSERT_TRUE_MESSAGE(strlen(t.field) < sizeof(t.field) - 1, #field)
void test_longest_base_cuts_nothing(void)
{
    const char *base = "heatpump/ground-floor/altherma-3"; // 32 characters
    TEST_ASSERT_EQUAL(TOPICS_BASE_MAX, strlen(base));
    MqttTopics t;
    t.init(base);
    TEST_ASSERT_EQUAL_STRING(base, t.base);
    NOT_CUT(attr); NOT_CUT(lwt); NOT_CUT(log); NOT_CUT(detect); NOT_CUT(detectRun); NOT_CUT(power); NOT_CUT(state);
    NOT_CUT(sgSet); NOT_CUT(sgState); NOT_CUT(safety); NOT_CUT(safetyState); NOT_CUT(serialTx); NOT_CUT(serialRx);
    for (int k = 0; k < 2; k++)
    {
        NOT_CUT(ctlSet[k]); NOT_CUT(ctlState[k]); NOT_CUT(ctlAttrs[k]); NOT_CUT(cfgCtl[k]); NOT_CUT(cfgRestore[k]);
        NOT_CUT(ctlUniq[k]); NOT_CUT(ctlEntity[k]); NOT_CUT(restoreUniq[k]); NOT_CUT(restoreEntity[k]);
    }
    NOT_CUT(limitSet); NOT_CUT(limitState); NOT_CUT(limitAttrs); NOT_CUT(limitOnSet); NOT_CUT(limitOnState);
    NOT_CUT(cfgSensor); NOT_CUT(cfgSwitch); NOT_CUT(cfgSg); NOT_CUT(cfgSafety); NOT_CUT(cfgDevice); NOT_CUT(cfgLimit);
    NOT_CUT(cfgLimitOn); NOT_CUT(cfgUpdate); NOT_CUT(deviceId); NOT_CUT(deviceName); NOT_CUT(relayDeviceId);
    NOT_CUT(relayDeviceName); NOT_CUT(limitUniq); NOT_CUT(limitEntity); NOT_CUT(limitOnUniq); NOT_CUT(limitOnEntity);
    NOT_CUT(updateTopic); NOT_CUT(updateUniq); NOT_CUT(updateEntity); NOT_CUT(clientId);
    TEST_ASSERT_EQUAL_STRING("heatpump/ground-floor/altherma-3/power_limit/attributes", t.limitAttrs);
    TEST_ASSERT_EQUAL_STRING("heatpump/ground-floor/altherma-3/power_limit/on/set", t.limitOnSet);
    TEST_ASSERT_EQUAL_STRING("heatpump/ground-floor/altherma-3/power_limit/on/state", t.limitOnState);
    TEST_ASSERT_EQUAL_STRING("button.heatpump_ground_floor_altherma_3_leaving_water_curve_restore", t.restoreEntity[1]);
    TEST_ASSERT_EQUAL_STRING("Daikin Altherma via ESPAltherma (heatpump/ground-floor/altherma-3)", t.deviceName);
    // what the board subscribes to is never what it publishes on
    const char *listens[] = {t.power, t.detectRun, t.sgSet, t.safety, t.serialTx, t.ctlSet[0], t.ctlSet[1], t.limitSet, t.limitOnSet};
    const char *says[] = {t.attr, t.lwt, t.log, t.detect, t.state, t.sgState, t.safetyState, t.serialRx, t.ctlState[0], t.ctlState[1],
                          t.ctlAttrs[0], t.ctlAttrs[1], t.limitState, t.limitAttrs, t.limitOnState, t.updateTopic};
    for (const char *l : listens)
        for (const char *p : says)
            TEST_ASSERT_TRUE_MESSAGE(strcmp(l, p) != 0, l);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_base_keeps_the_legacy_strings);
    RUN_TEST(test_other_base_gives_its_own_names);
    RUN_TEST(test_identifier_from_any_base);
    RUN_TEST(test_sanitise_base);
    RUN_TEST(test_client_id);
    RUN_TEST(test_longest_base_cuts_nothing);
    return UNITY_END();
}
