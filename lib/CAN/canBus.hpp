#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <Arduino.h>
#include "debugStream.hpp"
#include "config.hpp"
#include "driver.hpp"
#include "Drivers/mcp2515.hpp"
#include "Drivers/elm327.hpp"
#include "Drivers/sja1000.hpp"

bool canBus_setup(const CAN_Config* CANConfig);

void canBus_loop();

void canBus_set(const String label, const char *payload, const uint32_t length);

String canBus_readAllCommands();

void canBus_stop();

#endif