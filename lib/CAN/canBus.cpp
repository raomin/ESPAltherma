#include "canBus.hpp"

using namespace CAN;

static CANDriver *driver = nullptr;

bool canBus_setup(const CAN_Config* CANConfig)
{
    canBus_stop();

    debugStream->println("Starting new CAN Driver...");

    bool result = false;

    switch (CANConfig->CAN_IC)
    {
    case CAN_ICTypes::ELM327:
        driver = new DriverELM327(CANConfig);
        result = driver->initInterface();
        break;

    #if !defined(ARDUINO_ARCH_ESP8266) && !defined(PIO_UNIT_TESTING)
    case CAN_ICTypes::MCP2515:
        driver = new DriverMCP2515(CANConfig);
        result = driver->initInterface();
        break;

    case CAN_ICTypes::SJA1000:
        driver = new DriverSJA1000(CANConfig);
        result = driver->initInterface();
        break;
    #endif

    default:
        debugStream->println("No CAN Driver found");
        return result;
    }

    if(!result)
        return false;

    driver->enableSniffing(CANConfig->CAN_SNIFFING_ENABLED);

    return true;
}

void canBus_loop()
{
    if(driver != nullptr)
        driver->handleLoop();
}

void canBus_set(const String label, const char *payload, const uint32_t length)
{
    if(driver != nullptr)
        driver->handleMQTTSetRequest(label, payload, length);
}

String canBus_readAllCommands()
{
    if(driver != nullptr)
        return driver->readAllCommands();

    return "";
}

void canBus_fill_config(JsonObject &jsonObject, CAN_Config *config)
{
    JsonArray commands = jsonObject["COMMANDS"].as<JsonArray>();
    config->COMMANDS_LENGTH = commands.size();
    config->COMMANDS = new CANCommand *[config->COMMANDS_LENGTH];

    for (size_t i = 0; i < config->COMMANDS_LENGTH; i++) {
        JsonArray command = commands[i];

        JsonArray commandBytes = command[CAN_COMMAND_INDEX_COMMAND];
        byte commandArray[] = {
            commandBytes[0],
            commandBytes[1],
            commandBytes[2],
            commandBytes[3],
            commandBytes[4],
            commandBytes[5],
            commandBytes[6]};

        CANCommandValueCode **valueCodes;
        uint8_t valueCodeSize = 0;

        if (command.size() > CAN_COMMAND_INDEX_VALUE_CODE) {
            JsonObject valueCodeCommands = command[CAN_COMMAND_INDEX_VALUE_CODE].as<JsonObject>();
            valueCodeSize = valueCodeCommands.size();
            valueCodes = new CANCommandValueCode *[valueCodeSize];

            uint8_t valueCodeCounter = 0;

            for (JsonPair keyValue : valueCodeCommands) {
                valueCodes[valueCodeCounter] = new CANCommandValueCode(keyValue.key().c_str(), keyValue.value().as<std::string>().c_str());
                valueCodeCounter++;
            }
        } else {
            valueCodes = nullptr;
        }

        config->COMMANDS[i] = new CANCommand(
            command[CAN_COMMAND_INDEX_NAME],
            command[CAN_COMMAND_INDEX_LABEL],
            commandArray,
            command[CAN_COMMAND_INDEX_ID].as<const uint16_t>(),
            command[CAN_COMMAND_INDEX_DIVISOR].as<const float>(),
            command[CAN_COMMAND_INDEX_WRITABLE].as<const bool>(),
            command[CAN_COMMAND_INDEX_UNIT],
            command[CAN_COMMAND_INDEX_TYPE],
            valueCodeSize,
            valueCodes);
    }
}

void canBus_stop()
{
    if(driver != nullptr)
        delete driver;
}