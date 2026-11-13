#include <unity.h>
#include <ArduinoJson.h>
#include "sgdiscovery.h"
void setUp(){}void tearDown(){}
void test_board_identity_changes_without_changing_commands_or_entities()
{
    MqttTopics topics;topics.init("espaltherma");
    for(const char *board:{"esp32s3","m5stickcplus2","esp32c6","wt32-eth01"})
    {
        char json[1400];int n=smartGridDiscovery(json,sizeof(json),topics,board);
        TEST_ASSERT_TRUE(n>0 && n<(int)sizeof(json));JsonDocument doc;
        TEST_ASSERT_TRUE(deserializeJson(doc,json)==DeserializationError::Ok);
        TEST_ASSERT_EQUAL_STRING(board,doc["device"]["model"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(topics.sgSet,doc["command_topic"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING(topics.sgState,doc["state_topic"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING("espaltherma_sg",doc["unique_id"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING("Free Running",doc["options"][0].as<const char*>());
        TEST_ASSERT_NOT_NULL(strstr(doc["command_template"].as<const char*>(),"{% if value == 'Free Running' %}"));
    }
}
void test_maximum_base_topic_fits_and_reports_small_buffer_truncation()
{
    MqttTopics topics;topics.init("12345678901234567890123456789012");char json[1400];
    int n=smartGridDiscovery(json,sizeof(json),topics,"m5stack-tough");TEST_ASSERT_TRUE(n>0 && n<(int)sizeof(json));
    char tiny[8];TEST_ASSERT_EQUAL(n,smartGridDiscovery(tiny,sizeof(tiny),topics,"m5stack-tough"));
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_board_identity_changes_without_changing_commands_or_entities);RUN_TEST(test_maximum_base_topic_fits_and_reports_small_buffer_truncation);return UNITY_END();}
