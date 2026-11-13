#include <unity.h>
#include <vector>
#include "mqttframe.h"

struct Client {
    std::vector<uint8_t> wire;
    bool online = true, shortWrite = false;
    bool connected() { return online; }
    size_t write(const uint8_t *p, size_t n) {
        size_t used = shortWrite && n ? n - 1 : n;
        wire.insert(wire.end(), p, p + used);
        return used;
    }
};

void test_large_stream_header_uses_all_remaining_length_bits()
{
    Client c;
    TEST_ASSERT_TRUE(mqttBeginStream(c, "espaltherma/discovery", 100000, true));
    TEST_ASSERT_EQUAL_HEX8(0x31, c.wire[0]);
    size_t i = 1; uint32_t length = 0, multiplier = 1;
    uint8_t byte;
    do { byte = c.wire[i++]; length += (byte & 127) * multiplier; multiplier *= 128; } while (byte & 128);
    TEST_ASSERT_EQUAL(100000 + 2 + strlen("espaltherma/discovery"), length);
    TEST_ASSERT_EQUAL(0, c.wire[i++]);
    TEST_ASSERT_EQUAL(strlen("espaltherma/discovery"), c.wire[i++]);
    TEST_ASSERT_EQUAL_STRING_LEN("espaltherma/discovery", reinterpret_cast<const char *>(c.wire.data() + i), strlen("espaltherma/discovery"));
}

void test_bad_sizes_and_short_writes_are_rejected()
{
    Client c;
    TEST_ASSERT_FALSE(mqttBeginStream(c, "test", 268435455UL, false));
    TEST_ASSERT_EQUAL(0, c.wire.size());
    c.online = false;
    TEST_ASSERT_FALSE(mqttBeginStream(c, "test", 10, false));
    c.online = true; c.shortWrite = true;
    TEST_ASSERT_FALSE(mqttBeginStream(c, "test", 10, false));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_large_stream_header_uses_all_remaining_length_bits);
    RUN_TEST(test_bad_sizes_and_short_writes_are_rejected);
    return UNITY_END();
}
