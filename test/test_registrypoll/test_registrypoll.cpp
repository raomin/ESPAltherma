#include <unity.h>
#include "registrypoll.h"

void test_failed_query_with_matching_registry_is_rejected()
{
    uint8_t bytes[64];
    unsigned int queries = 0, retries = 0;
    bool result = registryPoll(0x20, bytes, sizeof(bytes), 'I', [&]() {
        queries++;
        bytes[0] = 0x40; bytes[1] = 0x20; bytes[2] = 18;
        return false; // a CRC error or a partial reply, despite its matching register
    }, [&]() { retries++; });
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(4, queries);
    TEST_ASSERT_EQUAL(3, retries);
}

void test_register_zero_timeout_does_not_become_a_valid_reply()
{
    uint8_t bytes[64];
    TEST_ASSERT_FALSE(registryPoll(0, bytes, sizeof(bytes), 'I', []() { return false; }, []() {}));
    TEST_ASSERT_FALSE(registryPoll(0, bytes, sizeof(bytes), 'S', []() { return false; }, []() {}));
}

void test_valid_retry_does_not_inherit_payload_from_failed_attempt()
{
    uint8_t bytes[64];
    unsigned int queries = 0;
    TEST_ASSERT_TRUE(registryPoll(0x20, bytes, sizeof(bytes), 'I', [&]() {
        queries++;
        bytes[0] = 0x40; bytes[1] = 0x20;
        if (queries == 1) { bytes[50] = 123; return false; }
        return true;
    }, []() {}));
    TEST_ASSERT_EQUAL(2, queries);
    TEST_ASSERT_EQUAL(0, bytes[50]);
}

void test_wrong_register_or_header_is_retried()
{
    uint8_t bytes[64];
    unsigned int queries = 0;
    TEST_ASSERT_TRUE(registryPoll(0x20, bytes, sizeof(bytes), 'I', [&]() {
        queries++;
        bytes[0] = queries == 2 ? 0x15 : 0x40;
        bytes[1] = queries == 1 ? 0x30 : 0x20;
        return true;
    }, []() {}));
    TEST_ASSERT_EQUAL(3, queries);
}

void test_protocol_s_uses_its_first_byte()
{
    uint8_t bytes[64];
    TEST_ASSERT_TRUE(registryPoll(0x56, bytes, sizeof(bytes), 'S', [&]() {
        bytes[0] = 0x56; return true;
    }, []() {}));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_failed_query_with_matching_registry_is_rejected);
    RUN_TEST(test_register_zero_timeout_does_not_become_a_valid_reply);
    RUN_TEST(test_valid_retry_does_not_inherit_payload_from_failed_attempt);
    RUN_TEST(test_wrong_register_or_header_is_retried);
    RUN_TEST(test_protocol_s_uses_its_first_byte);
    return UNITY_END();
}
