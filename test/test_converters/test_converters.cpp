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

// R134a (refrigerant code 804, the Altherma HT hydrobox): a low pressure of 2.55 kgf/cm2 gauge is about 5.1 C
// of saturation, where the R410A curve would give -23 C (issue #95)
void test_refrigerant_r134a(void)
{
    Converter c;
    LabelDef kind(0x00, 0, 804, 0, -1, "*Refrigerant type");
    unsigned char none[] = {0, 0};
    c.convert(&kind, none);
    TEST_ASSERT_EQUAL_STRING("R134a", kind.asString);
    TEST_ASSERT_EQUAL(804, c.RType);
    TEST_ASSERT_FLOAT_WITHIN(0.2, 5.15, c.convertPress2Temp(2.55));
    TEST_ASSERT_FLOAT_WITHIN(0.5, -26.4, c.convertPress2Temp(0.0)); // atmospheric: the boiling point of R134a
    // conversion 403: the pressure word /256 (2.55 = 0x028D), then the saturation temperature
    LabelDef lp(0x50, 2, 403, 2, -1, "Low Pressure(T)");
    unsigned char data[] = {0x8D, 0x02};
    c.convert(&lp, data);
    TEST_ASSERT_EQUAL_STRING_LEN("5.", lp.asString, 2);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_conv119_signed);
    RUN_TEST(test_conv119_no_sensor);
    RUN_TEST(test_conv119_matches_103);
    RUN_TEST(test_refrigerant_r134a);
    UNITY_END();

    return 0;
}
