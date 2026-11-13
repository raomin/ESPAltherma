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
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_api_ignores_runtime_fields_in_legacy_backup_but_applies_settings);RUN_TEST(test_public_backup_omits_runtime_and_secrets);RUN_TEST(test_internal_persistence_still_round_trips_runtime_and_passwords);return UNITY_END();}
