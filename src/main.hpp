#ifndef MAIN_H
#define MAIN_H

#include <PubSubClient.h>

#include "arduinoC.hpp"
#include "Config/config.hpp"
#include "debugSerial.hpp"
#include "X10A/converters.hpp"
#include "X10A/comm.hpp"
#include "X10A/x10a.hpp"
#include "MQTT/mqtt.hpp"
#include "WebUI/webui.hpp"
#include "wireless.hpp"
#include "persistence.hpp"
#include "CAN/canBus.hpp"
#include "arrayFunctions.hpp"
#include "restart.hpp"

#ifdef ARDUINO_M5Stick_C
long LCDTimeout = 40000; // Keep screen ON for 40s then turn off. ButtonA will turn it On again.
#endif

void IRAM_ATTR restartInStandaloneWifi();

void setup();

void waitLoop(ulong ms);

void loop();
#endif