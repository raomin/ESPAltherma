#include <string>
#include <unity.h>

// Few defines to mimic Arduino framework
#include "../arduino_to_native.h"

#include "labeldef.h"
#include "converters.h"

void setUp(void) {}
void tearDown(void) {}

static std::string convert(int convid, unsigned char lo, unsigned char hi)
{
    Converter converter;
    LabelDef def(0xA0, 2, convid, 2, 1, "Outdoor heat exchanger temp.");
    unsigned char data[] = {lo, hi};
    converter.convert(&def, data);
    return def.asString;
}

// Conversion 119 is signed (#543): a frosted heat exchanger read 250° instead of -6°
void test_conv119_signed(void)
{
    TEST_ASSERT_EQUAL_STRING("-6", convert(119, 0x00, 0xFA).c_str());
    TEST_ASSERT_EQUAL_STRING("-0.5", convert(119, 0x80, 0xFF).c_str());
    TEST_ASSERT_EQUAL_STRING("23.5", convert(119, 0x80, 0x17).c_str());
    TEST_ASSERT_EQUAL_STRING("0", convert(119, 0x00, 0x00).c_str());
}

// 0x8000: no sensor
void test_conv119_no_sensor(void)
{
    TEST_ASSERT_EQUAL_STRING("---", convert(119, 0x00, 0x80).c_str());
}

// Same reading as conversion 103, which owners used as the workaround
void test_conv119_matches_103(void)
{
    TEST_ASSERT_EQUAL_STRING(convert(103, 0x00, 0xFA).c_str(), convert(119, 0x00, 0xFA).c_str());
    TEST_ASSERT_EQUAL_STRING(convert(103, 0x40, 0x2D).c_str(), convert(119, 0x40, 0x2D).c_str());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_conv119_signed);
    RUN_TEST(test_conv119_no_sensor);
    RUN_TEST(test_conv119_matches_103);
    UNITY_END();

    return 0;
}
