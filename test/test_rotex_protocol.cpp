#include <ArduinoFake.h>
#include <unity.h>

// Few defines to mimic Arduino framework
#include "X10A/converters.hpp"
//#include "def/PROTOCOL_S_ROTEX.h"

using ESPAltherma::Converter;

void setUp(void)
{
    ArduinoFakeReset();
}

void tearDown(void)
{
    // clean stuff up here
}

std::string decode_data(unsigned char *buff)
{
    Converter converter;
    converter.readRegistryValues(buff, 'S');
    LabelDef *labels[128];
    int num = 0;
    converter.getLabels(buff[0], labels, num);
    std::stringstream result;
    for (size_t i = 0; i < num; ++i)
    {
        result << labels[i]->label << ": " << labels[i]->asString << "; ";
    }
    return result.str();
}

void test_rotex_converter_53(void)
{
    Converter converter;
    /*
    Possible values for reg 0x53

    0x53 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xac
    0x53 0x00 0x00 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xab
    0x53 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xab
    0x53 0x01 0x00 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xaa
    0x53 0x01 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xaa
    0x53 0x01 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xaa

    */
    unsigned char buff[] = {0x53, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa};
    auto result = decode_data(buff);
    TEST_ASSERT_EQUAL_STRING("Circulation pump: ON; External heater?: ON; Priority to domestic water: OFF; Burner inhibit from solaris: OFF; ", result.c_str());
    // std::cout << result << std::endl;
    unsigned char buff2[] = {0x53, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa};
    result = decode_data(buff2);
    TEST_ASSERT_EQUAL_STRING("Circulation pump: ON; External heater?: OFF; Priority to domestic water: OFF; Burner inhibit from solaris: ON; ", result.c_str());
    // std::cout << result << std::endl;
}

void test_rotex_converter_54(void)
{
    unsigned char buff[] = {0x54, 0x98, 0x1e, 0x70, 0x1f, 0x1c, 0x24, 0x1c, 0x24, 0xd8, 0x31, 0x01, 0x00, 0x24, 0x00, 0x00, 0x00, 0xb8};
    // auto result = decode_data(buff);
    // std::cout << result << std::endl;

    unsigned char buff1[] = {0x54, 0x54, 0x13, 0xb0, 0x1c, 0x54, 0x1c, 0x54, 0x1c, 0x40, 0x2e, 0x01, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x0b};
    unsigned char buff2[] = {0x54, 0x3c, 0x13, 0x98, 0x1c, 0x3c, 0x1c, 0x54, 0x1c, 0x40, 0x2e, 0x01, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x53};
    unsigned char buff3[] = {0x54, 0xb4, 0x12, 0x84, 0x1c, 0x24, 0x1c, 0x3c, 0x1c, 0x40, 0x2e, 0x01, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x20};
    unsigned char buff4[] = {0x54, 0xcc, 0x1a, 0x6c, 0x1c, 0xb0, 0x1c, 0x3c, 0x1c, 0x40, 0x2e, 0x01, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x8c};
    TEST_ASSERT_EQUAL_STRING("Refrig. Temp. liquid side(C): 30.5938; Inlet water temp.(C): 31.4375; Outlet Water Temp.(C): 36.1094; D(C): 36.1094; DHW tank temp.(C): 49.8438; F(C): 0.00390625; Delta-Tr(deg): 36; R/C Setpoint(C): 0; ",
                             decode_data(buff).c_str());
    TEST_ASSERT_EQUAL_STRING("Refrig. Temp. liquid side(C): 19.3281; Inlet water temp.(C): 28.6875; Outlet Water Temp.(C): 28.3281; D(C): 28.3281; DHW tank temp.(C): 46.25; F(C): 0.00390625; Delta-Tr(deg): 30; R/C Setpoint(C): 0; ",
                             decode_data(buff1).c_str());
    TEST_ASSERT_EQUAL_STRING("Refrig. Temp. liquid side(C): 19.2344; Inlet water temp.(C): 28.5938; Outlet Water Temp.(C): 28.2344; D(C): 28.3281; DHW tank temp.(C): 46.25; F(C): 0.00390625; Delta-Tr(deg): 30; R/C Setpoint(C): 0; ",
                             decode_data(buff2).c_str());
    TEST_ASSERT_EQUAL_STRING("Refrig. Temp. liquid side(C): 18.7031; Inlet water temp.(C): 28.5156; Outlet Water Temp.(C): 28.1406; D(C): 28.2344; DHW tank temp.(C): 46.25; F(C): 0.00390625; Delta-Tr(deg): 30; R/C Setpoint(C): 0; ",
                             decode_data(buff3).c_str());
    TEST_ASSERT_EQUAL_STRING("Refrig. Temp. liquid side(C): 26.7969; Inlet water temp.(C): 28.4219; Outlet Water Temp.(C): 28.6875; D(C): 28.2344; DHW tank temp.(C): 46.25; F(C): 0.00390625; Delta-Tr(deg): 30; R/C Setpoint(C): 0; ",
                             decode_data(buff4).c_str());
    // std::cout << decode_data(buff1) << std::endl;
    // std::cout << decode_data(buff2) << std::endl;
    // std::cout << decode_data(buff3) << std::endl;
    // std::cout << decode_data(buff4) << std::endl;
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_rotex_converter_53);
    RUN_TEST(test_rotex_converter_54);
    UNITY_END();

    return 0;
}