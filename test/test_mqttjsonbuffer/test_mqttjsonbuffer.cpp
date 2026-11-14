#include <unity.h>
#include <ArduinoJson.h>
#include "mqttjsonbuffer.h"
void setUp(){}void tearDown(){}
void test_object_and_table_finalize_to_valid_json()
{
    for(bool table:{false,true})
    {
        char out[80];strcpy(out,table?"[{":"{");bool failed=false;
        TEST_ASSERT_TRUE(mqttJsonAppend(out,sizeof(out),failed,"\"Outdoor\":%g,",-10.5));
        TEST_ASSERT_TRUE(mqttJsonAppend(out,sizeof(out),failed,"\"Status\":\"%s\",","ON"));
        TEST_ASSERT_TRUE(mqttJsonFinish(out,sizeof(out),table,failed));JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc,out)==DeserializationError::Ok);
        float outdoor=table?doc[0]["Outdoor"].as<float>():doc["Outdoor"].as<float>();
        TEST_ASSERT_EQUAL_FLOAT(-10.5,outdoor);
    }
}
void test_oversize_payload_is_rejected_without_overrunning_the_table_closer()
{
    struct {char out[7120];char canary[8];} storage;memset(&storage,'x',sizeof(storage));
    strcpy(storage.out,"[{");bool failed=false;std::string longName(72,'L');
    for(int i=0;i<128;i++) mqttJsonAppend(storage.out,sizeof(storage.out),failed,"\"%s%d\":\"01234567890123456789012345678\",",longName.c_str(),i);
    TEST_ASSERT_TRUE(failed);TEST_ASSERT_FALSE(mqttJsonFinish(storage.out,sizeof(storage.out),true,failed));
    TEST_ASSERT_EQUAL_STRING_LEN("xxxxxxxx",storage.canary,8);
    strcpy(storage.out,"[{");failed=false;mqttJsonAppend(storage.out,sizeof(storage.out),failed,"\"ok\":1,");
    TEST_ASSERT_TRUE(mqttJsonFinish(storage.out,sizeof(storage.out),true,failed));TEST_ASSERT_EQUAL_STRING("[{\"ok\":1}]",storage.out);
}
void test_saturated_legacy_append_and_tiny_buffers_are_not_published()
{
    char out[16];strcpy(out,"[{");snprintf(out+2,sizeof(out)-2,"\"long-key\":%s,","12345678901234567890");
    TEST_ASSERT_FALSE(mqttJsonFinish(out,sizeof(out),true,false));
    char tiny[3]={'[','{',0};TEST_ASSERT_FALSE(mqttJsonFinish(tiny,sizeof(tiny),true,false));
    TEST_ASSERT_FALSE(mqttJsonFinish(nullptr,0,false,false));
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_object_and_table_finalize_to_valid_json);RUN_TEST(test_oversize_payload_is_rejected_without_overrunning_the_table_closer);RUN_TEST(test_saturated_legacy_append_and_tiny_buffers_are_not_published);return UNITY_END();}
