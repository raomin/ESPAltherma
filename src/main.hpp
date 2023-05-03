#ifndef MAIN_H
#define MAIN_H

#include <PubSubClient.h>

#include "ArduinoC.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"
#include "X10A/converters.hpp"
#include "X10A/comm.hpp"
#include "MQTT/mqtt.hpp"
#include "WebUI/webui.hpp"
#include "wireless.hpp"
#include "persistence.hpp"
#include "X10A/X10A.hpp"
#include "CAN/canBus.hpp"
#include "arrayFunctions.hpp"
#include "restart.hpp"

#if defined(ARDUINO_M5Stick_C) || defined(ARDUINO_M5Stick_C_Plus)
long LCDTimeout = 40000; // Keep screen ON for 40s then turn off. ButtonA will turn it On again.
#endif

void IRAM_ATTR restartInStandaloneWifi();

void setup();

void waitLoop(ulong ms);

void loop();
#endif