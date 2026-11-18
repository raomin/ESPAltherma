#include <unity.h>
#include <ArduinoJson.h>
#define ESPALTHERMA_VERSION "2.0.0-test.42"
#include "../../src/homeassistant.cpp"
void setUp(){haExtraCount=0;haExtraLabels=nullptr;}
void tearDown(){}
static std::string discovery(const char *hostname)
{
    haHostname=hostname;std::string json;
    auto sink=[](void *p,const char *s,size_t n){static_cast<std::string*>(p)->append(s,n);};
    size_t length=streamDeviceDiscoveryPayload(nullptr,0,sink,&json);
    TEST_ASSERT_EQUAL(length,json.size());
    return json;
}
void test_discovery_uses_build_version_and_configured_hostname()
{
    std::string json=discovery("heat-pump");JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc,json)==DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING(ESPALTHERMA_VERSION,doc["o"]["sw"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("ESPAltherma " ESPALTHERMA_VERSION,doc["dev"]["sw_version"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("http://heat-pump.local/",doc["dev"]["configuration_url"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("espaltherma-mqtt-discovery",doc["dev"]["ids"].as<const char*>());
}
// #703: Home Assistant 2026.11 refuses an address with a space in its host, and the whole message with it
void test_a_name_that_is_not_an_address_gives_no_address()
{
    const char *names[]={"ESP32 S2 Rotex W\xC3\xA4rmepumpe","pompe a chaleur","W\xC3\xA4rmepumpe","test\"host","a:b","a/b","-pump","pump-","",
                         "a-name-longer-than-the-sixty-three-characters-a-host-label-may-have"};
    for(const char *name:names)
    {
        std::string json=discovery(name);JsonDocument doc;
        TEST_ASSERT_TRUE_MESSAGE(deserializeJson(doc,json)==DeserializationError::Ok,name);
        TEST_ASSERT_TRUE_MESSAGE(doc["dev"]["configuration_url"].isNull(),name);
        TEST_ASSERT_EQUAL_STRING("ESPAltherma " ESPALTHERMA_VERSION,doc["dev"]["sw_version"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING("espaltherma-mqtt-discovery",doc["dev"]["ids"].as<const char*>());
        TEST_ASSERT_EQUAL_STRING("espaltherma-mqtt-discovery",doc["o"]["name"].as<const char*>());
        TEST_ASSERT_FALSE(doc["cmps"].isNull());
    }
}
void test_a_name_that_is_an_address_gives_it()
{
    const char *names[]={"ESPAltherma","heat-pump","my_board","Altherma3","a"};
    for(const char *name:names)
    {
        std::string json=discovery(name);JsonDocument doc;
        TEST_ASSERT_TRUE_MESSAGE(deserializeJson(doc,json)==DeserializationError::Ok,name);
        TEST_ASSERT_EQUAL_STRING((std::string("http://")+name+".local/").c_str(),doc["dev"]["configuration_url"].as<const char*>());
    }
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_discovery_uses_build_version_and_configured_hostname);RUN_TEST(test_a_name_that_is_not_an_address_gives_no_address);RUN_TEST(test_a_name_that_is_an_address_gives_it);return UNITY_END();}
