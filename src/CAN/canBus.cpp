#include "canBus.hpp"

static CANDriver *driver = nullptr;

bool canBus_setup(const CAN_Config* CANConfig)
{
    canBus_stop();

    debugSerial.println("Starting new CAN Driver...");

    bool result = false;

    switch (CANConfig->CAN_IC)
    {
    case CAN_ICTypes::MCP2515:
        driver = new DriverMCP2515(CANConfig);
        result = driver->initInterface();
        break;

    case CAN_ICTypes::ELM327:
        driver = new DriverELM327(CANConfig);
        result = driver->initInterface();
        break;

    #ifndef ARDUINO_ARCH_ESP8266
    case CAN_ICTypes::SJA1000:
        driver = new DriverSJA1000(CANConfig);
        result = driver->initInterface();
        break;
    #endif

    default:
        debugSerial.println("No CAN Driver found");
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

String canBus_readAllCommands()
{
    if(driver != nullptr)
        return driver->readAllCommands();

    return "";
}

void canBus_stop()
{
    if(driver != nullptr)
        delete driver;
}