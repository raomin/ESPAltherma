#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <IDebugStream.hpp>
#include "config.hpp"
#include "driver.hpp"
#include "Drivers/mcp2515.hpp"
#include "Drivers/elm327.hpp"
#include "Drivers/sja1000.hpp"

bool canBus_setup(const CAN_Config* CANConfig, IDebugStream* const debugStream);

void canBus_loop();

void canBus_set(const String label, const char *payload, const uint32_t length);

String canBus_readAllCommands();

void canBus_fill_config(JsonObject &jsonObject, CAN_Config *config);

void canBus_stop();