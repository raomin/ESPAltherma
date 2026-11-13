#include <unity.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
static uint32_t clockMs;
typedef int SemaphoreHandle_t;
static const int portMAX_DELAY=0,SERIAL_8E1=0;
int xSemaphoreCreateRecursiveMutex(){return 0;}
void xSemaphoreTakeRecursive(int,int){}
void xSemaphoreGiveRecursive(int){}
uint32_t millis(){return clockMs;}
void delay(unsigned long n){clockMs+=(uint32_t)n;}
struct Log {
    void print(const char*){} void println(const char*){}
    template<typename... A> void printf(const char*,A...){}
} Serial,mqttSerial;
class HardwareSerial {
    size_t cursor=0;uint32_t start=0;
public:
    std::vector<uint8_t> response;
    uint32_t arriveAfter=0,perByte=0;
    bool echo=false;
    explicit HardwareSerial(int){}
    void flush(bool){}
    size_t write(const uint8_t *p,size_t n){cursor=0;start=clockMs;if(echo) response.insert(response.begin(),p,p+n);return n;}
    int available(){return cursor<response.size() && (uint32_t)(clockMs-start)>=arriveAfter;}
    int read(){clockMs+=perByte;return response[cursor++];}
};
#define ESPALTHERMA_COMM_NATIVE_TEST
#define ulong uint32_t // Arduino's unsigned long is 32 bits; the Linux host's is 64 bits.
#include "comm.h"
#undef ulong
void setUp(){clockMs=1000;MySerial.response={0x40,0x20,0x04,0x97,0xFF,0x05};MySerial.arriveAfter=MySerial.perByte=0;MySerial.echo=false;}
void tearDown(){}
void test_real_query_accepts_frame_across_millis_wrap()
{
    clockMs=UINT32_MAX-100;MySerial.arriveAfter=150;
    uint8_t buffer[REPLY_BUFFER_SIZE]={};
    TEST_ASSERT_TRUE(queryRegistry(0x20,buffer));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MySerial.response.data(),buffer,6);
}
void test_frame_completed_exactly_at_deadline_is_valid()
{
    MySerial.arriveAfter=SER_TIMEOUT-6;MySerial.perByte=1;
    uint8_t buffer[REPLY_BUFFER_SIZE]={};
    TEST_ASSERT_TRUE(queryRegistry(0x20,buffer));TEST_ASSERT_EQUAL(1300,clockMs);
}
void test_late_partial_empty_and_bad_crc_replies_fail()
{
    uint8_t buffer[REPLY_BUFFER_SIZE]={};
    MySerial.arriveAfter=SER_TIMEOUT;TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
    MySerial.arriveAfter=0;MySerial.response.resize(4);TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
    MySerial.response.clear();clockMs=UINT32_MAX-50;TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
    MySerial.response={0x40,0x20,0x04,0x97,0xFF,0x04};TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
}
void test_echo_and_error_handling_are_preserved()
{
    uint8_t buffer[REPLY_BUFFER_SIZE]={};MySerial.echo=true;TEST_ASSERT_TRUE(queryRegistry(0x20,buffer));
    MySerial.echo=false;MySerial.response={0x15,0xEA};TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_real_query_accepts_frame_across_millis_wrap);RUN_TEST(test_frame_completed_exactly_at_deadline_is_valid);RUN_TEST(test_late_partial_empty_and_bad_crc_replies_fail);RUN_TEST(test_echo_and_error_handling_are_preserved);return UNITY_END();}
