#ifndef CAN_BUS_H
#define CAN_BUS_H

#include "debugSerial.hpp"
#include "config.hpp"
#include "driver.hpp"
#include "CAN/Drivers/mcp2515.hpp"
#include "CAN/Drivers/elm327.hpp"
#include "CAN/Drivers/sja1000.hpp"

bool canBus_setup(const CAN_Config* CANConfig);

void canBus_loop();

String canBus_readAllCommands();

void canBus_stop();

#endif