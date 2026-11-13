#include <unity.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
static unsigned long clockMs;
typedef int SemaphoreHandle_t;
static const int portMAX_DELAY=0,SERIAL_8E1=0;
int xSemaphoreCreateRecursiveMutex(){return 0;}void xSemaphoreTakeRecursive(int,int){}void xSemaphoreGiveRecursive(int){}
unsigned long millis(){return clockMs;}void delay(unsigned long n){clockMs+=n;}
struct Log {void print(const char*){}void println(const char*){}template<typename... A>void printf(const char*,A...){} } Serial,mqttSerial;
class HardwareSerial {
    size_t cursor=0;std::vector<uint8_t> rx;
public:
    std::vector<uint8_t> response;bool echo=false,brokenEcho=false;
    explicit HardwareSerial(int){}void flush(bool){}
    size_t write(const uint8_t *p,size_t n){rx=response;cursor=0;if(echo){rx.insert(rx.begin(),p,p+n);if(brokenEcho)rx[n-1]^=1;}return n;}
    int available(){return cursor<rx.size();}int read(){return rx[cursor++];}
};
#define ESPALTHERMA_COMM_NATIVE_TEST
#include "comm.h"
void setUp(){clockMs=1000;MySerial.echo=false;MySerial.brokenEcho=false;}
void tearDown(){}
static void frame(uint8_t reg){MySerial.response={0x40,reg,4,0x97,0xFF,0};MySerial.response.back()=getCRC(MySerial.response.data(),5);}
void test_bounded_header_validation_accepts_plain_and_echoed_i_replies()
{
    uint8_t buffer[REPLY_BUFFER_SIZE];
    for(uint8_t reg:{uint8_t(0x20),uint8_t(0x50)})
      for(bool echo:{false,true})
      {frame(reg);MySerial.echo=echo;TEST_ASSERT_TRUE(queryRegistry(reg,buffer));TEST_ASSERT_EQUAL_UINT8_ARRAY(MySerial.response.data(),buffer,6);}
}
void test_broken_echo_wrong_registry_short_and_oversized_headers_still_fail()
{
    uint8_t buffer[REPLY_BUFFER_SIZE];frame(0x20);MySerial.echo=true;MySerial.brokenEcho=true;
    TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));MySerial.echo=false;MySerial.brokenEcho=false;
    frame(0x21);TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
    MySerial.response={0x40,0x20,0};TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
    MySerial.response={0x40,0x20,255};TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
    frame(0x20);MySerial.response.back()^=1;TEST_ASSERT_FALSE(queryRegistry(0x20,buffer));
}
void test_s_protocol_echo_and_echo_without_response()
{
    uint8_t buffer[REPLY_BUFFER_SIZE];
    for(bool echo:{false,true})
    {
        MySerial.echo=echo;MySerial.response={0x56,0x11,0x22,0};MySerial.response.back()=getCRC(MySerial.response.data(),3);
        TEST_ASSERT_TRUE(queryRegistry(0x56,buffer,'S'));TEST_ASSERT_EQUAL_UINT8_ARRAY(MySerial.response.data(),buffer,4);
    }
    MySerial.response.clear();TEST_ASSERT_FALSE(queryRegistry(0x56,buffer,'S'));
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_bounded_header_validation_accepts_plain_and_echoed_i_replies);RUN_TEST(test_broken_echo_wrong_registry_short_and_oversized_headers_still_fail);RUN_TEST(test_s_protocol_echo_and_echo_without_response);return UNITY_END();}
