#include <unity.h>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Only the UART, clock and lock primitives are substituted. Run the production query, retry and converter code.
static unsigned long clockMs;
typedef unsigned long ulong;
typedef int SemaphoreHandle_t;
static const int portMAX_DELAY = 0, SERIAL_8E1 = 0;
int xSemaphoreCreateRecursiveMutex() { return 0; }
void xSemaphoreTakeRecursive(int, int) {}
void xSemaphoreGiveRecursive(int) {}
unsigned long millis() { return clockMs; }
void delay(unsigned long n) { clockMs += n; }
struct Log {
    void print(const char *) {} void println(const char *) {}
    template <typename... Args> void printf(const char *, Args...) {}
} Serial, mqttSerial;
class HardwareSerial {
    size_t cursor = 0;
public:
    std::vector<uint8_t> response;
    explicit HardwareSerial(int) {}
    void flush(bool) {}
    size_t write(const uint8_t *, size_t n) { cursor = 0; return n; }
    int available() { return cursor < response.size(); }
    int read() { return response[cursor++]; }
};
#define ESPALTHERMA_COMM_NATIVE_TEST
#include "comm.h"
#include "registrypoll.h"
#include "labeldef.h"
#include "converters.h"

void test_crc_invalid_matching_reply_never_reaches_converter()
{
    MySerial.response = {0x40,0x20,0x04,0x97,0xFF,0x04}; // matching registry, intentionally wrong checksum
    uint8_t buffer[REPLY_BUFFER_SIZE];
    LabelDef label(0x20,0,105,2,1,"coil");
    Converter c; c.quiet = true; c.setLabels(&label, 1);
    bool ok = registryPoll(0x20, buffer, sizeof(buffer), 'I', [&]() { return queryRegistry(0x20, buffer); }, []() {});
    if (ok) c.readRegistryValues(buffer, 'I', (size_t)buffer[2] + 2);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(label.valid);
}

void test_real_reply_is_accepted_but_missing_fields_stay_invalid()
{
    MySerial.response = {0x40,0x00,0x0C,0x04,0x01,0x00,0x01,0x01,0x01,0x00,0x00,0x01,0x01,0xA9};
    uint8_t buffer[REPLY_BUFFER_SIZE];
    TEST_ASSERT_TRUE(registryPoll(0, buffer, sizeof(buffer), 'I', [&]() { return queryRegistry(0, buffer); }, []() {}));
    LabelDef labels[] = {{0,9,152,1,-1,"valid"},{0,10,152,1,-1,"missing"}};
    Converter c; c.quiet = true; c.setLabels(labels, 2);
    TEST_ASSERT_TRUE(c.readRegistryValues(buffer, 'I', (size_t)buffer[2] + 2));
    TEST_ASSERT_TRUE(labels[0].valid);
    TEST_ASSERT_FALSE(labels[1].valid);
}

void test_partial_and_error_replies_fail_at_transport()
{
    uint8_t buffer[REPLY_BUFFER_SIZE];
    MySerial.response = {0x40,0x20,0x04,0x97};
    TEST_ASSERT_FALSE(queryRegistry(0x20, buffer));
    MySerial.response = {0x15,0xEA};
    TEST_ASSERT_FALSE(queryRegistry(0x20, buffer));
    MySerial.response = {0x40,0x20,0x00};
    TEST_ASSERT_FALSE(queryRegistry(0x20, buffer));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_invalid_matching_reply_never_reaches_converter);
    RUN_TEST(test_real_reply_is_accepted_but_missing_fields_stay_invalid);
    RUN_TEST(test_partial_and_error_replies_fail_at_transport);
    return UNITY_END();
}
