#include <unity.h>
#include <ArduinoJson.h>
#include <vector>
#include <atomic>
#include <string>
#include "../../src/homeassistant.cpp" // production serializers, without Arduino/network dependencies
void setUp(){haExtraLabels=nullptr;haExtraCount=0;haBinaryAsNumber=false;haDeviceModel="Altherma";}
void tearDown(){}
static void append(void *ctx,const char *p,size_t n){static_cast<std::string*>(ctx)->append(p,n);}
void test_discovery_snapshot_matches_existing_bytes_and_survives_source_reload_mid_send()
{
    std::vector<LabelDef> selected={{0x20,0,105,2,1,"Outdoor temperature"},{0x10,1,304,1,-1,"Defrost"}};
    selected[0].name="Temp. \"ext\"";
    std::vector<LabelDef> extras={{-1,-1,-1,-1,ESP_SENSOR_POWER_KW,"Heat output"}};
    haExtraLabels=extras.data();haExtraCount=extras.size();char model[]="EHBX08EF6V";haDeviceModel=model;
    std::vector<HaComponent> removed={{"old_sensor","sensor"}};
    std::string legacy;streamDeviceDiscoveryPayload(selected.data(),selected.size(),append,&legacy,&removed);
    HaDiscoverySnapshot snapshot;TEST_ASSERT_TRUE(snapshot.capture(selected.data(),selected.size(),extras.data(),extras.size(),model));
    TEST_ASSERT_EQUAL(0x20,snapshot.labels[0].definition().registryID);
    TEST_ASSERT_EQUAL(-1,snapshot.labels[2].definition().registryID);
    size_t measured=streamDeviceDiscoverySnapshot(snapshot,nullptr,nullptr,&removed);
    struct Context {std::string output;std::vector<LabelDef> *selected,*extras;bool changed=false;char *model;};
    Context context;context.selected=&selected;context.extras=&extras;context.model=model;
    auto sink=[](void *p,const char *data,size_t n){
        auto &c=*static_cast<Context*>(p);c.output.append(data,n);
        if(!c.changed) {c.changed=true;c.selected->clear();c.selected->shrink_to_fit();c.extras->clear();c.extras->shrink_to_fit();
            haExtraLabels=nullptr;haExtraCount=0;strcpy(c.model,"changed");}
    };
    TEST_ASSERT_EQUAL(measured,streamDeviceDiscoverySnapshot(snapshot,sink,&context,&removed));
    TEST_ASSERT_EQUAL(measured,context.output.size());TEST_ASSERT_EQUAL_STRING(legacy.c_str(),context.output.c_str());
    JsonDocument doc;TEST_ASSERT_TRUE(deserializeJson(doc,context.output)==DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING("EHBX08EF6V",doc["dev"]["mdl"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Temp. \"ext\"",doc["cmps"]["outdoor_temperature"]["name"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("binary_sensor",doc["cmps"]["defrost"]["p"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("sensor",doc["cmps"]["heat_output"]["p"].as<const char*>());
    auto components=haSnapshotComponents(snapshot);TEST_ASSERT_EQUAL(3,components.size());
}
void test_one_value_sink_runs_without_lock_and_owns_its_copied_value()
{
    LabelDef defs[]={{0x20,0,105,2,1,"out"},{0x30,0,152,1,-1,"inv"}};
    strcpy(defs[0].asString,"2");strcpy(defs[1].asString,"40");
    LabelDef *labels=defs;size_t count=2;char regs[32]={0x20,0x30,(char)0xFF};std::atomic<bool> ok[32];ok[0]=true;ok[1]=false;uint32_t generation=1;
    bool locked=false;int sent=0;
    TEST_ASSERT_TRUE(publishOneValueSnapshots(labels,count,regs,ok,generation,[&](){TEST_ASSERT_FALSE(locked);locked=true;},[&](){TEST_ASSERT_TRUE(locked);locked=false;},[&](const MqttReading &r){
        TEST_ASSERT_FALSE(locked);strcpy(defs[0].asString,"changed during network write");
        TEST_ASSERT_EQUAL_STRING("2",r.value);TEST_ASSERT_EQUAL_STRING("out",r.label);sent++;
    }));
    TEST_ASSERT_EQUAL(1,sent);TEST_ASSERT_FALSE(locked);
}
void test_label_reload_during_publish_stops_without_reading_freed_selection()
{
    std::vector<LabelDef> defs={{0x20,0,105,2,1,"out"},{0x20,8,105,2,1,"coil"}};
    strcpy(defs[0].asString,"2");LabelDef *labels=defs.data();size_t count=defs.size();uint32_t generation=1;
    char regs[32]={0x20,(char)0xFF};bool ok[32]={true};bool locked=false;int sent=0;
    TEST_ASSERT_FALSE(publishOneValueSnapshots(labels,count,regs,ok,generation,[&](){locked=true;},[&](){locked=false;},[&](const MqttReading &){
        TEST_ASSERT_FALSE(locked);defs.clear();defs.shrink_to_fit();labels=nullptr;count=0;generation++;sent++;
    }));
    TEST_ASSERT_EQUAL(1,sent);TEST_ASSERT_FALSE(locked);
}
void test_snapshot_rejects_invalid_or_unbounded_sources()
{
    HaDiscoverySnapshot snapshot;TEST_ASSERT_FALSE(snapshot.capture(nullptr,225,nullptr,0,"x"));
    TEST_ASSERT_FALSE(snapshot.capture(nullptr,1,nullptr,0,"x"));
    TEST_ASSERT_TRUE(snapshot.capture(nullptr,0,nullptr,0,"x"));TEST_ASSERT_EQUAL(0,snapshot.count);
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_discovery_snapshot_matches_existing_bytes_and_survives_source_reload_mid_send);RUN_TEST(test_one_value_sink_runs_without_lock_and_owns_its_copied_value);RUN_TEST(test_label_reload_during_publish_stops_without_reading_freed_selection);RUN_TEST(test_snapshot_rejects_invalid_or_unbounded_sources);return UNITY_END();}
