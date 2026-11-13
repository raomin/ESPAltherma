#include <unity.h>
#include <ArduinoJson.h>
#include <string>
#include <string.h>
#include <stdlib.h>
struct AsyncWebServerRequest {
    void *_tempObject=nullptr;size_t length;int status=0;std::string response;
    explicit AsyncWebServerRequest(size_t n):length(n){}
    ~AsyncWebServerRequest(){free(_tempObject);}
    size_t contentLength(){return length;}
    void send(int code,const char*,const char *body){status=code;response=body;}
};
#include "requestbody.h"
void setUp(){} void tearDown(){}
void feed(AsyncWebServerRequest &r,const std::string &body,size_t start,size_t n)
{collectBody(&r,(uint8_t*)body.data()+start,n,start,body.size());}
void test_oversize_body_is_413_without_allocation_even_if_json_prefix_is_valid()
{
    std::string body="{}"+std::string(MAX_BODY_SIZE-1,' ');AsyncWebServerRequest r(body.size());JsonDocument doc;
    feed(r,body,0,2);feed(r,body,2,body.size()-2);
    TEST_ASSERT_NULL(r._tempObject);TEST_ASSERT_FALSE(parseBody(&r,doc));TEST_ASSERT_EQUAL(413,r.status);
    TEST_ASSERT_TRUE(r.response.find("too large")!=std::string::npos);
}
void test_exact_limit_and_split_valid_json_are_accepted()
{
    std::string body="{\"target\":25}"+std::string(MAX_BODY_SIZE-13,' ');
    TEST_ASSERT_EQUAL(MAX_BODY_SIZE,body.size());
    AsyncWebServerRequest r(body.size());JsonDocument doc;
    feed(r,body,0,7);feed(r,body,7,body.size()-7);
    TEST_ASSERT_TRUE(parseBody(&r,doc));TEST_ASSERT_EQUAL(25,doc["target"].as<int>());
}
void test_malformed_missing_and_empty_body_remain_400()
{
    for(const std::string &body:{std::string("{"),std::string("")})
    {
        AsyncWebServerRequest r(body.size());JsonDocument doc;feed(r,body,0,body.size());
        TEST_ASSERT_FALSE(parseBody(&r,doc));TEST_ASSERT_EQUAL(400,r.status);
    }
    AsyncWebServerRequest r(4);JsonDocument doc;TEST_ASSERT_FALSE(parseBody(&r,doc));TEST_ASSERT_EQUAL(400,r.status);
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_oversize_body_is_413_without_allocation_even_if_json_prefix_is_valid);RUN_TEST(test_exact_limit_and_split_valid_json_are_accepted);RUN_TEST(test_malformed_missing_and_empty_body_remain_400);return UNITY_END();}
