#include <unity.h>
#include <ArduinoJson.h>
#define ESPALTHERMA_VERSION "2.0.0-test.42"
#include "../../src/homeassistant.cpp"
void setUp(){haExtraCount=0;haExtraLabels=nullptr;}
void tearDown(){}
void test_discovery_uses_build_version_and_configured_hostname()
{
    haHostname="heat-pump";std::string json;
    auto sink=[](void *p,const char *s,size_t n){static_cast<std::string*>(p)->append(s,n);};
    size_t length=streamDeviceDiscoveryPayload(nullptr,0,sink,&json);
    TEST_ASSERT_EQUAL(length,json.size());JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc,json)==DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING(ESPALTHERMA_VERSION,doc["o"]["sw"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("ESPAltherma " ESPALTHERMA_VERSION,doc["dev"]["sw_version"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("http://heat-pump.local/",doc["dev"]["configuration_url"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("espaltherma-mqtt-discovery",doc["dev"]["ids"].as<const char*>());
}
void test_hostname_metadata_is_json_escaped()
{
    haHostname="test\"host";std::string json;
    streamDeviceDiscoveryPayload(nullptr,0,[](void *p,const char *s,size_t n){static_cast<std::string*>(p)->append(s,n);},&json);
    JsonDocument doc;TEST_ASSERT_TRUE(deserializeJson(doc,json)==DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING("http://test\"host.local/",doc["dev"]["configuration_url"].as<const char*>());
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_discovery_uses_build_version_and_configured_hostname);RUN_TEST(test_hostname_metadata_is_json_escaped);return UNITY_END();}
