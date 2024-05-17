#include <iostream>
#include <fstream>
#include <unity.h>
#include <ArduinoFake.h>
#include <ArduinoJson.h>

#include <canBus.hpp>

using namespace fakeit;
using namespace std;

#define MODELS_CONFIG_SIZE 1024*10

#define CANFile "build/CAN/English/commands_hpsu.json"

void setUp(void)
{
    ArduinoFakeReset();
}

void tearDown(void)
{
    // clean stuff up here
}

void test_can_read(void)
{
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_can_read);
    UNITY_END();

    return 0;
}