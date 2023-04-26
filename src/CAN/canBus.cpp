#include "canBus.hpp"

static CANDriver *driver = nullptr;

void canBus_setup()
{
    switch (config->CAN_IC)
    {
    case CAN_ICTypes::MCP2515:
        driver = new DriverMCP2515();
        driver->initInterface();
        break;

    case CAN_ICTypes::ELM327:
        driver = new DriverELM327();
        driver->initInterface();
        break;

    case CAN_ICTypes::SJA1000:
        driver = new DriverSJA1000();
        driver->initInterface();
        break;

    default:
        debugSerial.println("No CAN Driver found");
        return;
    }

    driver->enableSniffing(config->CAN_SNIFFING_ENABLED);
}

void canBus_loop()
{
    if(driver != nullptr)
        driver->handleLoop();
}

void canBus_stop()
{
    if(driver != nullptr)
        delete driver;
}