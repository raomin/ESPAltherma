#ifndef CAN_BUS_H
#define CAN_BUS_H

#include "debugSerial.hpp"
#include "Config.hpp"
#include "Driver.hpp"
#include "CAN/Drivers/MCP2515.hpp"
#include "CAN/Drivers/ELM327.hpp"
#include "CAN/Drivers/SJA1000.hpp"

void canBus_setup(const CAN_Config* CANConfig);

void canBus_loop();

String canBus_readAllCommands();

void canBus_stop();

#endif