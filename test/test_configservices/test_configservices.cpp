#include <unity.h>
#include "../config_native.h"
#include "configservices.h"
void setUp(){}void tearDown(){}
void test_debug_gateway_enabling_and_disabling_request_new_subscriptions()
{
    AppConfig before,after;configDefaults(before);after=before;JsonDocument doc;
    deserializeJson(doc,"{\"output\":{\"debug_serial\":true}}");configFromJson(after,doc.as<JsonVariantConst>());
    TEST_ASSERT_TRUE(configServiceChanges(before,after).mqtt);TEST_ASSERT_FALSE(configServiceChanges(before,after).clock);
    before=after;deserializeJson(doc,"{\"output\":{\"debug_serial\":false}}");configFromJson(after,doc.as<JsonVariantConst>());
    TEST_ASSERT_TRUE(configServiceChanges(before,after).mqtt);
    before=after;TEST_ASSERT_FALSE(configServiceChanges(before,after).mqtt);
}
void test_time_server_change_reconfigures_clock_in_both_directions()
{
    AppConfig before,after;configDefaults(before);after=before;JsonDocument doc;
    for(const char *server:{"192.168.1.2",""})
    {
        doc["ntp"]=server;configFromJson(after,doc.as<JsonVariantConst>());auto changes=configServiceChanges(before,after);
        TEST_ASSERT_TRUE(changes.clock);TEST_ASSERT_FALSE(changes.mqtt);
        int calls=0;configureTimeServers(after.ntpServer,[&](long off,int dst,const char *a,const char *b,const char *c){
            calls++;TEST_ASSERT_EQUAL(0,off);TEST_ASSERT_EQUAL(0,dst);TEST_ASSERT_NULL(c);
            TEST_ASSERT_EQUAL_STRING(server[0]?server:"pool.ntp.org",a);
            if(server[0]) TEST_ASSERT_NULL(b);else TEST_ASSERT_EQUAL_STRING("time.google.com",b);
        });TEST_ASSERT_EQUAL(1,calls);before=after;
    }
    TEST_ASSERT_FALSE(configServiceChanges(before,after).clock);
}
void test_unrelated_settings_leave_network_services_running()
{
    AppConfig before,after;configDefaults(before);after=before;JsonDocument doc;
    deserializeJson(doc,"{\"stats\":{\"voltage\":240},\"output\":{\"no_log\":true}}");configFromJson(after,doc.as<JsonVariantConst>());
    auto changes=configServiceChanges(before,after);TEST_ASSERT_FALSE(changes.mqtt);TEST_ASSERT_FALSE(changes.clock);
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_debug_gateway_enabling_and_disabling_request_new_subscriptions);RUN_TEST(test_time_server_change_reconfigures_clock_in_both_directions);RUN_TEST(test_unrelated_settings_leave_network_services_running);return UNITY_END();}
