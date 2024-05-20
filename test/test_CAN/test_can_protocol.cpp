#include <iostream>
#include <fstream>
#include <unity.h>
#include <ArduinoFake.h>
#include <ArduinoJson.h>
#include <canBus.hpp>

#include "../mockSetup.hpp"

using namespace fakeit;
using namespace std;

#define MODELS_CONFIG_SIZE 1024*10

#define CANFile "build/CAN/English/commands_hpsu.json"

CAN_Config* TestCANConfig = nullptr;

void setUp(void)
{
    ArduinoFakeReset();

    TestCANConfig = new CAN_Config();

    ifstream inputFile(CANFile);

    if(!inputFile) {
        throw runtime_error("Run script \"build_can_commands.py\" before running test!");
    }

    string jsonContent((istreambuf_iterator<char>(inputFile)), istreambuf_iterator<char>());
    inputFile.close();

    DynamicJsonDocument configDoc(MODELS_CONFIG_SIZE);
    deserializeJson(configDoc, jsonContent);

    JsonObject jsonObject = configDoc.as<JsonObject>();

    jsonObject["COMMANDS"] = jsonObject["Commands"];
    jsonObject.remove("Commands");

    canBus_fill_config(jsonObject, TestCANConfig);
    //canBus_setup(TestCANConfig);
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