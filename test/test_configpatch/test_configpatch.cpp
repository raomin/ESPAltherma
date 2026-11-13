#include <unity.h>
#include "../config_native.h"
#include "configpatch.h"
void setUp(){}void tearDown(){}
void test_queued_settings_do_not_roll_back_intervening_runtime_updates()
{
    AppConfig live;configDefaults(live);ConfigPatchQueue queue;
    JsonDocument doc;deserializeJson(doc,"{\"hostname\":\"new-name\"}");
    TEST_ASSERT_TRUE(queue.submit(doc)==ConfigQueueResult::queued);
    live.ctlHeld[1]=30;live.ctlLimitValue=5;strcpy(live.model,"newly detected model");strcpy(live.installId,"0123456789abcdef");
    strcpy(live.detectedKey,"new key");live.telemetrySent=true;live.fixCount=1;live.fixReg[0]=0x21;
    JsonDocument *patch=queue.peek();configApplySettingsPatch(live,patch->as<JsonVariantConst>());queue.complete(patch);
    TEST_ASSERT_EQUAL_STRING("new-name",live.hostname);TEST_ASSERT_EQUAL(30,live.ctlHeld[1]);TEST_ASSERT_EQUAL_FLOAT(5,live.ctlLimitValue);
    TEST_ASSERT_EQUAL_STRING("newly detected model",live.model);TEST_ASSERT_EQUAL_STRING("new key",live.detectedKey);
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef",live.installId);TEST_ASSERT_TRUE(live.telemetrySent);TEST_ASSERT_EQUAL(1,live.fixCount);
}
void test_queue_owns_json_and_rejects_overwriting_an_in_flight_patch()
{
    ConfigPatchQueue queue;
    {JsonDocument first;first["hostname"]="first";TEST_ASSERT_TRUE(queue.submit(first)==ConfigQueueResult::queued);}
    JsonDocument second;second["hostname"]="second";TEST_ASSERT_TRUE(queue.submit(second)==ConfigQueueResult::busy);
    TEST_ASSERT_EQUAL_STRING("first",(*queue.peek())["hostname"].as<const char*>());
    queue.complete(queue.peek());TEST_ASSERT_FALSE(queue.busy());
    JsonDocument third;third["hostname"]="third";TEST_ASSERT_TRUE(queue.submit(third)==ConfigQueueResult::queued);
    TEST_ASSERT_EQUAL_STRING("third",(*queue.peek())["hostname"].as<const char*>());
}
void test_explicit_model_change_resets_corrections_unless_patch_supplies_them()
{
    AppConfig live;configDefaults(live);strcpy(live.model,"old");live.fixCount=1;JsonDocument doc;
    deserializeJson(doc,"{\"hp\":{\"model\":\"new\"}}");configApplySettingsPatch(live,doc.as<JsonVariantConst>());TEST_ASSERT_EQUAL(0,live.fixCount);
    deserializeJson(doc,"{\"hp\":{\"model\":\"other\",\"layout_fixes\":[{\"reg\":33,\"model\":\"correction\"}]}}");
    configApplySettingsPatch(live,doc.as<JsonVariantConst>());TEST_ASSERT_EQUAL(1,live.fixCount);TEST_ASSERT_EQUAL(33,live.fixReg[0]);
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_queued_settings_do_not_roll_back_intervening_runtime_updates);RUN_TEST(test_queue_owns_json_and_rejects_overwriting_an_in_flight_patch);RUN_TEST(test_explicit_model_change_resets_corrections_unless_patch_supplies_them);return UNITY_END();}
