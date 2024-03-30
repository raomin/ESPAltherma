#include <ArduinoFake.h>
#include <unity.h>

// Few defines to mimic Arduino framework
#include "X10A/config.hpp"
#include "Config/config.hpp"
#include "X10A/converters.hpp"
#include "X10A/x10a.hpp"

#define X10AFile "../build/X10A/English/PROTOCOL_S_ROTEX.json"

#if not __has_include(X10AFile)
#error Run script "build_x10a_commands.py" before running test!
#endif

X10A_Config* X10AConfig = nullptr;

void setUp(void)
{
    ArduinoFakeReset();

    X10AConfig = new X10A_Config();
    X10AConfig->X10A_PROTOCOL = X10AProtocol::S;

    FILE* x10aFile = fopen(X10AFile, "r");

    DynamicJsonDocument configDoc(MODELS_CONFIG_SIZE);
    deserializeJson(configDoc, x10aFile);
    fclose(x10aFile);

    JsonObject jsonObject = configDoc.as<JsonObject>();

    fillX10AParameters(jsonObject, X10AConfig);
    x10a_init(X10AConfig, true);
}

void tearDown(void)
{
    // clean stuff up here
}
/*
std::string decode_data(unsigned char *buff)
{
    ESPAltherma::Converter converter;

    converter.convert()
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
}*/

void makeAssert(const String label, const String value) {
    for (size_t i = 0; i < X10AConfig->PARAMETERS_LENGTH; i++) {
        auto&& param = *X10AConfig->PARAMETERS[i];

        if(param.label.equals(label)) {
            TEST_ASSERT_EQUAL_STRING(value, label.asString);
        }
    }
}

void test_rotex_converter_53(void)
{
    /*
    Possible values for reg 0x53

    0x53 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xac
    0x53 0x00 0x00 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xab
    0x53 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xab
    0x53 0x01 0x00 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xaa
    0x53 0x01 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xaa
    0x53 0x01 0x00 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xaa

    */
    RegistryBuffer[] buffer = new RegistryBuffer[1];

    RegistryBuffer* X10ABuffer1 = new RegistryBuffer();
    X10ABuffer1->RegistryID = 0x53;
    X10ABuffer1->Buffer = {0x53, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa};
    X10ABuffer1->Success = true;

    buffer[0] = X10ABuffer1;

    x10a_convert_values(buffer, 1, false);
    makeAssert("Circulation pump", "ON");
    makeAssert("External heater?", "ON");
    makeAssert("Priority to domestic water", "OFF");
    makeAssert("Burner inhibit from solaris", "OFF");

    RegistryBuffer* X10ABuffer2 = new RegistryBuffer();
    X10ABuffer2->RegistryID = 0x53;
    X10ABuffer2->Buffer = {0x53, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa};
    X10ABuffer2->Success = true;

    buffer[0] = X10ABuffer2;

    x10a_convert_values(buffer, 1, false);
    makeAssert("Circulation pump", "ON");
    makeAssert("External heater?", "OFF");
    makeAssert("Priority to domestic water", "OFF");
    makeAssert("Burner inhibit from solaris", "ON");
}

void test_rotex_converter_54(void)
{   /*
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
    */
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_rotex_converter_53);
    RUN_TEST(test_rotex_converter_54);
    UNITY_END();

    return 0;
}