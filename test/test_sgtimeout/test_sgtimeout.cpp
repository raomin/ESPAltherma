#include "../config_native.h"
#include <stdarg.h>
#include <string>
#include <unity.h>
#include <vector>

using byte = uint8_t;
static uint64_t nowMs;
static int sgMode = 0, published = 0, delivered = 0, lastDelivered = -1, saves = 0;
static bool broker = true;
struct Write
{
    int pin, level;
};
static std::vector<Write> writes;
static std::vector<std::string> events;
int64_t esp_timer_get_time()
{
    return nowMs * 1000;
}
void digitalWrite(int pin, int level)
{
    writes.push_back({pin, level});
}
int sgActiveState()
{
    return config.sgActiveHigh ? 1 : 0;
}
int sgInactiveState()
{
    return config.sgActiveHigh ? 0 : 1;
}
void publishSgState()
{
    published++;
    if (broker)
    {
        delivered++;
        lastDelivered = sgMode;
    }
}
void configSave()
{
    saves++;
}
struct Logger
{
    template <typename... A> void printf(const char *, A...) {}
} mqttSerial;
void eventAddf(const char *format, ...)
{
    char text[128];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    events.push_back(text);
}
#include "sgcontrol.h" // actual callback, relay mapping and unconditional timeout-loop code

void setUp()
{
    configDefaults(config);
    config.sg1Pin = 32;
    config.sg2Pin = 33;
    sgCommandTimeout = SgCommandTimeout();
    nowMs = 1000;
    sgMode = 0;
    published = delivered = saves = 0;
    lastDelivered = -1;
    broker = true;
    writes.clear();
    events.clear();
}
void tearDown() {}
static void command(const char *text)
{
    std::vector<byte> payload(text, text + strlen(text));
    callbackSg(payload.data(), payload.size());
}
static void assertNormal()
{
    TEST_ASSERT_EQUAL(0, sgMode);
    TEST_ASSERT_GREATER_OR_EQUAL(2, writes.size());
    TEST_ASSERT_EQUAL(32, writes[writes.size() - 2].pin);
    TEST_ASSERT_EQUAL(33, writes.back().pin);
    TEST_ASSERT_EQUAL(sgInactiveState(), writes[writes.size() - 2].level);
    TEST_ASSERT_EQUAL(sgInactiveState(), writes.back().level);
}

void test_default_timeout_is_disabled_and_commands_do_not_save_flash()
{
    command("3");
    nowMs = UINT32_MAX - 1;
    sgTimeoutLoop();
    TEST_ASSERT_EQUAL(0, config.sgTimeoutSeconds);
    TEST_ASSERT_EQUAL(3, sgMode);
    TEST_ASSERT_EQUAL(1, published);
    TEST_ASSERT_TRUE(events.empty());
    TEST_ASSERT_EQUAL(0, saves);
    TEST_ASSERT_LESS_OR_EQUAL(16, sizeof(sgCommandTimeout));
}
void test_each_non_normal_mode_expires_for_both_relay_polarities()
{
    for (bool high : {false, true})
        for (int mode = 1; mode <= 3; mode++)
        {
            setUp();
            config.sgActiveHigh = high;
            config.sgTimeoutSeconds = 30;
            char payload = (char)('0' + mode);
            callbackSg((byte *)&payload, 1);
            TEST_ASSERT_EQUAL(mode, sgMode);
            TEST_ASSERT_EQUAL(mode & 2 ? sgActiveState() : sgInactiveState(), writes[0].level);
            TEST_ASSERT_EQUAL(mode & 1 ? sgActiveState() : sgInactiveState(), writes[1].level);
            nowMs = 30999;
            sgTimeoutLoop();
            TEST_ASSERT_EQUAL(mode, sgMode);
            nowMs = 31000;
            sgTimeoutLoop();
            assertNormal();
            TEST_ASSERT_EQUAL(0, lastDelivered);
            TEST_ASSERT_EQUAL(1, events.size());
            nowMs = 100000;
            sgTimeoutLoop();
            TEST_ASSERT_EQUAL(2, published);
            TEST_ASSERT_EQUAL(1, events.size());
            TEST_ASSERT_EQUAL(0, saves);
        }
}
void test_same_mode_heartbeat_renews_lease_and_whitespace_is_accepted()
{
    config.sgTimeoutSeconds = 30;
    nowMs = 0;
    command("3");
    nowMs = 25000;
    command(" \t3\r\n");
    nowMs = 54999;
    sgTimeoutLoop();
    TEST_ASSERT_EQUAL(3, sgMode);
    nowMs = 55000;
    sgTimeoutLoop();
    assertNormal();
    TEST_ASSERT_EQUAL(1, events.size());
}
void test_invalid_or_nonterminated_payload_does_not_mutate_input_or_renew_lease()
{
    config.sgTimeoutSeconds = 30;
    nowMs = 0;
    byte input[] = {'3', 0xA5};
    callbackSg(input, 1);
    TEST_ASSERT_EQUAL(0xA5, input[1]);
    nowMs = 29000;
    for (const char *bad : {"", "4", "30", "-1", "ON", "3.0", "3junk"})
        command(bad);
    byte binary[] = {'3', 0};
    callbackSg(binary, 2);
    callbackSg(nullptr, 0);
    TEST_ASSERT_EQUAL(1, published);
    TEST_ASSERT_EQUAL(2, writes.size());
    nowMs = 30000;
    sgTimeoutLoop();
    assertNormal();
}
void test_normal_command_disarms_timeout()
{
    config.sgTimeoutSeconds = 30;
    command("2");
    nowMs = 10000;
    command("0");
    nowMs = 100000;
    sgTimeoutLoop();
    assertNormal();
    TEST_ASSERT_TRUE(events.empty());
    TEST_ASSERT_EQUAL(2, published);
}
void test_timeout_changes_use_last_command_and_not_settings_save_time()
{
    command("3");
    nowMs = 60000;
    config.sgTimeoutSeconds = 30;
    sgTimeoutLoop();
    assertNormal(); // opt-in does not renew a stale command
    setUp();
    config.sgTimeoutSeconds = 30;
    command("3");
    nowMs = 20000;
    config.sgTimeoutSeconds = 10;
    sgTimeoutLoop();
    assertNormal();
    setUp();
    config.sgTimeoutSeconds = 10;
    command("3");
    nowMs = 5000;
    config.sgTimeoutSeconds = 30;
    sgTimeoutLoop();
    nowMs = 30999;
    sgTimeoutLoop();
    TEST_ASSERT_EQUAL(3, sgMode);
    nowMs = 31000;
    sgTimeoutLoop();
    assertNormal();
    setUp();
    config.sgTimeoutSeconds = 30;
    command("3");
    nowMs = 30000;
    strcpy(config.hostname, "saved-other-setting");
    sgTimeoutLoop();
    nowMs = 31000;
    sgTimeoutLoop();
    assertNormal();
}
void test_broker_loss_does_not_prevent_gpio_reset_or_change_retained_state_on_reconnect()
{
    config.sgTimeoutSeconds = 30;
    command("3");
    broker = false;
    nowMs = 31000;
    sgTimeoutLoop();
    assertNormal();
    TEST_ASSERT_EQUAL(1, delivered);
    TEST_ASSERT_EQUAL(1, events.size());
    broker = true;
    publishSgState();
    TEST_ASSERT_EQUAL(0, lastDelivered);
    nowMs = 62000;
    sgTimeoutLoop();
    TEST_ASSERT_EQUAL(1, events.size());
}
void test_uptime_wrap_and_missing_second_pin()
{
    config.sgTimeoutSeconds = 2;
    nowMs = UINT32_MAX - 1000;
    command("2");
    nowMs = uint64_t(UINT32_MAX) + 999;
    sgTimeoutLoop();
    TEST_ASSERT_EQUAL(2, sgMode);
    nowMs = uint64_t(UINT32_MAX) + 1000;
    sgTimeoutLoop();
    assertNormal();
    setUp();
    command("3");
    nowMs = (uint64_t{1} << 32) + 1001;
    config.sgTimeoutSeconds = 30;
    sgTimeoutLoop();
    assertNormal();
    setUp();
    config.sgTimeoutSeconds = 2;
    config.sg2Pin = -1;
    command("3");
    nowMs = 100000;
    sgTimeoutLoop();
    TEST_ASSERT_EQUAL(0, sgMode);
    TEST_ASSERT_TRUE(writes.empty());
    TEST_ASSERT_EQUAL(0, published);
}
void test_config_default_legacy_load_and_json_round_trip()
{
    AppConfig old;
    configDefaults(old);
    TEST_ASSERT_EQUAL(0, old.sgTimeoutSeconds);
    JsonDocument doc;
    deserializeJson(doc, "{\"relays\":{\"sg1_pin\":32,\"sg2_pin\":33}}");
    configFromJson(old, doc.as<JsonVariantConst>());
    TEST_ASSERT_EQUAL(0, old.sgTimeoutSeconds);
    deserializeJson(doc, "{\"relays\":{\"sg_timeout_s\":30}}");
    configFromJson(old, doc.as<JsonVariantConst>());
    TEST_ASSERT_EQUAL(30, old.sgTimeoutSeconds);
    configToJson(old, doc, true);
    std::string text;
    serializeJson(doc, text);
    JsonDocument loaded;
    deserializeJson(loaded, text);
    AppConfig restored;
    configDefaults(restored);
    configFromJson(restored, loaded.as<JsonVariantConst>());
    TEST_ASSERT_EQUAL(30, restored.sgTimeoutSeconds);
    deserializeJson(doc, "{\"hostname\":\"old-backup\"}");
    configFromJson(restored, doc.as<JsonVariantConst>());
    TEST_ASSERT_EQUAL(30, restored.sgTimeoutSeconds);
}
void test_api_validation_rejects_invalid_values_without_disabling_existing_timeout()
{
    config.sgTimeoutSeconds = 30;
    JsonDocument doc;
    for (const char *invalid : {"-1", "0.5", "86401", "4294967295", "\"30\"", "true", "[]", "{}"})
    {
        std::string json = "{\"relays\":{\"sg_timeout_s\":" + std::string(invalid) + "}}";
        deserializeJson(doc, json);
        TEST_ASSERT_FALSE(sgTimeoutSettingValid(doc["relays"]["sg_timeout_s"]));
        configFromJson(config, doc.as<JsonVariantConst>());
        TEST_ASSERT_EQUAL(30, config.sgTimeoutSeconds);
    }
    for (const char *valid : {"0", "1", "30.0", "86400"})
    {
        std::string json = "{\"relays\":{\"sg_timeout_s\":" + std::string(valid) + "}}";
        deserializeJson(doc, json);
        TEST_ASSERT_TRUE(sgTimeoutSettingValid(doc["relays"]["sg_timeout_s"]));
        configFromJson(config, doc.as<JsonVariantConst>());
        TEST_ASSERT_EQUAL((uint32_t)doc["relays"]["sg_timeout_s"].as<double>(), config.sgTimeoutSeconds);
    }
    deserializeJson(doc, "{\"relays\":{\"sg_timeout_s\":null}}");
    configFromJson(config, doc.as<JsonVariantConst>());
    TEST_ASSERT_EQUAL(86400, config.sgTimeoutSeconds);
}
int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_timeout_is_disabled_and_commands_do_not_save_flash);
    RUN_TEST(test_each_non_normal_mode_expires_for_both_relay_polarities);
    RUN_TEST(test_same_mode_heartbeat_renews_lease_and_whitespace_is_accepted);
    RUN_TEST(test_invalid_or_nonterminated_payload_does_not_mutate_input_or_renew_lease);
    RUN_TEST(test_normal_command_disarms_timeout);
    RUN_TEST(test_timeout_changes_use_last_command_and_not_settings_save_time);
    RUN_TEST(test_broker_loss_does_not_prevent_gpio_reset_or_change_retained_state_on_reconnect);
    RUN_TEST(test_uptime_wrap_and_missing_second_pin);
    RUN_TEST(test_config_default_legacy_load_and_json_round_trip);
    RUN_TEST(test_api_validation_rejects_invalid_values_without_disabling_existing_timeout);
    return UNITY_END();
}
