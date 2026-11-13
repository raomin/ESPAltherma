#include <unity.h>
#include <algorithm>
#include <string>
#include "mqttjson.h"

struct FakeMqtt
{
    bool online = true, beginOk = true, endOk = true, retained = false;
    unsigned int announced = 0;
    size_t writes = 0, maxWrite = 0, failAt = 0;
    int begins = 0, ends = 0;
    std::string topic, payload;
    bool connected() { return online; }
    unsigned int getBufferSize() { return 2048; }
    bool beginPublish(const char *t, unsigned int n, bool r)
    {
        begins++;
        topic = t;
        announced = n;
        retained = r;
        return beginOk;
    }
    size_t write(const uint8_t *data, size_t n)
    {
        writes++;
        maxWrite = std::max(maxWrite, n);
        size_t written = writes == failAt ? n - 1 : n;
        payload.append(reinterpret_cast<const char *>(data), written);
        return written;
    }
    bool endPublish() { ends++; return endOk; }
};

void test_values_keep_numbers_strings_nulls_and_diagnostics()
{
    FakeMqtt mqtt;
    TEST_ASSERT_TRUE(mqttStreamJson(mqtt, "espaltherma/ATTR", false, [](MqttJson &json) {
        json.begin(false);
        json.converted("temperature", "-10.5", false);
        json.converted("mode", "Heating", false);
        json.converted("missing", "-", false);
        json.converted("empty", "", false);
        json.converted("valve", "ON", false);
        json.number("COP", "null");
        json.format("energy", "%lu.%03lu", 123UL, 45UL);
        json.text("Uptime", "3600");
        json.end(false);
    }));
    TEST_ASSERT_EQUAL_STRING("{\"temperature\":-10.5,\"mode\":\"Heating\",\"missing\":\"-\",\"empty\":\"\",\"valve\":\"ON\",\"COP\":null,\"energy\":123.045,\"Uptime\":\"3600\"}", mqtt.payload.c_str());
    TEST_ASSERT_EQUAL_size_t(mqtt.payload.size(), mqtt.announced);
    TEST_ASSERT_FALSE(mqtt.retained);
}

void test_table_binary_mode_and_empty_object()
{
    FakeMqtt mqtt;
    TEST_ASSERT_TRUE(mqttStreamJson(mqtt, "test", true, [](MqttJson &json) {
        json.begin(true);
        json.converted("on", "ON", true);
        json.converted("off", "OFF", true);
        json.end(true);
    }));
    TEST_ASSERT_EQUAL_STRING("[{\"on\":1,\"off\":0}]", mqtt.payload.c_str());
    TEST_ASSERT_TRUE(mqtt.retained);
    FakeMqtt empty;
    TEST_ASSERT_TRUE(mqttStreamJson(empty, "test", false, [](MqttJson &json) { json.begin(false); json.end(false); }));
    TEST_ASSERT_EQUAL_STRING("{}", empty.payload.c_str());
}

void test_strings_escape_quotes_backslashes_and_controls()
{
    FakeMqtt mqtt;
    TEST_ASSERT_TRUE(mqttStreamJson(mqtt, "test", false, [](MqttJson &json) {
        json.begin(false);
        json.text("a\"b", "quote\" slash\\ line\n tab\t");
        json.end(false);
    }));
    TEST_ASSERT_EQUAL_STRING("{\"a\\\"b\":\"quote\\\" slash\\\\ line\\u000a tab\\u0009\"}", mqtt.payload.c_str());
    TEST_ASSERT_EQUAL_size_t(mqtt.payload.size(), mqtt.announced);
}

void test_payload_larger_than_old_7120_buffer_is_complete()
{
    FakeMqtt mqtt;
    auto render = [](MqttJson &json) {
        json.begin(false);
        for (int i = 0; i < 400; i++)
        {
            char key[64];
            snprintf(key, sizeof(key), "Measured heat exchanger temperature %d", i);
            json.converted(key, "-10.5", false);
        }
        json.end(false);
    };
    TEST_ASSERT_TRUE(mqttStreamJson(mqtt, "espaltherma/ATTR", false, render));
    TEST_ASSERT_GREATER_THAN(7120, mqtt.announced);
    TEST_ASSERT_EQUAL_size_t(mqtt.payload.size(), mqtt.announced);
    TEST_ASSERT_LESS_OR_EQUAL(MqttJson::CHUNK_SIZE, mqtt.maxWrite);
    TEST_ASSERT_NOT_NULL(strstr(mqtt.payload.c_str(), "\"Measured heat exchanger temperature 399\":-10.5}"));
}

void test_short_write_stops_without_finishing_packet()
{
    for (size_t failure : {size_t(1), size_t(2)})
    {
        FakeMqtt mqtt;
        mqtt.failAt = failure;
        std::string value(2000, 'x');
        TEST_ASSERT_FALSE(mqttStreamJson(mqtt, "test", false, [&](MqttJson &json) {
            json.begin(false); json.text("value", value.c_str()); json.end(false);
        }));
        TEST_ASSERT_EQUAL_size_t(failure, mqtt.writes);
        TEST_ASSERT_EQUAL(0, mqtt.ends);
    }
}

void test_header_failures_do_not_write_payload()
{
    auto render = [](MqttJson &json) { json.begin(false); json.end(false); };
    FakeMqtt mqtt;
    mqtt.beginOk = false;
    TEST_ASSERT_FALSE(mqttStreamJson(mqtt, "test", false, render));
    TEST_ASSERT_EQUAL(0, mqtt.writes);
    TEST_ASSERT_EQUAL(0, mqtt.ends);
    FakeMqtt offline;
    offline.online = false;
    TEST_ASSERT_FALSE(mqttStreamJson(offline, "test", false, render));
    TEST_ASSERT_EQUAL(0, offline.begins);
    FakeMqtt longTopic;
    TEST_ASSERT_FALSE(mqttStreamJson(longTopic, std::string(2048, 't').c_str(), false, render));
    TEST_ASSERT_EQUAL(0, longTopic.begins);
}

void test_uint16_header_limit_is_checked_before_begin()
{
    FakeMqtt mqtt;
    std::string value(65536, 'x');
    TEST_ASSERT_FALSE(mqttStreamJson(mqtt, "test", false, [&](MqttJson &json) {
        json.begin(false); json.text("value", value.c_str()); json.end(false);
    }));
    TEST_ASSERT_EQUAL(0, mqtt.begins);
}

void test_changed_length_and_format_overflow_are_rejected()
{
    FakeMqtt mqtt;
    int pass = 0;
    TEST_ASSERT_FALSE(mqttStreamJson(mqtt, "test", false, [&](MqttJson &json) {
        json.begin(false); json.text("value", pass++ ? "longer" : "short"); json.end(false);
    }));
    TEST_ASSERT_EQUAL(0, mqtt.ends);
    FakeMqtt overflow;
    TEST_ASSERT_FALSE(mqttStreamJson(overflow, "test", false, [](MqttJson &json) {
        json.begin(false); json.format("value", "%060d", 1); json.end(false);
    }));
    TEST_ASSERT_EQUAL(0, overflow.begins);
}

void test_only_successful_registers_are_emitted_in_poll_order()
{
    struct Label { int registryID; const char *label, *asString; };
    const Label labels[] = {{0x30, "speed", "48"}, {0x20, "coil", "-10.5"}, {0x20, "valve", "OFF"}, {0x40, "failed", "stale"}};
    const uint8_t answered[] = {0x20, 0x30};
    FakeMqtt mqtt;
    TEST_ASSERT_TRUE(mqttStreamJson(mqtt, "test", false, [&](MqttJson &json) {
        json.begin(false);
        mqttVisitSelected(labels, 4, answered, 2, [&](const Label &l) { json.converted(l.label, l.asString, true); });
        json.end(false);
    }));
    TEST_ASSERT_EQUAL_STRING("{\"coil\":-10.5,\"valve\":0,\"speed\":48}", mqtt.payload.c_str());
    // One-value-per-topic uses the same selection and binary conversion, without JSON quoting.
    std::string topics;
    mqttVisitSelected(labels, 4, answered, 2, [&](const Label &l) {
        topics += std::string(l.label) + "=" + MqttJson::binaryValue(l.asString, true) + ";";
    });
    TEST_ASSERT_EQUAL_STRING("coil=-10.5;valve=0;speed=48;", topics.c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_values_keep_numbers_strings_nulls_and_diagnostics);
    RUN_TEST(test_table_binary_mode_and_empty_object);
    RUN_TEST(test_strings_escape_quotes_backslashes_and_controls);
    RUN_TEST(test_payload_larger_than_old_7120_buffer_is_complete);
    RUN_TEST(test_short_write_stops_without_finishing_packet);
    RUN_TEST(test_header_failures_do_not_write_payload);
    RUN_TEST(test_uint16_header_limit_is_checked_before_begin);
    RUN_TEST(test_changed_length_and_format_overflow_are_rejected);
    RUN_TEST(test_only_successful_registers_are_emitted_in_poll_order);
    return UNITY_END();
}
