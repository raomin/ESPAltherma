#include <unity.h>
#include <ArduinoJson.h>
#include <string>
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
// The room counted for the values holds their message, texts and numbers. With texts only and as a table it is the
// least that does: one byte less is refused (an object has the two bytes of the table's brackets to spare)
void test_the_room_counted_for_the_values_holds_them_at_their_longest()
{
    const char *names[]={"Operation Mode","DHW tank temp. (R5T)","INV frequency (rps)","A name as long as the longest of the definitions (R1T-R2T) [unit]"};
    const char longest[]="01234567890123456789012345678"; // 29 characters: LabelDef::asString holds 30
    size_t room=MQTT_JSON_FRAME_ROOM;
    for(const char *name:names) room+=mqttJsonValueRoom(name,longest);
    for(bool table:{false,true})
    {
        std::string storage(room+8,'x');char *out=&storage[0];
        strcpy(out,table?"[{":"{");bool failed=false;
        for(const char *name:names) TEST_ASSERT_TRUE(mqttJsonAppend(out,room,failed,"\"%s\":\"%s\",",name,longest));
        TEST_ASSERT_TRUE(mqttJsonFinish(out,room,table,failed));
        TEST_ASSERT_EQUAL(table?room-1:room-3,strlen(out));
        TEST_ASSERT_EQUAL_STRING_LEN("xxxxxxxx",out+room,8);
        JsonDocument doc;TEST_ASSERT_TRUE(deserializeJson(doc,out)==DeserializationError::Ok);
    }
    std::string storage(room+8,'x');char *out=&storage[0];
    strcpy(out,"[{");bool failed=false;
    for(const char *name:names) mqttJsonAppend(out,room-1,failed,"\"%s\":\"%s\",",name,longest);
    TEST_ASSERT_FALSE(mqttJsonFinish(out,room-1,true,failed));

    const char *numbers[]={"45.5","-10","0","230"};size_t numberRoom=MQTT_JSON_FRAME_ROOM;
    for(size_t i=0;i<4;i++) numberRoom+=mqttJsonValueRoom(names[i],numbers[i]);
    strcpy(out,"[{");failed=false;
    for(size_t i=0;i<4;i++) TEST_ASSERT_TRUE(mqttJsonAppend(out,numberRoom,failed,"\"%s\":%s,",names[i],numbers[i]));
    TEST_ASSERT_TRUE(mqttJsonFinish(out,numberRoom,true,failed));
}
// The room is taken in steps, so that it stays the same while the texts of the values move by a few bytes
void test_the_room_is_rounded_up_to_a_step()
{
    TEST_ASSERT_EQUAL(1024,mqttJsonRoomRounded(1));
    TEST_ASSERT_EQUAL(1024,mqttJsonRoomRounded(1024));
    TEST_ASSERT_EQUAL(2048,mqttJsonRoomRounded(1025));
    TEST_ASSERT_EQUAL(mqttJsonRoomRounded(5273),mqttJsonRoomRounded(5290));
    TEST_ASSERT_EQUAL(6144,mqttJsonRoomRounded(5273));
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_the_room_is_rounded_up_to_a_step);RUN_TEST(test_the_room_counted_for_the_values_holds_them_at_their_longest);RUN_TEST(test_object_and_table_finalize_to_valid_json);RUN_TEST(test_oversize_payload_is_rejected_without_overrunning_the_table_closer);RUN_TEST(test_saturated_legacy_append_and_tiny_buffers_are_not_published);return UNITY_END();}
