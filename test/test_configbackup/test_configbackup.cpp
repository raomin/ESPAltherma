#include <unity.h>
#include "../config_native.h"
void setUp(){}void tearDown(){}
static AppConfig running()
{
    AppConfig c;configDefaults(c);c.ctlHeld[1]=30;c.ctlOrig[1][0]=25;c.ctlOrig[1][1]=38;
    strcpy(c.ctlOrigSw,"20002203");strcpy(c.installId,"0123456789abcdef");strcpy(c.detectedKey,"current unit");
    c.ctlLimitOn=true;c.ctlLimitValue=5;c.ctlLimitOrig=2;strcpy(c.ctlLimitSw,"20002203");c.telemetrySent=true;
    strcpy(c.wifiPwd,"network-secret");return c;
}
void test_api_ignores_runtime_fields_in_legacy_backup_but_applies_settings()
{
    AppConfig c=running();JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc,R"({"hostname":"restored","control":{"lwt_target":true,"ram_only":true,"held_lwt":20,"orig_lwt_hi":50,"orig_sw":"wrong","limit_on":false,"limit_value":1,"limit_orig":0,"limit_sw":"wrong"},"hp":{"detected_key":"other unit"},"telemetry":{"install_id":"ffffffffffffffff","sent":false}})")==DeserializationError::Ok);
    configFromJson(c,doc.as<JsonVariantConst>());
    TEST_ASSERT_EQUAL_STRING("restored",c.hostname);TEST_ASSERT_TRUE(c.lwtTarget);TEST_ASSERT_TRUE(c.ctlRamOnly);
    TEST_ASSERT_EQUAL(30,c.ctlHeld[1]);TEST_ASSERT_EQUAL(25,c.ctlOrig[1][0]);TEST_ASSERT_EQUAL_STRING("20002203",c.ctlOrigSw);
    TEST_ASSERT_TRUE(c.ctlLimitOn);TEST_ASSERT_EQUAL_FLOAT(5,c.ctlLimitValue);TEST_ASSERT_EQUAL(2,c.ctlLimitOrig);
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef",c.installId);TEST_ASSERT_EQUAL_STRING("current unit",c.detectedKey);TEST_ASSERT_TRUE(c.telemetrySent);
    TEST_ASSERT_EQUAL_STRING("network-secret",c.wifiPwd);
}
void test_public_backup_omits_runtime_and_secrets()
{
    AppConfig c=running();JsonDocument doc;configToJson(c,doc,false);
    TEST_ASSERT_TRUE(doc["control"]["held_lwt"].isNull());TEST_ASSERT_TRUE(doc["control"]["orig_sw"].isNull());
    TEST_ASSERT_TRUE(doc["control"]["limit_on"].isNull());TEST_ASSERT_TRUE(doc["telemetry"]["install_id"].isNull());
    TEST_ASSERT_TRUE(doc["hp"]["detected_key"].isNull());TEST_ASSERT_TRUE(doc["wifi"]["pwd"].isNull());
    TEST_ASSERT_TRUE(doc["wifi"]["has_pwd"].as<bool>());TEST_ASSERT_FALSE(doc["control"]["power_limit"].isNull());
}
void test_internal_persistence_still_round_trips_runtime_and_passwords()
{
    AppConfig c=running(),restored;configDefaults(restored);JsonDocument doc;configToJson(c,doc,true,true);
    std::string json;serializeJson(doc,json);JsonDocument loaded;TEST_ASSERT_TRUE(deserializeJson(loaded,json)==DeserializationError::Ok);
    configFromJson(restored,loaded.as<JsonVariantConst>(),true);
    TEST_ASSERT_EQUAL(30,restored.ctlHeld[1]);TEST_ASSERT_EQUAL_STRING(c.installId,restored.installId);
    TEST_ASSERT_EQUAL_STRING(c.ctlOrigSw,restored.ctlOrigSw);TEST_ASSERT_EQUAL_STRING(c.wifiPwd,restored.wifiPwd);
    TEST_ASSERT_TRUE(restored.ctlLimitOn);TEST_ASSERT_EQUAL_FLOAT(5,restored.ctlLimitValue);TEST_ASSERT_TRUE(restored.telemetrySent);
}
// A settings form works on a copy of the configuration: what the board wrote between the copy and the save stays
void test_settings_copy_keeps_what_the_board_wrote_meanwhile()
{
    AppConfig live = running();
    AppConfig form = live; // the copy made when the form was posted
    strcpy(form.hostname, "renamed");
    form.frequency = 15000;
    // meanwhile the heat pump task holds another target, switches the limit off and learns the unit's key
    live.ctlHeld[1] = 33;
    live.ctlOrig[0][0] = 55;
    live.ctlLimitOn = false;
    live.ctlLimitValue = -1;
    live.ctlLimitOrigValue = 16;
    strcpy(live.detectedKey, "the unit, complete");
    live.telemetrySent = 0x1234;
    configKeepRuntime(form, live);
    TEST_ASSERT_EQUAL(33, form.ctlHeld[1]);
    TEST_ASSERT_EQUAL(55, form.ctlOrig[0][0]);
    TEST_ASSERT_FALSE(form.ctlLimitOn);
    TEST_ASSERT_EQUAL_FLOAT(-1, form.ctlLimitValue);
    TEST_ASSERT_EQUAL_FLOAT(16, form.ctlLimitOrigValue);
    TEST_ASSERT_EQUAL_STRING("the unit, complete", form.detectedKey);
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef", form.installId);
    TEST_ASSERT_EQUAL_UINT32(0x1234, form.telemetrySent);
    // and the form's own settings are the ones applied
    TEST_ASSERT_EQUAL_STRING("renamed", form.hostname);
    TEST_ASSERT_EQUAL(15000, form.frequency);
}
// The external sensor (R6T) is shown as the outdoor or as the room temperature, never as both (#699)
void test_external_sensor_has_one_place()
{
    AppConfig c; configDefaults(c); JsonDocument doc;
    TEST_ASSERT_FALSE(c.outdoorAux); TEST_ASSERT_FALSE(c.indoorAux);
    deserializeJson(doc, R"({"hp":{"indoor_aux":true}})"); configFromJson(c, doc.as<JsonVariantConst>());
    TEST_ASSERT_TRUE(c.indoorAux); TEST_ASSERT_FALSE(c.outdoorAux);
    // the other place chosen alone: it takes the sensor
    deserializeJson(doc, R"({"hp":{"outdoor_aux":true}})"); configFromJson(c, doc.as<JsonVariantConst>());
    TEST_ASSERT_TRUE(c.outdoorAux); TEST_ASSERT_FALSE(c.indoorAux);
    deserializeJson(doc, R"({"hp":{"indoor_aux":true}})"); configFromJson(c, doc.as<JsonVariantConst>());
    TEST_ASSERT_TRUE(c.indoorAux); TEST_ASSERT_FALSE(c.outdoorAux);
    // both asked at once on a board that had neither: the outdoor one
    configDefaults(c);
    deserializeJson(doc, R"({"hp":{"outdoor_aux":true,"indoor_aux":true}})"); configFromJson(c, doc.as<JsonVariantConst>());
    TEST_ASSERT_TRUE(c.outdoorAux); TEST_ASSERT_FALSE(c.indoorAux);
    // and the setting is in the backup
    configDefaults(c); c.indoorAux = true; JsonDocument out; configToJson(c, out, false);
    TEST_ASSERT_TRUE(out["hp"]["indoor_aux"].as<bool>()); TEST_ASSERT_FALSE(out["hp"]["outdoor_aux"].as<bool>());
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_external_sensor_has_one_place);RUN_TEST(test_settings_copy_keeps_what_the_board_wrote_meanwhile);RUN_TEST(test_api_ignores_runtime_fields_in_legacy_backup_but_applies_settings);RUN_TEST(test_public_backup_omits_runtime_and_secrets);RUN_TEST(test_internal_persistence_still_round_trips_runtime_and_passwords);return UNITY_END();}
