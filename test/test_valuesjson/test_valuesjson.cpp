#include <string>
#include <unity.h>
#include <ArduinoJson.h>
#include "valuesjson.h"

void setUp() {}
void tearDown() {}

static LabelDef value(int registry, const char *label, const char *text)
{
    LabelDef l(registry, 0, 105, 2, 1, label);
    strncpy(l.asString, text, sizeof(l.asString) - 1);
    l.asString[sizeof(l.asString) - 1] = 0;
    return l;
}

// What the page got before, from a JsonDocument and serializeJson()
static std::string fromArduinoJson(const LabelDef *labels, size_t count)
{
    JsonDocument doc;
    JsonArray values = doc.to<JsonArray>();
    for (size_t i = 0; i < count; i++)
    {
        JsonObject item = values.add<JsonObject>();
        item["l"] = labels[i].label;
        item["v"] = labels[i].asString;
        item["r"] = labels[i].registryID;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

static std::string fromValuesJson(const LabelDef *labels, size_t count)
{
    size_t length = valuesJson(labels, count, nullptr, 0);
    std::string out(length + 1, 'x');
    TEST_ASSERT_EQUAL(length, valuesJson(labels, count, &out[0], out.size()));
    TEST_ASSERT_EQUAL(0, out[length]);
    out.resize(length);
    return out;
}

// The same text as serializeJson(), byte for byte, with what a definition or a value can hold
void test_the_values_text_is_the_one_serializejson_gave()
{
    const LabelDef labels[] = {
        value(0x61, "DHW tank temp. (R5T)", "47.5"),
        value(0x10, "Operation Mode", "Heating"),
        value(0x60, "A \"quoted\" name \\ with a backslash", "-"),
        value(0x20, "Tab\tand\nnewline", "\r\b\f"),
        value(0xA0, "Indoor unit blowout 50 \xb0 C flag", "\xc3\xa9t\xc3\xa9"),
        value(0x00, "Empty value", ""),
    };
    TEST_ASSERT_EQUAL_STRING(fromArduinoJson(labels, 6).c_str(), fromValuesJson(labels, 6).c_str());
    TEST_ASSERT_EQUAL_STRING(fromArduinoJson(labels, 0).c_str(), fromValuesJson(labels, 0).c_str());
}

// The other control characters go out as a unicode escape, which a browser's JSON.parse() takes. serializeJson()
// left them as they were, which it does not.
void test_other_control_characters_are_escaped()
{
    const LabelDef labels[] = {value(0x20, "Bell\x07", "\x01\x1f")};
    std::string text = fromValuesJson(labels, 1);
    TEST_ASSERT_EQUAL_STRING("[{\"l\":\"Bell\\u0007\",\"v\":\"\\u0001\\u001f\",\"r\":32}]", text.c_str());
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, text) == DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING("Bell\x07", doc[0]["l"].as<const char *>());
    TEST_ASSERT_EQUAL_STRING("\x01\x1f", doc[0]["v"].as<const char *>());
}

// A buffer too short holds what fits, terminated, and the length returned says how much was needed
void test_a_short_buffer_is_never_overrun()
{
    const LabelDef labels[] = {value(0x61, "DHW tank temp. (R5T)", "47.5")};
    size_t length = valuesJson(labels, 1, nullptr, 0);
    char out[16 + 8];
    memset(out, 'x', sizeof(out));
    TEST_ASSERT_EQUAL(length, valuesJson(labels, 1, out, 16));
    TEST_ASSERT_EQUAL(15, strlen(out));
    TEST_ASSERT_EQUAL_STRING_LEN("xxxxxxxx", out + 16, 8);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_values_text_is_the_one_serializejson_gave);
    RUN_TEST(test_other_control_characters_are_escaped);
    RUN_TEST(test_a_short_buffer_is_never_overrun);
    return UNITY_END();
}
