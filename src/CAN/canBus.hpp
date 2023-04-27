#ifndef CAN_BUS_H
#define CAN_BUS_H

#include "debugSerial.hpp"
#include "Config/config.hpp"
#include "CANDriver.hpp"
#include "CAN/Drivers/MCP2515.hpp"
#include "CAN/Drivers/ELM327.hpp"
#include "CAN/Drivers/SJA1000.hpp"

void canBus_setup(const CAN_ICTypes &type, const CAN_ICBus &bus, const uint16_t &speed, const void* driverConfig);

void canBus_loop();

void canBus_stop();

#endif